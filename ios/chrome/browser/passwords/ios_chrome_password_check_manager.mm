// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"

#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/credential_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_bulk_leak_check_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::LeakCheckCredential;
using State = password_manager::BulkLeakCheckServiceInterface::State;

// Key used to attach UserData to a LeakCheckCredential.
constexpr char kPasswordCheckDataKey[] = "password-check-manager-data-key";
// Minimum time the check should be running.
constexpr base::TimeDelta kDelay = base::Seconds(3);

// Class which ensures that IOSChromePasswordCheckManager will stay alive
// until password check is completed even if class what initially created
// IOSChromePasswordCheckManager was destroyed.
class IOSChromePasswordCheckManagerHolder : public LeakCheckCredential::Data {
 public:
  explicit IOSChromePasswordCheckManagerHolder(
      scoped_refptr<IOSChromePasswordCheckManager> manager)
      : manager_(std::move(manager)) {}
  ~IOSChromePasswordCheckManagerHolder() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<IOSChromePasswordCheckManagerHolder>(manager_);
  }

 private:
  scoped_refptr<IOSChromePasswordCheckManager> manager_;
};

PasswordCheckState ConvertBulkCheckState(State state) {
  switch (state) {
    case State::kIdle:
      return PasswordCheckState::kIdle;
    case State::kRunning:
      return PasswordCheckState::kRunning;
    case State::kSignedOut:
      return PasswordCheckState::kSignedOut;
    case State::kNetworkError:
      return PasswordCheckState::kOffline;
    case State::kQuotaLimit:
      return PasswordCheckState::kQuotaLimit;
    case State::kCanceled:
      return PasswordCheckState::kCanceled;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return PasswordCheckState::kOther;
  }
  NOTREACHED();
  return PasswordCheckState::kIdle;
}

// Returns true if the Password Checkup feature flag is enabled.
bool IsPasswordCheckupEnabled() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kIOSPasswordCheckup);
}
}  // namespace

IOSChromePasswordCheckManager::IOSChromePasswordCheckManager(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      profile_store_(IOSChromePasswordStoreFactory::GetForBrowserState(
          browser_state,
          ServiceAccessType::EXPLICIT_ACCESS)),
      account_store_(IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
          browser_state,
          ServiceAccessType::EXPLICIT_ACCESS)),
      saved_passwords_presenter_(
          IOSChromeAffiliationServiceFactory::GetForBrowserState(browser_state),
          profile_store_,
          account_store_),
      insecure_credentials_manager_(&saved_passwords_presenter_,
                                    profile_store_,
                                    account_store_),
      bulk_leak_check_service_adapter_(
          &saved_passwords_presenter_,
          IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(
              browser_state),
          browser_state->GetPrefs()) {
  observed_saved_passwords_presenter_.Observe(&saved_passwords_presenter_);
  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);
  observed_bulk_leak_check_service_.Observe(
      IOSChromeBulkLeakCheckServiceFactory::GetForBrowserState(browser_state));

  // Instructs the presenter and manager to initialize and build their caches.
  saved_passwords_presenter_.Init();
}

IOSChromePasswordCheckManager::~IOSChromePasswordCheckManager() {
  DCHECK(observers_.empty());
}

void IOSChromePasswordCheckManager::StartPasswordCheck() {
  if (is_initialized_) {
    IOSChromePasswordCheckManagerHolder data(
        scoped_refptr<IOSChromePasswordCheckManager>(this));
    bulk_leak_check_service_adapter_.StartBulkLeakCheck(kPasswordCheckDataKey,
                                                        &data);
    is_check_running_ = true;
    start_time_ = base::Time::Now();
  } else {
    start_check_on_init_ = true;
  }
}

void IOSChromePasswordCheckManager::StopPasswordCheck() {
  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
  is_check_running_ = false;
}

PasswordCheckState IOSChromePasswordCheckManager::GetPasswordCheckState()
    const {
  if (saved_passwords_presenter_.GetSavedPasswords().empty()) {
    return PasswordCheckState::kNoPasswords;
  }
  return ConvertBulkCheckState(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

base::Time IOSChromePasswordCheckManager::GetLastPasswordCheckTime() const {
  return base::Time::FromDoubleT(browser_state_->GetPrefs()->GetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted));
}

std::vector<CredentialUIEntry>
IOSChromePasswordCheckManager::GetInsecureCredentials() const {
  std::vector<CredentialUIEntry> insecure_crendentials =
      insecure_credentials_manager_.GetInsecureCredentialEntries();

  // Only filter out the muted compromised credentials if the
  // kIOSPasswordCheckup flag is disabled and the kMuteCompromisedPasswords flag
  // is enabled. When kIOSPasswordCheckup is enabled, we want to get all the
  // insecure credentials, not only the compromised and unmuted ones.
  if (!IsPasswordCheckupEnabled() &&
      base::FeatureList::IsEnabled(
          password_manager::features::kMuteCompromisedPasswords)) {
    base::EraseIf(insecure_crendentials,
                  [](const auto& credential) { return credential.IsMuted(); });
  }
  return insecure_crendentials;
}

WarningType IOSChromePasswordCheckManager::GetWarningOfHighestPriority() const {
  std::vector<CredentialUIEntry> insecure_credentials =
      insecure_credentials_manager_.GetInsecureCredentialEntries();

  bool has_reused_passwords = false;
  bool has_weak_passwords = false;
  bool has_muted_warnings = false;

  for (const auto& credential : insecure_credentials) {
    if (credential.IsMuted()) {
      has_muted_warnings = true;
    } else if (credential.IsPhished() || credential.IsLeaked()) {
      return WarningType::kCompromisedPasswordsWarning;
    } else if (credential.IsReused()) {
      has_reused_passwords = true;
    } else if (credential.IsWeak()) {
      has_weak_passwords = true;
    }
  }

  if (has_reused_passwords) {
    return WarningType::kReusedPasswordsWarning;
  } else if (has_weak_passwords) {
    return WarningType::kWeakPasswordsWarning;
  } else if (has_muted_warnings) {
    return WarningType::kDismissedWarningsWarning;
  }

  return WarningType::kNoInsecurePasswordsWarning;
}

void IOSChromePasswordCheckManager::OnSavedPasswordsChanged() {
  // Observing saved passwords to update possible kNoPasswords state.
  NotifyPasswordCheckStatusChanged();
  if (!std::exchange(is_initialized_, true) && start_check_on_init_) {
    StartPasswordCheck();
  }
}

void IOSChromePasswordCheckManager::OnInsecureCredentialsChanged() {
  for (auto& observer : observers_) {
    observer.CompromisedCredentialsChanged();
  }
}

void IOSChromePasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Saving time of last successful password check
    browser_state_->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().ToDoubleT());
    browser_state_->GetPrefs()->SetTime(
        password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
        base::Time::Now());
  }
  if (state != State::kRunning) {
    // If check was running
    if (is_check_running_) {
      const base::TimeDelta elapsed = base::Time::Now() - start_time_;
      if (elapsed < kDelay) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&IOSChromePasswordCheckManager::
                               NotifyPasswordCheckStatusChanged,
                           weak_ptr_factory_.GetWeakPtr()),
            kDelay - elapsed);
        is_check_running_ = false;
        return;
      }
    }
    is_check_running_ = false;
  }
  NotifyPasswordCheckStatusChanged();
}

void IOSChromePasswordCheckManager::OnCredentialDone(
    const LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (is_leaked) {
    insecure_credentials_manager_.SaveInsecureCredential(credential);
  }
}

void IOSChromePasswordCheckManager::NotifyPasswordCheckStatusChanged() {
  for (auto& observer : observers_) {
    observer.PasswordCheckStatusChanged(GetPasswordCheckState());
  }
}
