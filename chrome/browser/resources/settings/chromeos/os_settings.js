// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The main entry point for the ChromeOS Settings SWA. This
 * imports all of the necessary modules and custom elements to load the page.
 */

import '../strings.m.js';
import '../prefs/prefs.js';
import './device_page/audio.js';
import './device_page/cros_audio_config.js';
import './device_page/device_page.js';
import './device_page/display.js';
import './device_page/display_layout.js';
import './device_page/display_overscan_dialog.js';
import './device_page/fake_input_device_data.js';
import './device_page/fake_input_device_settings_provider.js';
import './device_page/input_device_mojo_interface_provider.js';
import './device_page/input_device_settings_types.js';
import './device_page/keyboard.js';
import './device_page/per_device_keyboard.js';
import './device_page/per_device_mouse.js';
import './device_page/per_device_pointing_stick.js';
import './device_page/per_device_touchpad.js';
import './device_page/pointers.js';
import './device_page/power.js';
import './device_page/storage.js';
import './device_page/storage_external.js';
import './device_page/storage_external_entry.js';
import './device_page/stylus.js';
import './google_assistant_page/google_assistant_page.js';
import './internet_page/apn_subpage.js';
import './internet_page/cellular_roaming_toggle_button.js';
import './internet_page/cellular_setup_dialog.js';
import './internet_page/esim_remove_profile_dialog.js';
import './internet_page/hotspot_subpage.js';
import './internet_page/hotspot_summary_item.js';
import './internet_page/internet_config.js';
import './internet_page/internet_detail_page.js';
import './internet_page/internet_known_networks_page.js';
import './internet_page/internet_page.js';
import './internet_page/internet_subpage.js';
import './internet_page/network_always_on_vpn.js';
import './internet_page/network_proxy_section.js';
import './internet_page/network_summary.js';
import './internet_page/network_summary_item.js';
import './internet_page/settings_traffic_counters.js';
import './internet_page/tether_connection_dialog.js';
import './kerberos_page/kerberos_accounts.js';
import './kerberos_page/kerberos_page.js';
import './multidevice_page/multidevice_page.js';
import './nearby_share_page/nearby_share_receive_dialog.js';
import './nearby_share_page/nearby_share_subpage.js';
import './personalization_page/personalization_page.js';
import './os_a11y_page/change_dictation_locale_dialog.js';
import './os_a11y_page/os_a11y_page.js';
import './os_about_page/channel_switcher_dialog.js';
import './os_about_page/detailed_build_info.js';
import './os_about_page/os_about_page.js';
import './os_about_page/update_warning_dialog.js';
import './os_apps_page/android_apps_subpage.js';
import './os_apps_page/app_notifications_page/app_notifications_subpage.js';
import './os_apps_page/app_management_page/app_detail_view.js';
import './os_apps_page/app_management_page/app_details_item.js';
import './os_apps_page/app_management_page/app_item.js';
import './os_apps_page/app_management_page/app_management_page.js';
import './os_apps_page/app_management_page/arc_detail_view.js';
import './os_apps_page/app_management_page/borealis_page/borealis_detail_view.js';
import './os_apps_page/app_management_page/chrome_app_detail_view.js';
import './os_apps_page/app_management_page/dom_switch.js';
import 'chrome://resources/cr_components/app_management/icons.html.js';
import './os_apps_page/app_management_page/main_view.js';
import './os_apps_page/app_management_page/pin_to_shelf_item.js';
import './os_apps_page/app_management_page/plugin_vm_page/plugin_vm_detail_view.js';
import './os_apps_page/app_management_page/pwa_detail_view.js';
import './os_apps_page/app_management_page/app_management_cros_shared_style.css.js';
import './os_apps_page/app_management_page/app_management_cros_shared_vars.css.js';
import './os_apps_page/app_management_page/supported_links_overlapping_apps_dialog.js';
import './os_apps_page/app_management_page/supported_links_dialog.js';
import './os_apps_page/app_management_page/supported_links_item.js';
import 'chrome://resources/cr_components/app_management/toggle_row.js';
import 'chrome://resources/cr_components/app_management/uninstall_button.js';
import './os_apps_page/app_notifications_page/mojo_interface_provider.js';
import './os_apps_page/os_apps_page.js';
import './os_bluetooth_page/os_bluetooth_devices_subpage.js';
import './os_bluetooth_page/os_bluetooth_device_detail_subpage.js';
import './os_bluetooth_page/os_bluetooth_saved_devices_subpage.js';
import './os_bluetooth_page/os_remove_saved_device_dialog.js';
import './os_bluetooth_page/os_bluetooth_forget_device_dialog.js';
import './os_bluetooth_page/os_bluetooth_true_wireless_images.js';
import './os_bluetooth_page/os_bluetooth_change_device_name_dialog.js';
import './os_bluetooth_page/os_bluetooth_pairing_dialog.js';
import './os_bluetooth_page/os_bluetooth_page.js';
import './os_bluetooth_page/os_bluetooth_summary.js';
import './os_bluetooth_page/os_paired_bluetooth_list.js';
import './os_bluetooth_page/os_paired_bluetooth_list_item.js';
import './os_bluetooth_page/os_saved_devices_list.js';
import './os_bluetooth_page/os_saved_devices_list_item.js';
import './os_bluetooth_page/settings_fast_pair_constants.js';
import './os_settings_icons.html.js';
import './os_people_page/account_manager.js';
import './os_people_page/os_people_page.js';
import './os_people_page/os_sync_controls.js';
import './os_search_page/os_search_page.js';
import './os_settings_main/os_settings_main.js';
import './os_settings_page/os_settings_page.js';
import './os_settings_page/settings_idle_load.js';
import './os_settings_menu/os_settings_menu.js';
import './os_settings_ui/os_settings_ui.js';
import './os_settings_icons.css.js';
import './os_settings_search_box/os_search_result_row.js';
import './os_settings_search_box/os_settings_search_box.js';
import './os_toolbar/os_toolbar.js';
import './parental_controls_page/parental_controls_page.js';
import './settings_scheduler_slider/settings_scheduler_slider.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import * as crosAudioConfigMojomWebui from '../mojom-webui/audio/cros_audio_config.mojom-webui.js';
import * as appNotificationHandlerMojomWebui from '../mojom-webui/os_apps_page/app_notification_handler.mojom-webui.js';
import * as personalizationSearchMojomWebui from '../mojom-webui/personalization/search.mojom-webui.js';
import * as routesMojomWebui from '../mojom-webui/routes.mojom-webui.js';
import * as searchMojomWebui from '../mojom-webui/search/search.mojom-webui.js';
import * as searchResultIconMojomWebui from '../mojom-webui/search/search_result_icon.mojom-webui.js';
import * as userActionRecorderMojomWebui from '../mojom-webui/search/user_action_recorder.mojom-webui.js';
import * as settingMojomWebui from '../mojom-webui/setting.mojom-webui.js';

import * as fakeCrosAudioConfig from './device_page/fake_cros_audio_config.js';

/**
 * With the optimize_webui() build step, the generated JS files are bundled
 * into a single JS file. The exports below are necessary so they can be
 * imported into browser tests.
 */
export {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
export {BrowserProxy as AppManagementComponentBrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
export {PageType, WindowMode} from 'chrome://resources/cr_components/app_management/constants.js';
export {createBoolPermission, createTriStatePermission, getBoolPermissionValue, isBoolValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
export {convertOptionalBoolToBool, createEmptyState, createInitialState, getPermissionValueBool} from 'chrome://resources/cr_components/app_management/util.js';
export {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';
export {OpenWindowProxyImpl} from '../open_window_proxy.js';
export {ProfileInfoBrowserProxyImpl} from '../people_page/profile_info_browser_proxy.js';
export {PageStatus, StatusAction, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';
export {CrSettingsPrefs} from '../prefs/prefs_types.js';
export {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from '../privacy_page/privacy_page_browser_proxy.js';
export {getContactManager, observeContactManager, setContactManagerForTesting} from '../shared/nearby_contact_manager.js';
export {getNearbyShareSettings, observeNearbyShareSettings, setNearbyShareSettingsForTesting} from '../shared/nearby_share_settings.js';
export {NearbySettings, NearbyShareSettingsBehavior} from '../shared/nearby_share_settings_behavior.js';
export {setCrosAudioConfigForTesting} from './device_page/cros_audio_config.js';
export {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, NoteAppLockScreenSupport, setDisplayApiForTesting, StorageSpaceState} from './device_page/device_page_browser_proxy.js';
export {fakeKeyboards, fakeMice, fakePointingSticks, fakeTouchpads} from './device_page/fake_input_device_data.js';
export {FakeInputDeviceSettingsProvider} from './device_page/fake_input_device_settings_provider.js';
export {getInputDeviceSettingsProvider, setInputDeviceSettingsProviderForTesting, setupFakeInputDeviceSettingsProvider} from './device_page/input_device_mojo_interface_provider.js';
export {MetaKey} from './device_page/input_device_settings_types.js';
export {FindShortcutBehavior, FindShortcutManager} from './find_shortcut_behavior.js';
export {GoogleAssistantBrowserProxyImpl} from './google_assistant_page/google_assistant_browser_proxy.js';
export {ConsentStatus, DspHotwordState} from './google_assistant_page/google_assistant_page.js';
export {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page/internet_page_browser_proxy.js';
export {KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType} from './kerberos_page/kerberos_accounts_browser_proxy.js';
export {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from './metrics_recorder.js';
export {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_page/multidevice_browser_proxy.js';
export {MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubFeatureAccessStatus, PhoneHubPermissionsSetupMode, SmartLockSignInEnabledState} from './multidevice_page/multidevice_constants.js';
export {NotificationAccessSetupOperationStatus} from './multidevice_page/multidevice_notification_access_setup_dialog.js';
export {PermissionsSetupStatus, SetupFlowStatus} from './multidevice_page/multidevice_permissions_setup_dialog.js';
export {Account, NearbyAccountManagerBrowserProxy, NearbyAccountManagerBrowserProxyImpl} from './nearby_share_page/nearby_account_manager_browser_proxy.js';
export {getReceiveManager, observeReceiveManager, setReceiveManagerForTesting} from './nearby_share_page/nearby_share_receive_manager.js';
export {dataUsageStringToEnum, NearbyShareDataUsage} from './nearby_share_page/types.js';
export {ManageA11yPageBrowserProxy, ManageA11yPageBrowserProxyImpl} from './os_a11y_page/manage_a11y_page_browser_proxy.js';
export {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page/os_a11y_page_browser_proxy.js';
export {SelectToSpeakSubpageBrowserProxy, SelectToSpeakSubpageBrowserProxyImpl} from './os_a11y_page/select_to_speak_subpage_browser_proxy.js';
export {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './os_a11y_page/switch_access_subpage_browser_proxy.js';
export {TextToSpeechPageBrowserProxy, TextToSpeechPageBrowserProxyImpl} from './os_a11y_page/text_to_speech_page_browser_proxy.js';
export {TtsSubpageBrowserProxy, TtsSubpageBrowserProxyImpl} from './os_a11y_page/tts_subpage_browser_proxy.js';
export {AboutPageBrowserProxyImpl, BrowserChannel, UpdateStatus} from './os_about_page/about_page_browser_proxy.js';
export {DeviceNameBrowserProxyImpl} from './os_about_page/device_name_browser_proxy.js';
export {DeviceNameState, SetDeviceNameResult} from './os_about_page/device_name_util.js';
export {AndroidAppsBrowserProxyImpl} from './os_apps_page/android_apps_browser_proxy.js';
export {addApp, changeApp, removeApp, updateSelectedAppId} from './os_apps_page/app_management_page/actions.js';
export {AppManagementBrowserProxy} from './os_apps_page/app_management_page/browser_proxy.js';
export {FakePageHandler} from './os_apps_page/app_management_page/fake_page_handler.js';
export {PluginVmBrowserProxyImpl} from './os_apps_page/app_management_page/plugin_vm_page/plugin_vm_browser_proxy.js';
export {reduceAction, updateApps} from './os_apps_page/app_management_page/reducers.js';
export {AppManagementStore} from './os_apps_page/app_management_page/store.js';
export {AppManagementStoreMixin} from './os_apps_page/app_management_page/store_mixin.js';
export {setAppNotificationProviderForTesting} from './os_apps_page/app_notifications_page/mojo_interface_provider.js';
export {OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_page/os_bluetooth_devices_subpage_browser_proxy.js';
export {FastPairSavedDevicesOptInStatus} from './os_bluetooth_page/settings_fast_pair_constants.js';
export {osPageVisibility} from './os_page_visibility.js';
export {AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from './os_people_page/account_manager_browser_proxy.js';
export {FingerprintBrowserProxyImpl, FingerprintResultType} from './os_people_page/fingerprint_browser_proxy.js';
export {OsSyncBrowserProxyImpl} from './os_people_page/os_sync_browser_proxy.js';
export {FingerprintSetupStep} from './os_people_page/setup_fingerprint_dialog.js';
export {MetricsConsentBrowserProxy, MetricsConsentBrowserProxyImpl, MetricsConsentState} from './os_privacy_page/metrics_consent_browser_proxy.js';
export {DataAccessPolicyState, PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './os_privacy_page/peripheral_data_access_browser_proxy.js';
export {OsResetBrowserProxyImpl} from './os_reset_page/os_reset_browser_proxy.js';
export {routes} from './os_route.js';
export {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './os_search_page/search_engines_browser_proxy.js';
export {OsSettingsSearchBoxBrowserProxyImpl} from './os_settings_search_box/os_settings_search_box_browser_proxy.js';
export {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_page/parental_controls_browser_proxy.js';
export {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_page/personalization_hub_browser_proxy.js';
export {Route, Router} from './router.js';
export {getPersonalizationSearchHandler, setPersonalizationSearchHandlerForTesting} from './search/personalization_search_handler.js';
export {getSettingsSearchHandler, setSettingsSearchHandlerForTesting} from './search/settings_search_handler.js';
export {
  appNotificationHandlerMojomWebui,
  crosAudioConfigMojomWebui,
  fakeCrosAudioConfig,
  personalizationSearchMojomWebui,
  routesMojomWebui,
  searchMojomWebui,
  searchResultIconMojomWebui,
  settingMojomWebui,
  userActionRecorderMojomWebui,
};

// TODO(b/257329722) After the Jelly experiment is launched, add the CSS link
// element directly to the HTML.
const jellyEnabled = loadTimeData.getBoolean('isJellyEnabled');
if (jellyEnabled) {
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://theme/colors.css?sets=legacy,sys';
  document.head.appendChild(link);
  document.body.classList.add('jelly-enabled');
}

window.onload = () => {
  if (jellyEnabled) {
    startColorChangeUpdater();
  }
};
