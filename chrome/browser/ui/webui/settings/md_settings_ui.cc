// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

#include <stddef.h>

#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"
#include "chrome/browser/ui/webui/settings/on_startup_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/safe_browsing_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"
#include "chrome/browser/ui/webui/settings/settings_import_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/browser/ui/webui/settings/site_settings_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/settings_resources.h"
#include "chrome/grit/settings_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/easy_unlock_settings_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/common/chrome_switches.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/settings/certificates_handler.h"
#elif defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/ui/webui/settings/native_certificates_handler.h"
#endif  // defined(USE_NSS_CERTS)

namespace settings {

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()) {
#if 0
  Profile* profile = Profile::FromWebUI(web_ui);
  AddSettingsPageUIHandler(base::MakeUnique<AppearanceHandler>(web_ui));

#if defined(USE_NSS_CERTS)
  AddSettingsPageUIHandler(base::MakeUnique<CertificatesHandler>(false));
#elif defined(OS_WIN) || defined(OS_MACOSX)
  AddSettingsPageUIHandler(base::MakeUnique<NativeCertificatesHandler>());
#endif  // defined(USE_NSS_CERTS)

  AddSettingsPageUIHandler(base::MakeUnique<BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<ClearBrowsingDataHandler>(web_ui));
  AddSettingsPageUIHandler(base::MakeUnique<CookiesViewHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<DownloadsHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<ExtensionControlHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<FontHandler>(web_ui));
  AddSettingsPageUIHandler(base::MakeUnique<ImportDataHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<LanguagesHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<MediaDevicesSelectionHandler>(profile));
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(base::MakeUnique<MetricsReportingHandler>());
#endif
  AddSettingsPageUIHandler(base::MakeUnique<OnStartupHandler>());
  AddSettingsPageUIHandler(base::MakeUnique<PeopleHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<SafeBrowsingHandler>(profile->GetPrefs()));
  AddSettingsPageUIHandler(base::MakeUnique<SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<SiteSettingsHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<StartupPagesHandler>(web_ui));

#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::AccessibilityHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::AndroidAppsHandler>(profile));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::ChangePictureHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::CupsPrintersHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::KeyboardHandler>(web_ui));
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::PointerHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::StorageHandler>());
  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::InternetHandler>());
#else
  AddSettingsPageUIHandler(base::MakeUnique<DefaultBrowserHandler>(web_ui));
  AddSettingsPageUIHandler(base::MakeUnique<ManageProfileHandler>(profile));
  AddSettingsPageUIHandler(base::MakeUnique<SystemHandler>());
#endif

  // Host must be derived from the visible URL, since this might be serving
  // either chrome://settings or chrome://md-settings.
  CHECK(url.GetOrigin() == GURL(chrome::kChromeUISettingsURL).GetOrigin() ||
        url.GetOrigin() == GURL(chrome::kChromeUIMdSettingsURL).GetOrigin());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(url.host());

#if defined(OS_CHROMEOS)
  chromeos::settings::EasyUnlockSettingsHandler* easy_unlock_handler =
      chromeos::settings::EasyUnlockSettingsHandler::Create(html_source,
                                                            profile);
  if (easy_unlock_handler)
    AddSettingsPageUIHandler(base::WrapUnique(easy_unlock_handler));

  AddSettingsPageUIHandler(base::WrapUnique(
      chromeos::settings::DateTimeHandler::Create(html_source)));

  AddSettingsPageUIHandler(
      base::MakeUnique<chromeos::settings::StylusHandler>());
  html_source->AddBoolean("pinUnlockEnabled",
                          chromeos::IsPinUnlockEnabled(profile->GetPrefs()));
  html_source->AddBoolean(
      "androidAppsAllowed",
      arc::ArcSessionManager::IsAllowedForProfile(profile) &&
          !arc::ArcSessionManager::IsOptInVerificationDisabled());

  // TODO(mash): Support Chrome power settings in Mash. crbug.com/644348
  bool enable_power_settings =
      !chrome::IsRunningInMash() &&
      (switches::PowerOverlayEnabled() ||
       (ash::PowerStatus::Get()->IsBatteryPresent() &&
        ash::PowerStatus::Get()->SupportsDualRoleDevices()));
  html_source->AddBoolean("enablePowerSettings", enable_power_settings);
  if (enable_power_settings) {
    AddSettingsPageUIHandler(
        base::MakeUnique<chromeos::settings::PowerHandler>());
  }
#endif

  AddSettingsPageUIHandler(
      base::WrapUnique(AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(
      base::WrapUnique(ResetSettingsHandler::Create(html_source, profile)));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(base::MakeUnique<MetricsHandler>());

  // Add all settings resources.
  for (size_t i = 0; i < kSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kSettingsResources[i].name,
                                 kSettingsResources[i].value);
  }

  AddLocalizedStrings(html_source, profile);
  html_source->SetDefaultResource(IDR_SETTINGS_SETTINGS_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
#endif
}

MdSettingsUI::~MdSettingsUI() {
}

void MdSettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<SettingsPageUIHandler> handler) {
  DCHECK(handler);
  handlers_.insert(handler.get());
  web_ui()->AddMessageHandler(std::move(handler));
}

void MdSettingsUI::DidStartProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page) {
  load_start_time_ = base::Time::Now();
}

void MdSettingsUI::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  UMA_HISTOGRAM_TIMES("Settings.LoadDocumentTime.MD",
                      base::Time::Now() - load_start_time_);
}

void MdSettingsUI::DocumentOnLoadCompletedInMainFrame() {
  UMA_HISTOGRAM_TIMES("Settings.LoadCompletedTime.MD",
                      base::Time::Now() - load_start_time_);
}

}  // namespace settings
