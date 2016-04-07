// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_context_keyed_service_factories.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/extensions/api/bluetooth_low_energy/bluetooth_low_energy_api.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_service.h"
#include "chrome/browser/extensions/api/cookies/cookies_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/dial/dial_api_factory.h"
#include "chrome/browser/extensions/api/easy_unlock_private/easy_unlock_private_api.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"
#include "chrome/browser/extensions/api/gcd_private/gcd_private_api.h"
#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/api/hotword_private/hotword_private_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/language_settings_private/language_settings_private_delegate_factory.h"
#include "chrome/browser/extensions/api/location/location_manager.h"
#include "chrome/browser/extensions/api/mdns/mdns_api.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/preference/chrome_direct_setting_api.h"
#include "chrome/browser/extensions/api/preference/preference_api.h"
#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/api/screenlock_private/screenlock_private_api.h"
#include "chrome/browser/extensions/api/search_engines_private/search_engines_private_event_router_factory.h"
#include "chrome/browser/extensions/api/sessions/sessions_api.h"
#include "chrome/browser/extensions/api/settings_overrides/settings_overrides_api.h"
#include "chrome/browser/extensions/api/settings_private/settings_private_event_router_factory.h"
#include "chrome/browser/extensions/api/signed_in_devices/signed_in_devices_manager.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_registry.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/api/webrtc_audio_private/webrtc_audio_private_api.h"
#include "chrome/browser/extensions/api/webstore/webstore_api.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/extensions/extension_storage_monitor_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/plugin_manager.h"
#include "chrome/browser/extensions/token_cache/token_cache_service_factory.h"
#include "chrome/browser/extensions/warning_badge_service_factory.h"
#include "chrome/browser/speech/extension_api/tts_extension_api.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model_factory.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api.h"
#include "chrome/browser/extensions/api/log_private/log_private_api.h"
#endif

#if defined(ENABLE_SPELLCHECK)
#include "chrome/browser/extensions/api/spellcheck/spellcheck_api.h"
#endif

#include "content/nw/src/api/object_manager_factory.h"

namespace chrome_extensions {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::ActivityLog::GetFactoryInstance();
  extensions::ActivityLogAPI::GetFactoryInstance();
  extensions::AutofillPrivateEventRouterFactory::GetInstance();
  extensions::BluetoothLowEnergyAPI::GetFactoryInstance();
  extensions::BookmarksAPI::GetFactoryInstance();
  extensions::BookmarkManagerPrivateAPI::GetFactoryInstance();
  extensions::BrailleDisplayPrivateAPI::GetFactoryInstance();
  extensions::chromedirectsetting::ChromeDirectSettingAPI::GetFactoryInstance();
  extensions::CommandService::GetFactoryInstance();
  extensions::ContentSettingsService::GetFactoryInstance();
  extensions::CookiesAPI::GetFactoryInstance();
  extensions::DeveloperPrivateAPI::GetFactoryInstance();
  extensions::DialAPIFactory::GetInstance();
  extensions::EasyUnlockPrivateAPI::GetFactoryInstance();
  extensions::ExtensionActionAPI::GetFactoryInstance();
  extensions::ExtensionGarbageCollectorFactory::GetInstance();
  extensions::ExtensionStorageMonitorFactory::GetInstance();
  extensions::ExtensionSystemFactory::GetInstance();
  extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance();
  extensions::FeedbackPrivateAPI::GetFactoryInstance();
  extensions::FontSettingsAPI::GetFactoryInstance();
  extensions::GcdPrivateAPI::GetFactoryInstance();
  extensions::HistoryAPI::GetFactoryInstance();
  extensions::HotwordPrivateEventService::GetFactoryInstance();
  extensions::IdentityAPI::GetFactoryInstance();
  extensions::InstallTrackerFactory::GetInstance();
  extensions::InstallVerifierFactory::GetInstance();
#if defined(OS_CHROMEOS)
  extensions::InputImeAPI::GetFactoryInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
#endif
  extensions::LanguageSettingsPrivateDelegateFactory::GetInstance();
  extensions::LocationManager::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  extensions::LogPrivateAPI::GetFactoryInstance();
#endif
  extensions::MDnsAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  extensions::MediaPlayerAPI::GetFactoryInstance();
#endif
  extensions::MenuManagerFactory::GetInstance();
  nw::ObjectManagerFactory::GetInstance();
  extensions::OmniboxAPI::GetFactoryInstance();
  extensions::PasswordsPrivateEventRouterFactory::GetInstance();
#if defined(ENABLE_PLUGINS)
  extensions::PluginManager::GetFactoryInstance();
#endif  // defined(ENABLE_PLUGINS)
  extensions::PreferenceAPI::GetFactoryInstance();
  extensions::ProcessesAPI::GetFactoryInstance();
  extensions::ScreenlockPrivateEventRouter::GetFactoryInstance();
  extensions::SearchEnginesPrivateEventRouterFactory::GetInstance();
  extensions::SessionsAPI::GetFactoryInstance();
  extensions::SettingsPrivateEventRouterFactory::GetInstance();
  extensions::SettingsOverridesAPI::GetFactoryInstance();
  extensions::SignedInDevicesManager::GetFactoryInstance();
#if defined(ENABLE_SPELLCHECK)
  extensions::SpellcheckAPI::GetFactoryInstance();
#endif
  extensions::StreamsPrivateAPI::GetFactoryInstance();
  extensions::TabCaptureRegistry::GetFactoryInstance();
  extensions::TabsWindowsAPI::GetFactoryInstance();
  extensions::TtsAPI::GetFactoryInstance();
  extensions::WarningBadgeServiceFactory::GetInstance();
  extensions::WebNavigationAPI::GetFactoryInstance();
  extensions::WebrtcAudioPrivateEventService::GetFactoryInstance();
  extensions::WebstoreAPI::GetFactoryInstance();
#if defined(OS_CHROMEOS)
  file_manager::EventRouterFactory::GetInstance();
#endif
  TokenCacheServiceFactory::GetInstance();
  ToolbarActionsModelFactory::GetInstance();
  extensions::ExtensionGCMAppHandler::GetFactoryInstance();
}

}  // namespace chrome_extensions
