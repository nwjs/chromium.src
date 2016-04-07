// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_tab_util.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/url_constants.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

using content::NavigationEntry;
using content::WebContents;

namespace extensions {

namespace {

namespace keys = tabs_constants;

WindowController* GetAppWindowController(const WebContents* contents) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile);
  if (!registry)
    return NULL;
  AppWindow* app_window = registry->GetAppWindowForWebContents(contents);
  if (!app_window)
    return NULL;
  return WindowControllerList::GetInstance()->FindWindowById(
      app_window->session_id().id());
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the window cannot be found by id.
Browser* GetBrowserInProfileWithId(Profile* profile,
                                   const int window_id,
                                   bool include_incognito,
                                   std::string* error_message) {
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile()
          ? profile->GetOffTheRecordProfile()
          : NULL;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* browser = *it;
    if ((browser->profile() == profile ||
         browser->profile() == incognito_profile) &&
        ExtensionTabUtil::GetWindowId(browser) == window_id &&
        browser->window()) {
      return browser;
    }
  }

  if (error_message)
    *error_message = ErrorUtils::FormatErrorMessage(
        keys::kWindowNotFoundError, base::IntToString(window_id));

  return NULL;
}

Browser* CreateBrowser(ChromeUIThreadExtensionFunction* function,
                       int window_id,
                       std::string* error) {
  content::WebContents* web_contents = function->GetAssociatedWebContents();
  chrome::HostDesktopType desktop_type =
      web_contents && web_contents->GetNativeView()
          ? chrome::GetHostDesktopTypeForNativeView(
                web_contents->GetNativeView())
          : chrome::GetHostDesktopTypeForNativeView(NULL);
  Browser::CreateParams params(
      Browser::TYPE_TABBED, function->GetProfile(), desktop_type);
  Browser* browser = new Browser(params);
  browser->window()->Show();
  return browser;
}

// Use this function for reporting a tab id to an extension. It will
// take care of setting the id to TAB_ID_NONE if necessary (for
// example with devtools).
int GetTabIdForExtensions(const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && !ExtensionTabUtil::BrowserSupportsTabs(browser))
    return -1;
  return SessionTabHelper::IdForTab(web_contents);
}

}  // namespace

ExtensionTabUtil::OpenTabParams::OpenTabParams()
    : create_browser_if_needed(false) {
}

ExtensionTabUtil::OpenTabParams::~OpenTabParams() {
}

// Opens a new tab for a given extension. Returns NULL and sets |error| if an
// error occurs.
base::DictionaryValue* ExtensionTabUtil::OpenTab(
    ChromeUIThreadExtensionFunction* function,
    const OpenTabParams& params,
    std::string* error) {
  // windowId defaults to "current" window.
  int window_id = extension_misc::kCurrentWindowId;
  if (params.window_id.get())
    window_id = *params.window_id;

  Browser* browser = GetBrowserFromWindowID(function, window_id, error);
  if (!browser) {
    if (!params.create_browser_if_needed) {
      return NULL;
    }
    browser = CreateBrowser(function, window_id, error);
    if (!browser)
      return NULL;
  }

  // Ensure the selected browser is tabbed.
  if (!browser->is_type_tabbed() && browser->IsAttemptingToCloseBrowser())
    browser = chrome::FindTabbedBrowser(function->GetProfile(),
                                        function->include_incognito(),
                                        browser->host_desktop_type());

  if (!browser || !browser->window()) {
    if (error)
      *error = keys::kNoCurrentWindowError;
    return NULL;
  }

  // TODO(jstritar): Add a constant, chrome.tabs.TAB_ID_ACTIVE, that
  // represents the active tab.
  WebContents* opener = NULL;
  if (params.opener_tab_id.get()) {
    int opener_id = *params.opener_tab_id;

    if (!ExtensionTabUtil::GetTabById(opener_id,
                                      function->GetProfile(),
                                      function->include_incognito(),
                                      NULL,
                                      NULL,
                                      &opener,
                                      NULL)) {
      if (error) {
        *error = ErrorUtils::FormatErrorMessage(keys::kTabNotFoundError,
                                                base::IntToString(opener_id));
      }
      return NULL;
    }
  }

  // TODO(rafaelw): handle setting remaining tab properties:
  // -title
  // -favIconUrl

  GURL url;
  if (params.url.get()) {
    std::string url_string = *params.url;
    url = ExtensionTabUtil::ResolvePossiblyRelativeURL(url_string,
                                                       function->extension());
    if (!url.is_valid()) {
      *error =
          ErrorUtils::FormatErrorMessage(keys::kInvalidUrlError, url_string);
      return NULL;
    }
  } else {
    url = GURL(chrome::kChromeUINewTabURL);
  }

  // Don't let extensions crash the browser or renderers.
  if (ExtensionTabUtil::IsKillURL(url)) {
    *error = keys::kNoCrashBrowserError;
    return NULL;
  }

  // Default to foreground for the new tab. The presence of 'active' property
  // will override this default.
  bool active = true;
  if (params.active.get())
    active = *params.active;

  // Default to not pinning the tab. Setting the 'pinned' property to true
  // will override this default.
  bool pinned = false;
  if (params.pinned.get())
    pinned = *params.pinned;

  // We can't load extension URLs into incognito windows unless the extension
  // uses split mode. Special case to fall back to a tabbed window.
  if (url.SchemeIs(kExtensionScheme) &&
      !IncognitoInfo::IsSplitMode(function->extension()) &&
      browser->profile()->IsOffTheRecord()) {
    Profile* profile = browser->profile()->GetOriginalProfile();
    chrome::HostDesktopType desktop_type = browser->host_desktop_type();

    browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (!browser) {
      browser = new Browser(
          Browser::CreateParams(Browser::TYPE_TABBED, profile, desktop_type));
      browser->window()->Show();
    }
  }

  // If index is specified, honor the value, but keep it bound to
  // -1 <= index <= tab_strip->count() where -1 invokes the default behavior.
  int index = -1;
  if (params.index.get())
    index = *params.index;

  TabStripModel* tab_strip = browser->tab_strip_model();

  index = std::min(std::max(index, -1), tab_strip->count());

  int add_types = active ? TabStripModel::ADD_ACTIVE : TabStripModel::ADD_NONE;
  add_types |= TabStripModel::ADD_FORCE_INDEX;
  if (pinned)
    add_types |= TabStripModel::ADD_PINNED;
  chrome::NavigateParams navigate_params(
      browser, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.disposition =
      active ? NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB;
  navigate_params.tabstrip_index = index;
  navigate_params.tabstrip_add_types = add_types;
  chrome::Navigate(&navigate_params);

  // The tab may have been created in a different window, so make sure we look
  // at the right tab strip.
  tab_strip = navigate_params.browser->tab_strip_model();
  int new_index =
      tab_strip->GetIndexOfWebContents(navigate_params.target_contents);
  if (opener)
    tab_strip->SetOpenerOfWebContentsAt(new_index, opener);

  if (active)
    navigate_params.target_contents->SetInitialFocus();

  // Return data about the newly created tab.
  return ExtensionTabUtil::CreateTabValue(navigate_params.target_contents,
                                          tab_strip,
                                          new_index,
                                          function->extension());
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    ChromeUIThreadExtensionFunction* function,
    int window_id,
    std::string* error) {
  if (window_id == extension_misc::kCurrentWindowId) {
    Browser* result = function->GetCurrentBrowser();
    if (!result || !result->window()) {
      if (error)
        *error = keys::kNoCurrentWindowError;
      return NULL;
    }
    return result;
  } else {
    return GetBrowserInProfileWithId(function->GetProfile(),
                                     window_id,
                                     function->include_incognito(),
                                     error);
  }
}

Browser* ExtensionTabUtil::GetBrowserFromWindowID(
    const ChromeExtensionFunctionDetails& details,
    int window_id,
    std::string* error) {
  if (window_id == extension_misc::kCurrentWindowId) {
    Browser* result = details.GetCurrentBrowser();
    if (!result || !result->window()) {
      if (error)
        *error = keys::kNoCurrentWindowError;
      return NULL;
    }
    return result;
  } else {
    return GetBrowserInProfileWithId(details.GetProfile(),
                                     window_id,
                                     details.function()->include_incognito(),
                                     error);
  }
}

int ExtensionTabUtil::GetWindowId(const Browser* browser) {
  return browser->session_id().id();
}

int ExtensionTabUtil::GetWindowIdOfTabStripModel(
    const TabStripModel* tab_strip_model) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (it->tab_strip_model() == tab_strip_model)
      return GetWindowId(*it);
  }
  return -1;
}

int ExtensionTabUtil::GetTabId(const WebContents* web_contents) {
  return SessionTabHelper::IdForTab(web_contents);
}

std::string ExtensionTabUtil::GetTabStatusText(bool is_loading) {
  return is_loading ? keys::kStatusValueLoading : keys::kStatusValueComplete;
}

int ExtensionTabUtil::GetWindowIdOfTab(const WebContents* web_contents) {
  return SessionTabHelper::IdForWindowContainingTab(web_contents);
}

base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index,
    const Extension* extension) {
  // If we have a matching AppWindow with a controller, get the tab value
  // from its controller instead.
  WindowController* controller = GetAppWindowController(contents);
  if (controller &&
      (!extension || controller->IsVisibleToExtension(extension))) {
    return controller->CreateTabValue(extension, tab_index);
  }
  base::DictionaryValue* result =
      CreateTabValue(contents, tab_strip, tab_index);
  ScrubTabValueForExtension(contents, extension, result);
  return result;
}

base::ListValue* ExtensionTabUtil::CreateTabList(
    const Browser* browser,
    const Extension* extension) {
  base::ListValue* tab_list = new base::ListValue();
  TabStripModel* tab_strip = browser->tab_strip_model();
  for (int i = 0; i < tab_strip->count(); ++i) {
    tab_list->Append(CreateTabValue(tab_strip->GetWebContentsAt(i),
                                    tab_strip,
                                    i,
                                    extension));
  }

  return tab_list;
}

base::DictionaryValue* ExtensionTabUtil::CreateTabValue(
    WebContents* contents,
    TabStripModel* tab_strip,
    int tab_index) {
  // If we have a matching AppWindow with a controller, get the tab value
  // from its controller instead.
  WindowController* controller = GetAppWindowController(contents);
  if (controller)
    return controller->CreateTabValue(NULL, tab_index);

  if (!tab_strip)
    ExtensionTabUtil::GetTabStripModel(contents, &tab_strip, &tab_index);

  base::DictionaryValue* result = new base::DictionaryValue();
  bool is_loading = contents->IsLoading();
  result->SetInteger(keys::kIdKey, GetTabIdForExtensions(contents));
  result->SetInteger(keys::kIndexKey, tab_index);
  result->SetInteger(keys::kWindowIdKey, GetWindowIdOfTab(contents));
  result->SetString(keys::kStatusKey, GetTabStatusText(is_loading));
  result->SetBoolean(keys::kActiveKey,
                     tab_strip && tab_index == tab_strip->active_index());
  result->SetBoolean(keys::kSelectedKey,
                     tab_strip && tab_index == tab_strip->active_index());
  result->SetBoolean(keys::kHighlightedKey,
                   tab_strip && tab_strip->IsTabSelected(tab_index));
  result->SetBoolean(keys::kPinnedKey,
                     tab_strip && tab_strip->IsTabPinned(tab_index));
  result->SetBoolean(keys::kAudibleKey, contents->WasRecentlyAudible());
  result->Set(keys::kMutedInfoKey, CreateMutedInfo(contents));
  result->SetBoolean(keys::kIncognitoKey,
                     contents->GetBrowserContext()->IsOffTheRecord());
  result->SetInteger(keys::kWidthKey,
                     contents->GetContainerBounds().size().width());
  result->SetInteger(keys::kHeightKey,
                     contents->GetContainerBounds().size().height());

  // Privacy-sensitive fields: these should be stripped off by
  // ScrubTabValueForExtension if the extension should not see them.
  result->SetString(keys::kUrlKey, contents->GetURL().spec());
  result->SetString(keys::kTitleKey, contents->GetTitle());
  if (!is_loading) {
    NavigationEntry* entry = contents->GetController().GetVisibleEntry();
    if (entry && entry->GetFavicon().valid)
      result->SetString(keys::kFaviconUrlKey, entry->GetFavicon().url.spec());
  }

  if (tab_strip) {
    WebContents* opener = tab_strip->GetOpenerOfWebContentsAt(tab_index);
    if (opener)
      result->SetInteger(keys::kOpenerTabIdKey, GetTabIdForExtensions(opener));
  }

  return result;
}

// static
scoped_ptr<base::DictionaryValue> ExtensionTabUtil::CreateMutedInfo(
    content::WebContents* contents) {
  DCHECK(contents);
  api::tabs::MutedInfo info;
  info.muted = contents->IsAudioMuted();
  switch (chrome::GetTabAudioMutedReason(contents)) {
    case TAB_MUTED_REASON_NONE:
      break;
    case TAB_MUTED_REASON_CONTEXT_MENU:
    case TAB_MUTED_REASON_AUDIO_INDICATOR:
      info.reason = api::tabs::MUTED_INFO_REASON_USER;
      break;
    case TAB_MUTED_REASON_MEDIA_CAPTURE:
      info.reason = api::tabs::MUTED_INFO_REASON_CAPTURE;
      break;
    case TAB_MUTED_REASON_EXTENSION:
      info.reason = api::tabs::MUTED_INFO_REASON_EXTENSION;
      info.extension_id.reset(
          new std::string(chrome::GetExtensionIdForMutedTab(contents)));
      break;
  }
  return info.ToValue();
}

void ExtensionTabUtil::ScrubTabValueForExtension(
    WebContents* contents,
    const Extension* extension,
    base::DictionaryValue* tab_info) {
  int tab_id = GetTabId(contents);
  bool has_permission = tab_id >= 0 && extension &&
                        extension->permissions_data()->HasAPIPermissionForTab(
                            tab_id, APIPermission::kTab);

  if (!has_permission) {
    tab_info->Remove(keys::kUrlKey, NULL);
    tab_info->Remove(keys::kTitleKey, NULL);
    tab_info->Remove(keys::kFaviconUrlKey, NULL);
  }
}

void ExtensionTabUtil::ScrubTabForExtension(const Extension* extension,
                                            api::tabs::Tab* tab) {
  bool has_permission =
      extension &&
      extension->permissions_data()->HasAPIPermission(APIPermission::kTab);

  if (!has_permission) {
    tab->url.reset();
    tab->title.reset();
    tab->fav_icon_url.reset();
  }
}

bool ExtensionTabUtil::GetTabStripModel(const WebContents* web_contents,
                                        TabStripModel** tab_strip_model,
                                        int* tab_index) {
  DCHECK(web_contents);
  DCHECK(tab_strip_model);
  DCHECK(tab_index);

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    TabStripModel* tab_strip = it->tab_strip_model();
    int index = tab_strip->GetIndexOfWebContents(web_contents);
    if (index != -1) {
      *tab_strip_model = tab_strip;
      *tab_index = index;
      return true;
    }
  }

  return false;
}

bool ExtensionTabUtil::GetDefaultTab(Browser* browser,
                                     WebContents** contents,
                                     int* tab_id) {
  DCHECK(browser);
  DCHECK(contents);

  *contents = browser->tab_strip_model()->GetActiveWebContents();
  if (*contents) {
    if (tab_id)
      *tab_id = GetTabId(*contents);
    return true;
  }

  return false;
}

bool ExtensionTabUtil::GetTabById(int tab_id,
                                  content::BrowserContext* browser_context,
                                  bool include_incognito,
                                  Browser** browser,
                                  TabStripModel** tab_strip,
                                  WebContents** contents,
                                  int* tab_index) {
  if (tab_id == api::tabs::TAB_ID_NONE)
    return false;
  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile() ?
          profile->GetOffTheRecordProfile() : NULL;
  AppWindowRegistry* registry = AppWindowRegistry::Get(profile);
  for (AppWindow* app_window : registry->app_windows()) {
    WebContents* target_contents = app_window->web_contents();
    if (SessionTabHelper::IdForTab(target_contents) == tab_id) {
      if (contents)
        *contents = target_contents;
      return true;
    }
  }
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Browser* target_browser = *it;
    if (target_browser->profile() == profile ||
        target_browser->profile() == incognito_profile) {
      TabStripModel* target_tab_strip = target_browser->tab_strip_model();
      for (int i = 0; i < target_tab_strip->count(); ++i) {
        WebContents* target_contents = target_tab_strip->GetWebContentsAt(i);
        if (SessionTabHelper::IdForTab(target_contents) == tab_id) {
          if (browser)
            *browser = target_browser;
          if (tab_strip)
            *tab_strip = target_tab_strip;
          if (contents)
            *contents = target_contents;
          if (tab_index)
            *tab_index = i;
          return true;
        }
      }
    }
  }
  return false;
}

GURL ExtensionTabUtil::ResolvePossiblyRelativeURL(const std::string& url_string,
                                                  const Extension* extension) {
  GURL url = GURL(url_string);
  if (!url.is_valid())
    url = extension->GetResourceURL(url_string);

  return url;
}

bool ExtensionTabUtil::IsKillURL(const GURL& url) {
  static const char* kill_hosts[] = {
      chrome::kChromeUICrashHost,
      chrome::kChromeUIHangUIHost,
      chrome::kChromeUIKillHost,
      chrome::kChromeUIQuitHost,
      chrome::kChromeUIRestartHost,
      content::kChromeUIBrowserCrashHost,
  };

  // Check a fixed-up URL, to normalize the scheme and parse hosts correctly.
  GURL fixed_url =
      url_formatter::FixupURL(url.possibly_invalid_spec(), std::string());
  if (!fixed_url.SchemeIs(content::kChromeUIScheme))
    return false;

  base::StringPiece fixed_host = fixed_url.host_piece();
  for (size_t i = 0; i < arraysize(kill_hosts); ++i) {
    if (fixed_host == kill_hosts[i])
      return true;
  }

  return false;
}

void ExtensionTabUtil::CreateTab(WebContents* web_contents,
                                 const std::string& extension_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_rect,
                                 bool user_gesture) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  chrome::HostDesktopType active_desktop = chrome::GetActiveDesktop();
  Browser* browser = chrome::FindTabbedBrowser(profile, false, active_desktop);
  const bool browser_created = !browser;
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile, active_desktop));
  chrome::NavigateParams params(browser, web_contents);

  // The extension_app_id parameter ends up as app_name in the Browser
  // which causes the Browser to return true for is_app().  This affects
  // among other things, whether the location bar gets displayed.
  // TODO(mpcomplete): This seems wrong. What if the extension content is hosted
  // in a tab?
  if (disposition == NEW_POPUP)
    params.extension_app_id = extension_id;

  params.disposition = disposition;
  params.window_bounds = initial_rect;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.user_gesture = user_gesture;
  chrome::Navigate(&params);

  // Close the browser if chrome::Navigate created a new one.
  if (browser_created && (browser != params.browser))
    browser->window()->Close();
}

// static
void ExtensionTabUtil::ForEachTab(
    const base::Callback<void(WebContents*)>& callback) {
  for (TabContentsIterator iterator; !iterator.done(); iterator.Next())
    callback.Run(*iterator);
}

// static
WindowController* ExtensionTabUtil::GetWindowControllerOfTab(
    const WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser != NULL)
    return browser->extension_window_controller();

  return NULL;
}

bool ExtensionTabUtil::OpenOptionsPage(const Extension* extension,
                                       Browser* browser) {
  if (!OptionsPageInfo::HasOptionsPage(extension))
    return false;

  // Force the options page to open in non-OTR window, because it won't be
  // able to save settings from OTR.
  scoped_ptr<chrome::ScopedTabbedBrowserDisplayer> displayer;
  if (browser->profile()->IsOffTheRecord()) {
    displayer.reset(new chrome::ScopedTabbedBrowserDisplayer(
        browser->profile()->GetOriginalProfile(),
        browser->host_desktop_type()));
    browser = displayer->browser();
  }

  GURL url_to_navigate;
  if (OptionsPageInfo::ShouldOpenInTab(extension)) {
    // Options page tab is simply e.g. chrome-extension://.../options.html.
    url_to_navigate = OptionsPageInfo::GetOptionsPage(extension);
  } else {
    // Options page tab is Extension settings pointed at that Extension's ID,
    // e.g. chrome://extensions?options=...
    url_to_navigate = GURL(chrome::kChromeUIExtensionsURL);
    GURL::Replacements replacements;
    std::string query =
        base::StringPrintf("options=%s", extension->id().c_str());
    replacements.SetQueryStr(query);
    url_to_navigate = url_to_navigate.ReplaceComponents(replacements);
  }

  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(browser, url_to_navigate));
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  params.url = url_to_navigate;
  chrome::ShowSingletonTabOverwritingNTP(browser, params);
  return true;
}

// static
bool ExtensionTabUtil::BrowserSupportsTabs(Browser* browser) {
  return browser && browser->tab_strip_model() && !browser->is_devtools();
}

}  // namespace extensions
