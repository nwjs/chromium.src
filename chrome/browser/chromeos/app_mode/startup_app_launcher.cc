// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/startup_app_launcher.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/app_session_lifetime.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/updater/manifest_fetch_data.h"
#include "chrome/browser/extensions/updater/safe_manifest_parser.h"
#include "chrome/browser/extensions/webstore_startup_installer.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest_handlers/kiosk_mode_info.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::WebstoreStartupInstaller;

namespace chromeos {

namespace {

const char kOAuthRefreshToken[] = "refresh_token";
const char kOAuthClientId[] = "client_id";
const char kOAuthClientSecret[] = "client_secret";

const base::FilePath::CharType kOAuthFileName[] =
    FILE_PATH_LITERAL("kiosk_auth");

}  // namespace

class StartupAppLauncher::AppUpdateChecker
    : public base::SupportsWeakPtr<AppUpdateChecker>,
      public net::URLFetcherDelegate {
 public:
  explicit AppUpdateChecker(StartupAppLauncher* launcher)
      : launcher_(launcher),
        profile_(launcher->profile_),
        app_id_(launcher->app_id_) {}
  virtual ~AppUpdateChecker() {}

  void Start() {
    const Extension* app = GetInstalledApp();
    if (!app) {
      launcher_->OnUpdateCheckNotInstalled();
      return;
    }

    GURL update_url = extensions::ManifestURL::GetUpdateURL(app);
    if (update_url.is_empty())
      update_url = extension_urls::GetWebstoreUpdateUrl();
    if (!update_url.is_valid()) {
      launcher_->OnUpdateCheckNoUpdate();
      return;
    }

    manifest_fetch_data_.reset(
        new extensions::ManifestFetchData(update_url, 0));
    manifest_fetch_data_->AddExtension(
        app_id_, app->version()->GetString(), NULL, "", "");

    manifest_fetcher_.reset(net::URLFetcher::Create(
        manifest_fetch_data_->full_url(), net::URLFetcher::GET, this));
    manifest_fetcher_->SetRequestContext(profile_->GetRequestContext());
    manifest_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                                    net::LOAD_DO_NOT_SAVE_COOKIES |
                                    net::LOAD_DISABLE_CACHE);
    manifest_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
    manifest_fetcher_->Start();
  }

 private:
  const Extension* GetInstalledApp() {
    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    return extension_service->GetInstalledExtension(app_id_);
  }

  void HandleManifestResults(const extensions::ManifestFetchData& fetch_data,
                             const UpdateManifest::Results* results) {
    if (!results || results->list.empty()) {
      launcher_->OnUpdateCheckNoUpdate();
      return;
    }

    DCHECK_EQ(1u, results->list.size());

    const UpdateManifest::Result& update = results->list[0];

    if (update.browser_min_version.length() > 0) {
      Version browser_version;
      chrome::VersionInfo version_info;
      if (version_info.is_valid())
        browser_version = Version(version_info.Version());

      Version browser_min_version(update.browser_min_version);
      if (browser_version.IsValid() &&
          browser_min_version.IsValid() &&
          browser_min_version.CompareTo(browser_version) > 0) {
        launcher_->OnUpdateCheckNoUpdate();
        return;
      }
    }

    const Version& existing_version = *GetInstalledApp()->version();
    Version update_version(update.version);
    if (existing_version.IsValid() &&
        update_version.IsValid() &&
        update_version.CompareTo(existing_version) <= 0) {
      launcher_->OnUpdateCheckNoUpdate();
      return;
    }

    launcher_->OnUpdateCheckUpdateAvailable();
  }

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    DCHECK_EQ(source, manifest_fetcher_.get());

    if (source->GetStatus().status() != net::URLRequestStatus::SUCCESS ||
        source->GetResponseCode() != 200) {
      launcher_->OnUpdateCheckNoUpdate();
      return;
    }

    std::string data;
    source->GetResponseAsString(&data);
    scoped_refptr<extensions::SafeManifestParser> safe_parser(
        new extensions::SafeManifestParser(
            data,
            manifest_fetch_data_.release(),
            base::Bind(&AppUpdateChecker::HandleManifestResults,
                       AsWeakPtr())));
    safe_parser->Start();
  }

  StartupAppLauncher* launcher_;
  Profile* profile_;
  const std::string app_id_;

  scoped_ptr<extensions::ManifestFetchData> manifest_fetch_data_;
  scoped_ptr<net::URLFetcher> manifest_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(AppUpdateChecker);
};

StartupAppLauncher::StartupAppLauncher(Profile* profile,
                                       const std::string& app_id)
    : profile_(profile),
      app_id_(app_id),
      ready_to_launch_(false) {
  DCHECK(profile_);
  DCHECK(Extension::IdIsValid(app_id_));
}

StartupAppLauncher::~StartupAppLauncher() {
  // StartupAppLauncher can be deleted at anytime during the launch process
  // through a user bailout shortcut.
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void StartupAppLauncher::Initialize() {
  DVLOG(1) << "Starting... connection = "
           <<  net::NetworkChangeNotifier::GetConnectionType();
  StartLoadingOAuthFile();
}

void StartupAppLauncher::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void StartupAppLauncher::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void StartupAppLauncher::StartLoadingOAuthFile() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnLoadingOAuthFile());

  KioskOAuthParams* auth_params = new KioskOAuthParams();
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&StartupAppLauncher::LoadOAuthFileOnBlockingPool,
                 auth_params),
      base::Bind(&StartupAppLauncher::OnOAuthFileLoaded,
                 AsWeakPtr(),
                 base::Owned(auth_params)));
}

// static.
void StartupAppLauncher::LoadOAuthFileOnBlockingPool(
    KioskOAuthParams* auth_params) {
  int error_code = JSONFileValueSerializer::JSON_NO_ERROR;
  std::string error_msg;
  base::FilePath user_data_dir;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  base::FilePath auth_file = user_data_dir.Append(kOAuthFileName);
  scoped_ptr<JSONFileValueSerializer> serializer(
      new JSONFileValueSerializer(user_data_dir.Append(kOAuthFileName)));
  scoped_ptr<base::Value> value(
      serializer->Deserialize(&error_code, &error_msg));
  base::DictionaryValue* dict = NULL;
  if (error_code != JSONFileValueSerializer::JSON_NO_ERROR ||
      !value.get() || !value->GetAsDictionary(&dict)) {
    LOG(WARNING) << "Can't find auth file at " << auth_file.value();
    return;
  }

  dict->GetString(kOAuthRefreshToken, &auth_params->refresh_token);
  dict->GetString(kOAuthClientId, &auth_params->client_id);
  dict->GetString(kOAuthClientSecret, &auth_params->client_secret);
}

void StartupAppLauncher::OnOAuthFileLoaded(KioskOAuthParams* auth_params) {
  auth_params_ = *auth_params;
  // Override chrome client_id and secret that will be used for identity
  // API token minting.
  if (!auth_params_.client_id.empty() && !auth_params_.client_secret.empty()) {
    UserManager::Get()->SetAppModeChromeClientOAuthInfo(
            auth_params_.client_id,
            auth_params_.client_secret);
  }

  // If we are restarting chrome (i.e. on crash), we need to initialize
  // TokenService as well.
  InitializeTokenService();
}

void StartupAppLauncher::InitializeNetwork() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnInitializingNetwork());

  // TODO(tengs): Use NetworkStateInformer instead because it can handle
  // portal and proxy detection. We will need to do some refactoring to
  // make NetworkStateInformer more independent from the WebUI handlers.
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  OnNetworkChanged(net::NetworkChangeNotifier::GetConnectionType());
}

void StartupAppLauncher::InitializeTokenService() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnInitializingTokenService());

  ProfileOAuth2TokenService* profile_token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  if (profile_token_service->RefreshTokenIsAvailable(
          profile_token_service->GetPrimaryAccountId())) {
    InitializeNetwork();
    return;
  }

  // At the end of this method, the execution will be put on hold until
  // ProfileOAuth2TokenService triggers either OnRefreshTokenAvailable or
  // OnRefreshTokensLoaded. Given that we want to handle exactly one event,
  // whichever comes first, both handlers call RemoveObserver on PO2TS. Handling
  // any of the two events is the only way to resume the execution and enable
  // Cleanup method to be called, self-invoking a destructor. In destructor
  // StartupAppLauncher is no longer an observer of PO2TS and there is no need
  // to call RemoveObserver again.
  profile_token_service->AddObserver(this);

  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  token_service->Initialize(GaiaConstants::kChromeSource, profile_);

  // Pass oauth2 refresh token from the auth file.
  // TODO(zelidrag): We should probably remove this option after M27.
  // TODO(fgorski): This can go when we have persistence implemented on PO2TS.
  // Unless the code is no longer needed.
  if (!auth_params_.refresh_token.empty()) {
    token_service->UpdateCredentialsWithOAuth2(
        GaiaAuthConsumer::ClientOAuthResult(
            auth_params_.refresh_token,
            std::string(),  // access_token
            0));            // new_expires_in_secs
  } else {
    // Load whatever tokens we have stored there last time around.
    token_service->LoadTokensFromDB();
  }
}

void StartupAppLauncher::OnRefreshTokenAvailable(
    const std::string& account_id) {
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  InitializeNetwork();
}

void StartupAppLauncher::OnRefreshTokensLoaded() {
  ProfileOAuth2TokenServiceFactory::GetForProfile(profile_)
      ->RemoveObserver(this);
  InitializeNetwork();
}

void StartupAppLauncher::OnLaunchSuccess() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnLaunchSucceeded());
}

void StartupAppLauncher::OnLaunchFailure(KioskAppLaunchError::Error error) {
  LOG(ERROR) << "App launch failed, error: " << error;
  DCHECK_NE(KioskAppLaunchError::NONE, error);

  FOR_EACH_OBSERVER(Observer, observer_list_, OnLaunchFailed(error));
}

void StartupAppLauncher::LaunchApp() {
  if (!ready_to_launch_) {
    NOTREACHED();
    LOG(ERROR) << "LaunchApp() called but launcher is not initialized.";
  }

  const Extension* extension = extensions::ExtensionSystem::Get(profile_)->
      extension_service()->GetInstalledExtension(app_id_);
  CHECK(extension);

  if (!extensions::KioskModeInfo::IsKioskEnabled(extension)) {
    OnLaunchFailure(KioskAppLaunchError::NOT_KIOSK_ENABLED);
    return;
  }

  // Always open the app in a window.
  OpenApplication(AppLaunchParams(profile_, extension,
                                  extension_misc::LAUNCH_WINDOW, NEW_WINDOW));
  InitAppSession(profile_, app_id_);

  UserManager::Get()->SessionStarted();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_KIOSK_APP_LAUNCHED,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());

  OnLaunchSuccess();
}

void StartupAppLauncher::MaybeInstall() {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnInstallingApp());

  update_checker_.reset(new AppUpdateChecker(this));
  update_checker_->Start();
}

void StartupAppLauncher::OnUpdateCheckNotInstalled() {
  BeginInstall();
}

void StartupAppLauncher::OnUpdateCheckUpdateAvailable() {
  // Uninstall to force a re-install.
  // TODO(xiyuan): Find a better way. Either download CRX and install it
  // directly or integrate with ExtensionUpdater in someway.
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extension_service->UninstallExtension(app_id_, false, NULL);

  OnUpdateCheckNotInstalled();
}

void StartupAppLauncher::OnUpdateCheckNoUpdate() {
  OnReadyToLaunch();
}

void StartupAppLauncher::BeginInstall() {
  installer_ = new WebstoreStartupInstaller(
      app_id_,
      profile_,
      false,
      base::Bind(&StartupAppLauncher::InstallCallback, AsWeakPtr()));
  installer_->BeginInstall();
}

void StartupAppLauncher::InstallCallback(bool success,
                                         const std::string& error) {
  installer_ = NULL;
  if (success) {
    // Finish initialization after the callback returns.
    // So that the app finishes its installation.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&StartupAppLauncher::OnReadyToLaunch,
                   AsWeakPtr()));

    // Schedule app data update after installation.
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&StartupAppLauncher::UpdateAppData,
                   AsWeakPtr()));
    return;
  }

  LOG(ERROR) << "App install failed: " << error;
  OnLaunchFailure(KioskAppLaunchError::UNABLE_TO_INSTALL);
}

void StartupAppLauncher::OnReadyToLaunch() {
  ready_to_launch_ = true;
  FOR_EACH_OBSERVER(Observer, observer_list_, OnReadyToLaunch());
}

void StartupAppLauncher::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  DVLOG(1) << "OnNetworkChanged... connection = "
           <<  net::NetworkChangeNotifier::GetConnectionType();
  if (!net::NetworkChangeNotifier::IsOffline()) {
    DVLOG(1) << "Network up and running!";
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);

    MaybeInstall();
  } else {
    DVLOG(1) << "Network not running yet!";
  }
}

void StartupAppLauncher::UpdateAppData() {
  KioskAppManager::Get()->ClearAppData(app_id_);
  KioskAppManager::Get()->UpdateAppDataFromProfile(app_id_, profile_, NULL);
}

}   // namespace chromeos
