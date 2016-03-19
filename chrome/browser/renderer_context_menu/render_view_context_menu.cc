// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"

#include <stddef.h>

#include "content/nw/src/common/shell_switches.h"

#include <algorithm>
#include <set>
#include <utility>

#include "apps/app_load_service.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/renderer_context_menu/context_menu_content_type_factory.h"
#include "chrome/browser/renderer_context_menu/spelling_menu_observer.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ssl/chrome_security_state_model_client.h"
#include "chrome/browser/tab_contents/retargeting_details.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/content_restriction.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/spellcheck_common.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/google/core/browser/google_util.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/password_manager/core/common/experiments.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/url_formatter/url_formatter.h"
#include "components/user_prefs/user_prefs.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/path.h"
#include "ui/gfx/text_elider.h"

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#endif

#if defined(ENABLE_PRINTING)
#include "chrome/browser/printing/print_view_manager_common.h"
#include "components/printing/common/print_messages.h"

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_preview_context_menu_observer.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#endif  // defined(ENABLE_PRINT_PREVIEW)
#endif  // defined(ENABLE_PRINTING)

#if defined(ENABLE_MEDIA_ROUTER)
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_metrics.h"
#endif

using base::UserMetricsAction;
using blink::WebContextMenuData;
using blink::WebMediaPlayerAction;
using blink::WebPluginAction;
using blink::WebString;
using blink::WebURL;
using content::BrowserContext;
using content::ChildProcessSecurityPolicy;
using content::DownloadManager;
using content::DownloadUrlParameters;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::SSLStatus;
using content::WebContents;
using extensions::ContextMenuMatcher;
using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;

namespace {

// State of the profile that is activated via "Open Link as User".
enum UmaEnumOpenLinkAsUser {
  OPEN_LINK_AS_USER_ACTIVE_PROFILE_ENUM_ID,
  OPEN_LINK_AS_USER_INACTIVE_PROFILE_MULTI_PROFILE_SESSION_ENUM_ID,
  OPEN_LINK_AS_USER_INACTIVE_PROFILE_SINGLE_PROFILE_SESSION_ENUM_ID,
  OPEN_LINK_AS_USER_LAST_ENUM_ID,
};

#if !defined(OS_CHROMEOS)
// We report the number of "Open Link as User" entries shown in the context
// menu via UMA, differentiating between at most that many profiles.
const int kOpenLinkAsUserMaxProfilesReported = 10;
#endif  // !defined(OS_CHROMEOS)

// Whether to return the general enum_id or context_specific_enum_id
// in the FindUMAEnumValueForCommand lookup function.
enum UmaEnumIdLookupType {
  GENERAL_ENUM_ID,
  CONTEXT_SPECIFIC_ENUM_ID,
};

// Maps UMA enumeration to IDC. IDC could be changed so we can't use
// just them and |UMA_HISTOGRAM_CUSTOM_ENUMERATION|.
// Never change mapping or reuse |enum_id|. Always push back new items.
// Items that is not used any more by |RenderViewContextMenu.ExecuteCommand|
// could be deleted, but don't change the rest of |kUmaEnumToControlId|.
//
// |context_specific_enum_id| matches the ContextMenuOption histogram enum.
// Used to track command usage under specific contexts, specifically Menu
// items under 'link + image' and 'selected text'. Should be set to -1 if
// command is not context specific tracked.
const struct UmaEnumCommandIdPair {
  int enum_id;
  int context_specific_enum_id;
  int control_id;
} kUmaEnumToControlId[] = {
    /*
      enum id for 0, 1 are detected using
      RenderViewContextMenu::IsContentCustomCommandId and
      ContextMenuMatcher::IsExtensionsCustomCommandId
    */
    {2, -1, IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST},
    {3, 0, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB},
    {4, 15, IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW},
    {5, 1, IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD},
    {6, 5, IDC_CONTENT_CONTEXT_SAVELINKAS},
    {7, 18, IDC_CONTENT_CONTEXT_SAVEAVAS},
    {8, 6, IDC_CONTENT_CONTEXT_SAVEIMAGEAS},
    {9, 2, IDC_CONTENT_CONTEXT_COPYLINKLOCATION},
    {10, 10, IDC_CONTENT_CONTEXT_COPYIMAGELOCATION},
    {11, -1, IDC_CONTENT_CONTEXT_COPYAVLOCATION},
    {12, 9, IDC_CONTENT_CONTEXT_COPYIMAGE},
    {13, 8, IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB},
    {14, -1, IDC_CONTENT_CONTEXT_OPENAVNEWTAB},
    {15, -1, IDC_CONTENT_CONTEXT_PLAYPAUSE},
    {16, -1, IDC_CONTENT_CONTEXT_MUTE},
    {17, -1, IDC_CONTENT_CONTEXT_LOOP},
    {18, -1, IDC_CONTENT_CONTEXT_CONTROLS},
    {19, -1, IDC_CONTENT_CONTEXT_ROTATECW},
    {20, -1, IDC_CONTENT_CONTEXT_ROTATECCW},
    {21, -1, IDC_BACK},
    {22, -1, IDC_FORWARD},
    {23, -1, IDC_SAVE_PAGE},
    {24, -1, IDC_RELOAD},
    {25, -1, IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP},
    {26, -1, IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP},
    {27, 16, IDC_PRINT},
    {28, -1, IDC_VIEW_SOURCE},
    {29, -1, IDC_CONTENT_CONTEXT_INSPECTELEMENT},
    {30, -1, IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE},
    {31, -1, IDC_CONTENT_CONTEXT_VIEWPAGEINFO},
    {32, -1, IDC_CONTENT_CONTEXT_TRANSLATE},
    {33, -1, IDC_CONTENT_CONTEXT_RELOADFRAME},
    {34, -1, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE},
    {35, -1, IDC_CONTENT_CONTEXT_VIEWFRAMEINFO},
    {36, -1, IDC_CONTENT_CONTEXT_UNDO},
    {37, -1, IDC_CONTENT_CONTEXT_REDO},
    {38, -1, IDC_CONTENT_CONTEXT_CUT},
    {39, 4, IDC_CONTENT_CONTEXT_COPY},
    {40, -1, IDC_CONTENT_CONTEXT_PASTE},
    {41, -1, IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE},
    {42, -1, IDC_CONTENT_CONTEXT_DELETE},
    {43, -1, IDC_CONTENT_CONTEXT_SELECTALL},
    {44, 17, IDC_CONTENT_CONTEXT_SEARCHWEBFOR},
    {45, -1, IDC_CONTENT_CONTEXT_GOTOURL},
    {46, -1, IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS},
    {47, -1, IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS},
    {52, -1, IDC_CONTENT_CONTEXT_OPENLINKWITH},
    {53, -1, IDC_CHECK_SPELLING_WHILE_TYPING},
    {54, -1, IDC_SPELLCHECK_MENU},
    {55, -1, IDC_CONTENT_CONTEXT_SPELLING_TOGGLE},
    {56, -1, IDC_SPELLCHECK_LANGUAGES_FIRST},
    {57, 11, IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE},
    {58, -1, IDC_SPELLCHECK_SUGGESTION_0},
    {59, -1, IDC_SPELLCHECK_ADD_TO_DICTIONARY},
    {60, -1, IDC_SPELLPANEL_TOGGLE},
    {61, -1, IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB},
    {62, -1, IDC_WRITING_DIRECTION_MENU},
    {63, -1, IDC_WRITING_DIRECTION_DEFAULT},
    {64, -1, IDC_WRITING_DIRECTION_LTR},
    {65, -1, IDC_WRITING_DIRECTION_RTL},
    {66, -1, IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE},
    {67, -1, IDC_CONTENT_CONTEXT_FORCESAVEPASSWORD},
    {68, -1, IDC_ROUTE_MEDIA},
    {69, -1, IDC_CONTENT_CONTEXT_COPYLINKTEXT},
    {70, -1, IDC_CONTENT_CONTEXT_OPENLINKINPROFILE},
    {71, -1, IDC_OPEN_LINK_IN_PROFILE_FIRST},
    // Add new items here and use |enum_id| from the next line.
    // Also, add new items to RenderViewContextMenuItem enum in histograms.xml.
    {72, -1, 0},  // Must be the last. Increment |enum_id| when new IDC
                  // was added.
};

// Collapses large ranges of ids before looking for UMA enum.
int CollapseCommandsForUMA(int id) {
  DCHECK(!RenderViewContextMenu::IsContentCustomCommandId(id));
  DCHECK(!ContextMenuMatcher::IsExtensionsCustomCommandId(id));

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  }

  if (id >= IDC_SPELLCHECK_LANGUAGES_FIRST &&
      id <= IDC_SPELLCHECK_LANGUAGES_LAST) {
    return IDC_SPELLCHECK_LANGUAGES_FIRST;
  }

  if (id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      id <= IDC_SPELLCHECK_SUGGESTION_LAST) {
    return IDC_SPELLCHECK_SUGGESTION_0;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return IDC_OPEN_LINK_IN_PROFILE_FIRST;
  }

  return id;
}

// Returns UMA enum value for command specified by |id| or -1 if not found.
// |use_specific_context_enum| set to true returns the context_specific_enum_id.
int FindUMAEnumValueForCommand(int id, UmaEnumIdLookupType enum_lookup_type) {
  if (RenderViewContextMenu::IsContentCustomCommandId(id))
    return 0;

  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return 1;

  id = CollapseCommandsForUMA(id);
  const size_t kMappingSize = arraysize(kUmaEnumToControlId);
  for (size_t i = 0; i < kMappingSize; ++i) {
    if (kUmaEnumToControlId[i].control_id == id) {
      if (enum_lookup_type == GENERAL_ENUM_ID) {
        return kUmaEnumToControlId[i].enum_id;
      } else if (enum_lookup_type == CONTEXT_SPECIFIC_ENUM_ID &&
                 kUmaEnumToControlId[i].context_specific_enum_id > -1) {
        return kUmaEnumToControlId[i].context_specific_enum_id;
      }
    }
  }

  return -1;
}

// Usually a new tab is expected where this function is used,
// however users should be able to open a tab in background
// or in a new window.
WindowOpenDisposition ForceNewTabDispositionFromEventFlags(
    int event_flags) {
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  return disposition == CURRENT_TAB ? NEW_FOREGROUND_TAB : disposition;
}

// Returns the preference of the profile represented by the |context|.
PrefService* GetPrefs(content::BrowserContext* context) {
  return user_prefs::UserPrefs::Get(context);
}

bool ExtensionPatternMatch(const extensions::URLPatternSet& patterns,
                           const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty())
    return true;
  return patterns.MatchesURL(url);
}

const GURL& GetDocumentURL(const content::ContextMenuParams& params) {
  return params.frame_url.is_empty() ? params.page_url : params.frame_url;
}

content::Referrer CreateReferrer(const GURL& url,
                                 const content::ContextMenuParams& params) {
  const GURL& referring_url = GetDocumentURL(params);
  return content::Referrer::SanitizeForRequest(
      url,
      content::Referrer(referring_url.GetAsReferrer(), params.referrer_policy));
}

content::WebContents* GetWebContentsToUse(content::WebContents* web_contents) {
#if defined(ENABLE_EXTENSIONS)
  // If we're viewing in a MimeHandlerViewGuest, use its embedder WebContents.
  if (extensions::MimeHandlerViewGuest::FromWebContents(web_contents)) {
    WebContents* top_level_web_contents =
        guest_view::GuestViewBase::GetTopLevelWebContents(web_contents);
    if (top_level_web_contents)
      return top_level_web_contents;
  }
#endif
  return web_contents;
}

void WriteURLToClipboard(const GURL& url, const std::string& languages) {
  if (url.is_empty() || !url.is_valid())
    return;

  // Unescaping path and query is not a good idea because other applications
  // may not encode non-ASCII characters in UTF-8.  See crbug.com/2820.
  base::string16 text =
      url.SchemeIs(url::kMailToScheme)
          ? base::ASCIIToUTF16(url.path())
          : url_formatter::FormatUrl(
                url, languages, url_formatter::kFormatUrlOmitNothing,
                net::UnescapeRule::NONE, nullptr, nullptr, nullptr);

  ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
  scw.WriteURL(text);
}

void WriteTextToClipboard(const base::string16& text) {
  ui::ScopedClipboardWriter scw(ui::CLIPBOARD_TYPE_COPY_PASTE);
  scw.WriteText(text);
}

bool g_custom_id_ranges_initialized = false;

#if !defined(OS_CHROMEOS)
void AddIconToLastMenuItem(gfx::Image icon, ui::SimpleMenuModel* menu) {
  int width = icon.Width();
  int height = icon.Height();

  // Don't try to scale too small icons.
  if (width < 16 || height < 16)
    return;

  // Profile avatars are supposed to be displayed with a circular mask, so apply
  // one.
  gfx::Path circular_mask;
  gfx::Canvas canvas(icon.Size(), 1.0f, true);
  canvas.FillRect(gfx::Rect(icon.Size()), SK_ColorTRANSPARENT,
                  SkXfermode::kClear_Mode);
  circular_mask.addCircle(SkIntToScalar(width) / 2, SkIntToScalar(height) / 2,
                          SkIntToScalar(std::min(width, height)) / 2);
  canvas.ClipPath(circular_mask, true);
  canvas.DrawImageInt(*icon.ToImageSkia(), 0, 0);

  gfx::CalculateFaviconTargetSize(&width, &height);
  gfx::Image sized_icon = profiles::GetSizedAvatarIcon(
      gfx::Image(gfx::ImageSkia(canvas.ExtractImageRep())), true, width,
      height);
  menu->SetIcon(menu->GetItemCount() - 1, sized_icon);
}
#endif  // !defined(OS_CHROMEOS)

void OnProfileCreated(chrome::HostDesktopType desktop_type,
                      const GURL& link_url,
                      const content::Referrer& referrer,
                      Profile* profile,
                      Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    Browser* browser = chrome::FindLastActiveWithProfile(profile, desktop_type);
    chrome::NavigateParams nav_params(browser, link_url,
                                      ui::PAGE_TRANSITION_LINK);
    nav_params.disposition = NEW_FOREGROUND_TAB;
    nav_params.referrer = referrer;
    nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
    chrome::Navigate(&nav_params);
  }
}

}  // namespace

// static
gfx::Vector2d RenderViewContextMenu::GetOffset(
    RenderFrameHost* render_frame_host) {
  gfx::Vector2d offset;
#if defined(ENABLE_EXTENSIONS)
  WebContents* web_contents =
      WebContents::FromRenderFrameHost(render_frame_host);
  WebContents* top_level_web_contents =
      guest_view::GuestViewBase::GetTopLevelWebContents(web_contents);
  if (web_contents && top_level_web_contents &&
      web_contents != top_level_web_contents) {
    gfx::Rect bounds = web_contents->GetContainerBounds();
    gfx::Rect top_level_bounds = top_level_web_contents->GetContainerBounds();
    offset = bounds.origin() - top_level_bounds.origin();
  }
#endif  // defined(ENABLE_EXTENSIONS)
  return offset;
}

// static
bool RenderViewContextMenu::IsDevToolsURL(const GURL& url) {
  return url.SchemeIs(content::kChromeDevToolsScheme);
}

// static
bool RenderViewContextMenu::IsInternalResourcesURL(const GURL& url) {
  if (!url.SchemeIs(content::kChromeUIScheme))
    return false;
  return url.host() == chrome::kChromeUISyncResourcesHost;
}

RenderViewContextMenu::RenderViewContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params)
    : RenderViewContextMenuBase(render_frame_host, params),
      extension_items_(browser_context_,
                       this,
                       &menu_model_,
                       base::Bind(MenuItemMatchesParams, params_)),
      profile_link_submenu_model_(this),
      multiple_profiles_open_(false),
      protocol_handler_submenu_model_(this),
      protocol_handler_registry_(
          ProtocolHandlerRegistryFactory::GetForBrowserContext(GetProfile())),
      embedder_web_contents_(GetWebContentsToUse(source_web_contents_)) {
  if (!g_custom_id_ranges_initialized) {
    g_custom_id_ranges_initialized = true;
    SetContentCustomCommandIdRange(IDC_CONTENT_CONTEXT_CUSTOM_FIRST,
                                   IDC_CONTENT_CONTEXT_CUSTOM_LAST);
  }
  set_content_type(ContextMenuContentTypeFactory::Create(
      source_web_contents_, params));
}

RenderViewContextMenu::~RenderViewContextMenu() {
}

// Menu construction functions -------------------------------------------------

#if defined(ENABLE_EXTENSIONS)
// static
bool RenderViewContextMenu::ExtensionContextAndPatternMatch(
    const content::ContextMenuParams& params,
    const MenuItem::ContextList& contexts,
    const extensions::URLPatternSet& target_url_patterns) {
  const bool has_link = !params.link_url.is_empty();
  const bool has_selection = !params.selection_text.empty();
  const bool in_frame = !params.frame_url.is_empty();

  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (in_frame && contexts.Contains(MenuItem::FRAME)))
    return true;

  if (has_link && contexts.Contains(MenuItem::LINK) &&
      ExtensionPatternMatch(target_url_patterns, params.link_url))
    return true;

  switch (params.media_type) {
    case WebContextMenuData::MediaTypeImage:
      if (contexts.Contains(MenuItem::IMAGE) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case WebContextMenuData::MediaTypeVideo:
      if (contexts.Contains(MenuItem::VIDEO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    case WebContextMenuData::MediaTypeAudio:
      if (contexts.Contains(MenuItem::AUDIO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url))
        return true;
      break;

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply (except for FRAME, which is included in PAGE for
  // backwards compatibility).
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == WebContextMenuData::MediaTypeNone &&
      contexts.Contains(MenuItem::PAGE))
    return true;

  return false;
}

// static
bool RenderViewContextMenu::MenuItemMatchesParams(
    const content::ContextMenuParams& params,
    const extensions::MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match)
    return false;

  const GURL& document_url = GetDocumentURL(params);
  return ExtensionPatternMatch(item->document_url_patterns(), document_url);
}

void RenderViewContextMenu::AppendAllExtensionItems() {
  extension_items_.Clear();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(browser_context_)->extension_service();
  if (!service)
    return;  // In unit-tests, we may not have an ExtensionService.

  MenuManager* menu_manager = MenuManager::Get(browser_context_);
  if (!menu_manager)
    return;

  base::string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::set<MenuItem::ExtensionKey> ids = menu_manager->ExtensionIds();
  std::vector<base::string16> sorted_menu_titles;
  std::map<base::string16, std::vector<const Extension*>>
      title_to_extensions_map;
  for (std::set<MenuItem::ExtensionKey>::iterator iter = ids.begin();
       iter != ids.end();
       ++iter) {
    const Extension* extension =
        service->GetExtensionById(iter->extension_id, false);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app()) {
      base::string16 menu_title = extension_items_.GetTopLevelContextMenuTitle(
          *iter, printable_selection_text);
      title_to_extensions_map[menu_title].push_back(extension);
      sorted_menu_titles.push_back(menu_title);
    }
  }
  if (sorted_menu_titles.empty())
    return;

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());

  int index = 0;
  for (size_t i = 0; i < sorted_menu_titles.size(); ++i) {
    std::vector<const Extension*>& extensions =
        title_to_extensions_map[sorted_menu_titles[i]];
    for (const auto& extension : extensions) {
      MenuItem::ExtensionKey extension_key(extension->id());
      extension_items_.AppendExtensionItems(extension_key,
                                            printable_selection_text, &index,
                                            false);  // is_action_menu
    }
  }
}

void RenderViewContextMenu::AppendCurrentExtensionItems() {
  // Avoid appending extension related items when |extension| is null.
  // For Panel, this happens when the panel is navigated to a url outside of the
  // extension's package.
  const Extension* extension = GetExtension();
  if (!extension)
    return;

  extensions::WebViewGuest* web_view_guest =
      extensions::WebViewGuest::FromWebContents(source_web_contents_);
  MenuItem::ExtensionKey key;
  if (web_view_guest) {
    key = MenuItem::ExtensionKey(
        extension->id(),
        web_view_guest->owner_web_contents()->GetRenderProcessHost()->GetID(),
        web_view_guest->view_instance_id());
  } else {
    key = MenuItem::ExtensionKey(extension->id());
  }

  // Only add extension items from this extension.
  int index = 0;
  extension_items_.AppendExtensionItems(key, PrintableSelectionText(), &index,
                                        false /* is_action_menu */);
}
#endif  // defined(ENABLE_EXTENSIONS)

void RenderViewContextMenu::InitMenu() {
  RenderViewContextMenuBase::InitMenu();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    AppendPageItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK)) {
    AppendLinkItems();
    if (params_.media_type != WebContextMenuData::MediaTypeNone)
      menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    AppendImageItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCHWEBFORIMAGE)) {
    AppendSearchWebForImageItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_VIDEO)) {
    AppendVideoItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_AUDIO)) {
    AppendAudioItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_CANVAS)) {
    AppendCanvasItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendPluginItems();
  }

  // ITEM_GROUP_MEDIA_FILE has no specific items.

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_EDITABLE))
    AppendEditableItems();

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_COPY)) {
    DCHECK(!content_type_->SupportsGroup(
               ContextMenuContentType::ITEM_GROUP_EDITABLE));
    AppendCopyItem();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER)) {
    AppendSearchProvider();
  }

  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT) &&
      !content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
    AppendPrintItem();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_EDITABLE)) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    AppendPlatformEditableItems();
    AppendLanguageSettings();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_MEDIA_PLUGIN)) {
    AppendRotationItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
               ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION));
    AppendAllExtensionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_CURRENT_EXTENSION)) {
    DCHECK(!content_type_->SupportsGroup(
               ContextMenuContentType::ITEM_GROUP_ALL_EXTENSION));
    AppendCurrentExtensionItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVELOPER)) {
    AppendDeveloperItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_DEVTOOLS_UNPACKED_EXT)) {
    AppendDevtoolsForUnpackedExtensions();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PRINT_PREVIEW)) {
    AppendPrintPreviewItems();
  }

  if (content_type_->SupportsGroup(
          ContextMenuContentType::ITEM_GROUP_PASSWORD)) {
    AppendPasswordItems();
  }
}

Profile* RenderViewContextMenu::GetProfile() {
  return Profile::FromBrowserContext(browser_context_);
}

void RenderViewContextMenu::RecordUsedItem(int id) {
  int enum_id = FindUMAEnumValueForCommand(id, GENERAL_ENUM_ID);
  if (enum_id != -1) {
    const size_t kMappingSize = arraysize(kUmaEnumToControlId);
    UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.Used", enum_id,
                              kUmaEnumToControlId[kMappingSize - 1].enum_id);
    // Record to additional context specific histograms.
    enum_id = FindUMAEnumValueForCommand(id, CONTEXT_SPECIFIC_ENUM_ID);

    // Linked image context.
    if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_LINK) &&
        content_type_->SupportsGroup(
            ContextMenuContentType::ITEM_GROUP_MEDIA_IMAGE)) {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOption.ImageLink", enum_id,
                                kUmaEnumToControlId[kMappingSize - 1].enum_id);
    }
    // Selected text context.
    if (content_type_->SupportsGroup(
            ContextMenuContentType::ITEM_GROUP_SEARCH_PROVIDER) &&
        content_type_->SupportsGroup(
            ContextMenuContentType::ITEM_GROUP_PRINT)) {
      UMA_HISTOGRAM_ENUMERATION("ContextMenu.SelectedOption.SelectedText",
                                enum_id,
                                kUmaEnumToControlId[kMappingSize - 1].enum_id);
    }
  } else {
    NOTREACHED() << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
  }
}

void RenderViewContextMenu::RecordShownItem(int id) {
  int enum_id = FindUMAEnumValueForCommand(id, GENERAL_ENUM_ID);
  if (enum_id != -1) {
    const size_t kMappingSize = arraysize(kUmaEnumToControlId);
    UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.Shown", enum_id,
                              kUmaEnumToControlId[kMappingSize - 1].enum_id);
  } else {
    // Just warning here. It's harder to maintain list of all possibly
    // visible items than executable items.
    DLOG(ERROR) << "Update kUmaEnumToControlId. Unhanded IDC: " << id;
  }
}

#if defined(ENABLE_PLUGINS)
void RenderViewContextMenu::HandleAuthorizeAllPlugins() {
  ChromePluginServiceFilter::GetInstance()->AuthorizeAllPlugins(
      source_web_contents_, false, std::string());
}
#endif

void RenderViewContextMenu::AppendPrintPreviewItems() {
#if defined(ENABLE_PRINT_PREVIEW)
  if (!print_preview_menu_observer_.get()) {
    print_preview_menu_observer_.reset(
        new PrintPreviewContextMenuObserver(source_web_contents_));
  }

  observers_.AddObserver(print_preview_menu_observer_.get());
#endif
}

const Extension* RenderViewContextMenu::GetExtension() const {
  return extensions::ProcessManager::Get(browser_context_)
      ->GetExtensionForWebContents(source_web_contents_);
}

void RenderViewContextMenu::AppendDeveloperItems() {
  // Show Inspect Element in DevTools itself only in case of the debug
  // devtools build.
  bool show_developer_items = !IsDevToolsURL(params_.page_url);

#if defined(DEBUG_DEVTOOLS)
  show_developer_items = true;
#endif

  if (!show_developer_items)
    return;

  // In the DevTools popup menu, "developer items" is normally the only
  // section, so omit the separator there.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PAGE))
    menu_model_.AddItemWithStringId(IDC_VIEW_SOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWPAGESOURCE);
  if (content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_FRAME)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                                    IDS_CONTENT_CONTEXT_VIEWFRAMESOURCE);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOADFRAME,
                                    IDS_CONTENT_CONTEXT_RELOADFRAME);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                                  IDS_CONTENT_CONTEXT_INSPECTELEMENT);
}

void RenderViewContextMenu::AppendDevtoolsForUnpackedExtensions() {
  // Add a separator if there are any items already in the menu.
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RELOAD_PACKAGED_APP);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP,
                                  IDS_CONTENT_CONTEXT_RESTART_APP);
  AppendDeveloperItems();
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE,
                                  IDS_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE);
}

void RenderViewContextMenu::AppendLinkItems() {
  if (!params_.link_url.is_empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                    IDS_CONTENT_CONTEXT_OPENLINKNEWTAB);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW,
                                    IDS_CONTENT_CONTEXT_OPENLINKNEWWINDOW);
    if (params_.link_url.is_valid()) {
      AppendProtocolHandlerSubMenu();
    }

    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                                    IDS_CONTENT_CONTEXT_OPENLINKOFFTHERECORD);

    // While ChromeOS supports multiple profiles, only one can be open at a
    // time.
    // TODO(jochen): Consider adding support for ChromeOS with similar
    // semantics as the profile switcher in the system tray.
#if !defined(OS_CHROMEOS)
    // g_browser_process->profile_manager() is null during unit tests.
    if (g_browser_process->profile_manager() &&
        GetProfile()->GetProfileType() == Profile::REGULAR_PROFILE) {
      ProfileManager* profile_manager = g_browser_process->profile_manager();
      const ProfileInfoCache& profile_info_cache =
          profile_manager->GetProfileInfoCache();
      chrome::HostDesktopType desktop_type =
          chrome::GetHostDesktopTypeForNativeView(
              source_web_contents_->GetNativeView());

      // Find all regular profiles other than the current one which have at
      // least one open window.
      std::vector<size_t> target_profiles;
      const size_t profile_count = profile_info_cache.GetNumberOfProfiles();
      for (size_t profile_index = 0; profile_index < profile_count;
           ++profile_index) {
        base::FilePath profile_path =
            profile_info_cache.GetPathOfProfileAtIndex(profile_index);
        Profile* profile = profile_manager->GetProfileByPath(profile_path);
        if ((profile != GetProfile()) &&
            !profile_info_cache.IsOmittedProfileAtIndex(profile_index) &&
            !profile_info_cache.ProfileIsSigninRequiredAtIndex(profile_index)) {
          target_profiles.push_back(profile_index);
          if (chrome::FindLastActiveWithProfile(profile, desktop_type))
            multiple_profiles_open_ = true;
        }
      }

      if (!target_profiles.empty()) {
        UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.OpenLinkAsUserShown",
                                  target_profiles.size(),
                                  kOpenLinkAsUserMaxProfilesReported);
      }

      if (target_profiles.size() == 1) {
        size_t profile_index = target_profiles[0];
        menu_model_.AddItem(
            IDC_OPEN_LINK_IN_PROFILE_FIRST + profile_index,
            l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_OPENLINKINPROFILE,
                profile_info_cache.GetNameOfProfileAtIndex(profile_index)));
        AddIconToLastMenuItem(
            profile_info_cache.GetAvatarIconOfProfileAtIndex(profile_index),
            &menu_model_);
      } else if (target_profiles.size() > 1) {
        for (size_t profile_index : target_profiles) {
          // In extreme cases, we might have more profiles than available
          // command ids. In that case, just stop creating new entries - the
          // menu is probably useless at this point already.
          if (IDC_OPEN_LINK_IN_PROFILE_FIRST + profile_index >
              IDC_OPEN_LINK_IN_PROFILE_LAST)
            break;
          profile_link_submenu_model_.AddItem(
              IDC_OPEN_LINK_IN_PROFILE_FIRST + profile_index,
              profile_info_cache.GetNameOfProfileAtIndex(profile_index));
          AddIconToLastMenuItem(
              profile_info_cache.GetAvatarIconOfProfileAtIndex(profile_index),
              &profile_link_submenu_model_);
        }
        menu_model_.AddSubMenuWithStringId(
            IDC_CONTENT_CONTEXT_OPENLINKINPROFILE,
            IDS_CONTENT_CONTEXT_OPENLINKINPROFILES,
            &profile_link_submenu_model_);
      }
    }
#endif  // !defined(OS_CHROMEOS)
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVELINKAS,
                                    IDS_CONTENT_CONTEXT_SAVELINKAS);
  }

  menu_model_.AddItemWithStringId(
      IDC_CONTENT_CONTEXT_COPYLINKLOCATION,
      params_.link_url.SchemeIs(url::kMailToScheme) ?
          IDS_CONTENT_CONTEXT_COPYEMAILADDRESS :
          IDS_CONTENT_CONTEXT_COPYLINKLOCATION);

  if (params_.source_type == ui::MENU_SOURCE_TOUCH &&
      params_.media_type != WebContextMenuData::MediaTypeImage &&
      !params_.link_text.empty()) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYLINKTEXT,
                                    IDS_CONTENT_CONTEXT_COPYLINKTEXT);
  }
}

void RenderViewContextMenu::AppendImageItems() {
  std::map<std::string, std::string>::const_iterator it =
      params_.properties.find(data_reduction_proxy::chrome_proxy_header());
  if (it != params_.properties.end() && it->second ==
      data_reduction_proxy::chrome_proxy_lo_fi_directive()) {
    menu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE,
        IDS_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE);
  }
  DataReductionProxyChromeSettings* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context_);
  if (settings && settings->CanUseDataReductionProxy(params_.src_url)) {
    menu_model_.AddItemWithStringId(
        IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB,
        IDS_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB);
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB,
                                    IDS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
  }
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGELOCATION,
                                  IDS_CONTENT_CONTEXT_COPYIMAGELOCATION);
}

void RenderViewContextMenu::AppendSearchWebForImageItems() {
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(GetProfile());
  const TemplateURL* const default_provider =
      service->GetDefaultSearchProvider();
  if (params_.has_image_contents && default_provider &&
      !default_provider->image_url().empty() &&
      default_provider->image_url_ref().IsValid(service->search_terms_data())) {
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFORIMAGE,
                                   default_provider->short_name()));
  }
}

void RenderViewContextMenu::AppendAudioItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENAUDIONEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEAUDIOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYAUDIOLOCATION);
  AppendMediaRouterItem();
}

void RenderViewContextMenu::AppendCanvasItems() {
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEIMAGEAS,
                                  IDS_CONTENT_CONTEXT_SAVEIMAGEAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYIMAGE,
                                  IDS_CONTENT_CONTEXT_COPYIMAGE);
}

void RenderViewContextMenu::AppendVideoItems() {
  AppendMediaItems();
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_OPENAVNEWTAB,
                                  IDS_CONTENT_CONTEXT_OPENVIDEONEWTAB);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                  IDS_CONTENT_CONTEXT_SAVEVIDEOAS);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPYAVLOCATION,
                                  IDS_CONTENT_CONTEXT_COPYVIDEOLOCATION);
  AppendMediaRouterItem();
}

void RenderViewContextMenu::AppendMediaItems() {
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_LOOP,
                                       IDS_CONTENT_CONTEXT_LOOP);
  menu_model_.AddCheckItemWithStringId(IDC_CONTENT_CONTEXT_CONTROLS,
                                       IDS_CONTENT_CONTEXT_CONTROLS);
}

void RenderViewContextMenu::AppendPluginItems() {
  if (params_.page_url == params_.src_url ||
      guest_view::GuestViewBase::IsGuest(source_web_contents_)) {
    // Full page plugin, so show page menu items.
    if (params_.link_url.is_empty() && params_.selection_text.empty())
      AppendPageItems();
  } else {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SAVEAVAS,
                                    IDS_CONTENT_CONTEXT_SAVEPAGEAS);
    // The "Print" menu item should always be included for plugins. If
    // content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)
    // is true the item will be added inside AppendPrintItem(). Otherwise we
    // add "Print" here.
    if (!content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT))
      menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
}

void RenderViewContextMenu::AppendPageItems() {
  menu_model_.AddItemWithStringId(IDC_BACK, IDS_CONTENT_CONTEXT_BACK);
  menu_model_.AddItemWithStringId(IDC_FORWARD, IDS_CONTENT_CONTEXT_FORWARD);
  menu_model_.AddItemWithStringId(IDC_RELOAD, IDS_CONTENT_CONTEXT_RELOAD);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_SAVE_PAGE,
                                  IDS_CONTENT_CONTEXT_SAVEPAGEAS);
  menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  AppendMediaRouterItem();

#if 0
  if (TranslateService::IsTranslatableURL(params_.page_url)) {
    std::string locale = g_browser_process->GetApplicationLocale();
    locale = translate::TranslateDownloadManager::GetLanguageCode(locale);
    base::string16 language =
        l10n_util::GetDisplayNameForLocale(locale, locale, true);
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_TRANSLATE,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_TRANSLATE, language));
  }
#endif
}

void RenderViewContextMenu::AppendCopyItem() {
  if (menu_model_.GetItemCount())
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
}

void RenderViewContextMenu::AppendPrintItem() {
  if (GetPrefs(browser_context_)->GetBoolean(prefs::kPrintingEnabled) &&
      (params_.media_type == WebContextMenuData::MediaTypeNone ||
       params_.media_flags & WebContextMenuData::MediaCanPrint)) {
    menu_model_.AddItemWithStringId(IDC_PRINT, IDS_CONTENT_CONTEXT_PRINT);
  }
}

void RenderViewContextMenu::AppendMediaRouterItem() {
  if (!browser_context_->IsOffTheRecord() &&
      media_router::MediaRouterEnabled(browser_context_)) {
    menu_model_.AddItemWithStringId(IDC_ROUTE_MEDIA,
                                    IDS_MEDIA_ROUTER_MENU_ITEM_TITLE);
  }
}

void RenderViewContextMenu::AppendRotationItems() {
  if (params_.media_flags & WebContextMenuData::MediaCanRotate) {
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECW,
                                    IDS_CONTENT_CONTEXT_ROTATECW);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_ROTATECCW,
                                    IDS_CONTENT_CONTEXT_ROTATECCW);
  }
}

void RenderViewContextMenu::AppendSearchProvider() {
  DCHECK(browser_context_);

  base::TrimWhitespace(params_.selection_text, base::TRIM_ALL,
                       &params_.selection_text);
  if (params_.selection_text.empty())
    return;

  base::ReplaceChars(params_.selection_text, AutocompleteMatch::kInvalidChars,
                     base::ASCIIToUTF16(" "), &params_.selection_text);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(GetProfile())->Classify(
      params_.selection_text,
      false,
      false,
      metrics::OmniboxEventProto::INVALID_SPEC,
      &match,
      NULL);
  selection_navigation_url_ = match.destination_url;
  if (!selection_navigation_url_.is_valid())
    return;

  base::string16 printable_selection_text = PrintableSelectionText();
  EscapeAmpersands(&printable_selection_text);

  if (AutocompleteMatch::IsSearchType(match.type)) {
    const TemplateURL* const default_provider =
        TemplateURLServiceFactory::GetForProfile(GetProfile())
            ->GetDefaultSearchProvider();
    if (!default_provider)
      return;
    menu_model_.AddItem(
        IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
        l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_SEARCHWEBFOR,
                                   default_provider->short_name(),
                                   printable_selection_text));
  } else {
    if ((selection_navigation_url_ != params_.link_url) &&
        ChildProcessSecurityPolicy::GetInstance()->IsWebSafeScheme(
            selection_navigation_url_.scheme())) {
      menu_model_.AddItem(
          IDC_CONTENT_CONTEXT_GOTOURL,
          l10n_util::GetStringFUTF16(IDS_CONTENT_CONTEXT_GOTOURL,
                                     printable_selection_text));
    }
  }
}

void RenderViewContextMenu::AppendEditableItems() {
  bool use_spellcheck_and_search = !chrome::IsRunningInForcedAppMode();
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableSpellChecking))
    use_spellcheck_and_search = false;

  if (use_spellcheck_and_search)
    AppendSpellingSuggestionsSubMenu();

// 'Undo' and 'Redo' for text input with no suggestions and no text selected.
// We make an exception for OS X as context clicking will select the closest
// word. In this case both items are always shown.
#if defined(OS_MACOSX)
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                  IDS_CONTENT_CONTEXT_UNDO);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                  IDS_CONTENT_CONTEXT_REDO);
  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
#else
  if (!IsDevToolsURL(params_.page_url) && !menu_model_.GetItemCount() &&
      !content_type_->SupportsGroup(ContextMenuContentType::ITEM_GROUP_PRINT)) {
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_UNDO,
                                    IDS_CONTENT_CONTEXT_UNDO);
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_REDO,
                                    IDS_CONTENT_CONTEXT_REDO);
    menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  }
#endif

  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_CUT,
                                  IDS_CONTENT_CONTEXT_CUT);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_COPY,
                                  IDS_CONTENT_CONTEXT_COPY);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE,
                                  IDS_CONTENT_CONTEXT_PASTE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE,
                                  IDS_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_SELECTALL,
                                  IDS_CONTENT_CONTEXT_SELECTALL);

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
}

void RenderViewContextMenu::AppendLanguageSettings() {
#if 0
  const bool use_spellcheck_and_search = !chrome::IsRunningInForcedAppMode();

  if (use_spellcheck_and_search)
    menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS,
                                    IDS_CONTENT_CONTEXT_LANGUAGE_SETTINGS);
#endif
}

void RenderViewContextMenu::AppendSpellingSuggestionsSubMenu() {
  if (!spelling_menu_observer_.get())
    spelling_menu_observer_.reset(new SpellingMenuObserver(this));
  observers_.AddObserver(spelling_menu_observer_.get());
  spelling_menu_observer_->InitMenu(params_);
}

void RenderViewContextMenu::AppendProtocolHandlerSubMenu() {
  const ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      GetHandlersForLinkUrl();
  if (handlers.empty())
    return;
  size_t max = IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST -
      IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
  for (size_t i = 0; i < handlers.size() && i <= max; i++) {
    protocol_handler_submenu_model_.AddItem(
        IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST + i,
        base::UTF8ToUTF16(handlers[i].url().host()));
  }
  protocol_handler_submenu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  protocol_handler_submenu_model_.AddItem(
      IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH_CONFIGURE));

  menu_model_.AddSubMenu(
      IDC_CONTENT_CONTEXT_OPENLINKWITH,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_OPENLINKWITH),
      &protocol_handler_submenu_model_);
}

void RenderViewContextMenu::AppendPasswordItems() {
  if (!password_manager::ForceSavingExperimentEnabled())
    return;

  menu_model_.AddSeparator(ui::NORMAL_SEPARATOR);
  menu_model_.AddItemWithStringId(IDC_CONTENT_CONTEXT_FORCESAVEPASSWORD,
                                  IDS_CONTENT_CONTEXT_FORCESAVEPASSWORD);
}

// Menu delegate functions -----------------------------------------------------

bool RenderViewContextMenu::IsCommandIdEnabled(int id) const {
  {
    bool enabled = false;
    if (RenderViewContextMenuBase::IsCommandIdKnown(id, &enabled))
      return enabled;
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  int content_restrictions = 0;
  if (core_tab_helper)
    content_restrictions = core_tab_helper->content_restrictions();
  if (id == IDC_PRINT && (content_restrictions & CONTENT_RESTRICTION_PRINT))
    return false;

  if (id == IDC_SAVE_PAGE &&
      (content_restrictions & CONTENT_RESTRICTION_SAVE)) {
    return false;
  }

  PrefService* prefs = GetPrefs(browser_context_);

  // Allow Spell Check language items on sub menu for text area context menu.
  if ((id >= IDC_SPELLCHECK_LANGUAGES_FIRST) &&
      (id < IDC_SPELLCHECK_LANGUAGES_LAST)) {
    return prefs->GetBoolean(prefs::kEnableContinuousSpellcheck);
  }

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdEnabled(id);

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    return true;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    return params_.link_url.is_valid();
  }

  IncognitoModePrefs::Availability incognito_avail =
      IncognitoModePrefs::GetAvailability(prefs);
  switch (id) {
    case IDC_BACK:
      return embedder_web_contents_->GetController().CanGoBack();

    case IDC_FORWARD:
      return embedder_web_contents_->GetController().CanGoForward();

    case IDC_RELOAD: {
      CoreTabHelper* core_tab_helper =
          CoreTabHelper::FromWebContents(embedder_web_contents_);
      if (!core_tab_helper)
        return false;

      CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
      return !core_delegate ||
             core_delegate->CanReloadContents(embedder_web_contents_);
    }

    case IDC_VIEW_SOURCE:
    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      if (!!extensions::MimeHandlerViewGuest::FromWebContents(
              source_web_contents_)) {
        return false;
      }
      return (params_.media_type != WebContextMenuData::MediaTypePlugin) &&
             embedder_web_contents_->GetController().CanViewSource();

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE:
    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP:
    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP:
      return IsDevCommandEnabled(id);

    case IDC_CONTENT_CONTEXT_VIEWPAGEINFO:
      if (embedder_web_contents_->GetController().GetVisibleEntry() == NULL)
        return false;
      // Disabled if no browser is associated (e.g. desktop notifications).
      if (chrome::FindBrowserWithWebContents(embedder_web_contents_) == NULL)
        return false;
      return true;

    case IDC_CONTENT_CONTEXT_TRANSLATE: {
      return false;
#if 0
      ChromeTranslateClient* chrome_translate_client =
          ChromeTranslateClient::FromWebContents(embedder_web_contents_);
      // If no |chrome_translate_client| attached with this WebContents or we're
      // viewing in a MimeHandlerViewGuest translate will be disabled.
      if (!chrome_translate_client ||
          !!extensions::MimeHandlerViewGuest::FromWebContents(
              source_web_contents_)) {
        return false;
      }
      std::string original_lang =
          chrome_translate_client->GetLanguageState().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang =
          translate::TranslateDownloadManager::GetLanguageCode(target_lang);
      // Note that we intentionally enable the menu even if the original and
      // target languages are identical.  This is to give a way to user to
      // translate a page that might contains text fragments in a different
      // language.
      return ((params_.edit_flags & WebContextMenuData::CanTranslate) != 0) &&
             !original_lang.empty() &&  // Did we receive the page language yet?
             !chrome_translate_client->GetLanguageState().IsPageTranslated() &&
             !embedder_web_contents_->GetInterstitialPage() &&
             // There are some application locales which can't be used as a
             // target language for translation.
             translate::TranslateDownloadManager::IsSupportedLanguage(
                 target_lang) &&
             // Disable on the Instant Extended NTP.
             !search::IsInstantNTP(embedder_web_contents_);
#endif
    }

    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB:
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
    case IDC_CONTENT_CONTEXT_OPENLINKINPROFILE:
      return params_.link_url.is_valid();

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      return params_.unfiltered_link_url.is_valid();

    case IDC_CONTENT_CONTEXT_COPYLINKTEXT:
      return true;

    case IDC_CONTENT_CONTEXT_SAVELINKAS: {
      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      return params_.link_url.is_valid() &&
          ProfileIOData::IsHandledProtocol(params_.link_url.scheme());
    }

    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS: {
      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      return params_.has_image_contents;
    }

    // The images shown in the most visited thumbnails can't be opened or
    // searched for conventionally.
    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
    case IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE:
    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      return params_.src_url.is_valid() &&
          (params_.src_url.scheme() != content::kChromeUIScheme);

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      return params_.has_image_contents;

    // Media control commands should all be disabled if the player is in an
    // error state.
    case IDC_CONTENT_CONTEXT_PLAYPAUSE:
    case IDC_CONTENT_CONTEXT_LOOP:
      return (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    // Mute and unmute should also be disabled if the player has no audio.
    case IDC_CONTENT_CONTEXT_MUTE:
      return (params_.media_flags &
              WebContextMenuData::MediaHasAudio) != 0 &&
             (params_.media_flags &
              WebContextMenuData::MediaInError) == 0;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      return (params_.media_flags &
              WebContextMenuData::MediaCanToggleControls) != 0;

    case IDC_CONTENT_CONTEXT_ROTATECW:
    case IDC_CONTENT_CONTEXT_ROTATECCW:
      return
          (params_.media_flags & WebContextMenuData::MediaCanRotate) != 0;

    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
      return params_.src_url.is_valid();

    case IDC_CONTENT_CONTEXT_SAVEAVAS: {
      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      const GURL& url = params_.src_url;
      bool can_save =
          (params_.media_flags & WebContextMenuData::MediaCanSave) &&
          url.is_valid() && ProfileIOData::IsHandledProtocol(url.scheme());
#if defined(ENABLE_PRINT_PREVIEW)
          // Do not save the preview PDF on the print preview page.
      can_save = can_save &&
          !(printing::PrintPreviewDialogController::IsPrintPreviewURL(url));
#endif
      return can_save;
    }

    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      // Currently, a media element can be opened in a new tab iff it can
      // be saved. So rather than duplicating the MediaCanSave flag, we rely
      // on that here.
      return !!(params_.media_flags & WebContextMenuData::MediaCanSave);

    case IDC_SAVE_PAGE: {
      CoreTabHelper* core_tab_helper =
          CoreTabHelper::FromWebContents(embedder_web_contents_);
      if (!core_tab_helper)
        return false;

      CoreTabHelperDelegate* core_delegate = core_tab_helper->delegate();
      if (core_delegate &&
          !core_delegate->CanSaveContents(embedder_web_contents_))
        return false;

      PrefService* local_state = g_browser_process->local_state();
      DCHECK(local_state);
      // Test if file-selection dialogs are forbidden by policy.
      if (!local_state->GetBoolean(prefs::kAllowFileSelectionDialogs))
        return false;

      // We save the last committed entry (which the user is looking at), as
      // opposed to any pending URL that hasn't committed yet.
      NavigationEntry* entry =
          embedder_web_contents_->GetController().GetLastCommittedEntry();
      return content::IsSavableURL(entry ? entry->GetURL() : GURL());
    }

    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      return params_.frame_url.is_valid();

    case IDC_CONTENT_CONTEXT_UNDO:
      return !!(params_.edit_flags & WebContextMenuData::CanUndo);

    case IDC_CONTENT_CONTEXT_REDO:
      return !!(params_.edit_flags & WebContextMenuData::CanRedo);

    case IDC_CONTENT_CONTEXT_CUT:
      return !!(params_.edit_flags & WebContextMenuData::CanCut);

    case IDC_CONTENT_CONTEXT_COPY:
      return !!(params_.edit_flags & WebContextMenuData::CanCopy);

    case IDC_CONTENT_CONTEXT_PASTE: {
      if (!(params_.edit_flags & WebContextMenuData::CanPaste))
        return false;

      std::vector<base::string16> types;
      bool ignore;
      ui::Clipboard::GetForCurrentThread()->ReadAvailableTypes(
          ui::CLIPBOARD_TYPE_COPY_PASTE, &types, &ignore);
      return !types.empty();
    }

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE: {
      if (!(params_.edit_flags & WebContextMenuData::CanPaste))
        return false;

      return ui::Clipboard::GetForCurrentThread()->IsFormatAvailable(
          ui::Clipboard::GetPlainTextFormatType(),
          ui::CLIPBOARD_TYPE_COPY_PASTE);
    }

    case IDC_CONTENT_CONTEXT_DELETE:
      return !!(params_.edit_flags & WebContextMenuData::CanDelete);

    case IDC_CONTENT_CONTEXT_SELECTALL:
      return !!(params_.edit_flags & WebContextMenuData::CanSelectAll);

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      return !browser_context_->IsOffTheRecord() &&
             params_.link_url.is_valid() &&
             incognito_avail != IncognitoModePrefs::DISABLED;

    case IDC_PRINT:
      return prefs->GetBoolean(prefs::kPrintingEnabled) &&
             (params_.media_type == WebContextMenuData::MediaTypeNone ||
              params_.media_flags & WebContextMenuData::MediaCanPrint);

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL:
    case IDC_SPELLPANEL_TOGGLE:
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS:
      return true;
    case IDC_CHECK_SPELLING_WHILE_TYPING:
      return prefs->GetBoolean(prefs::kEnableContinuousSpellcheck);

#if !defined(OS_MACOSX) && defined(OS_POSIX)
    // TODO(suzhe): this should not be enabled for password fields.
    case IDC_INPUT_METHODS_MENU:
      return true;
#endif

    case IDC_SPELLCHECK_MENU:
    case IDC_CONTENT_CONTEXT_OPENLINKWITH:
    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS:
    case IDC_CONTENT_CONTEXT_FORCESAVEPASSWORD:
      return true;

    case IDC_ROUTE_MEDIA: {
      if (!media_router::MediaRouterEnabled(browser_context_))
        return false;

      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      if (!browser || browser->profile()->IsOffTheRecord())
        return false;

      // Disable the command if there is an active modal dialog.
      // We don't use |source_web_contents_| here because it could be the
      // WebContents for something that's not the current tab (e.g., WebUI
      // modal dialog).
      WebContents* web_contents =
          browser->tab_strip_model()->GetActiveWebContents();
      if (!web_contents)
        return false;

      const web_modal::WebContentsModalDialogManager* manager =
          web_modal::WebContentsModalDialogManager::FromWebContents(
              web_contents);
      return !manager || !manager->IsDialogActive();
    }

    default:
      NOTREACHED();
      return false;
  }
}

bool RenderViewContextMenu::IsCommandIdChecked(int id) const {
  if (RenderViewContextMenuBase::IsCommandIdChecked(id))
    return true;

  // See if the video is set to looping.
  if (id == IDC_CONTENT_CONTEXT_LOOP)
    return (params_.media_flags & WebContextMenuData::MediaLoop) != 0;

  if (id == IDC_CONTENT_CONTEXT_CONTROLS)
    return (params_.media_flags & WebContextMenuData::MediaControls) != 0;

  // Extension items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id))
    return extension_items_.IsCommandIdChecked(id);

  return false;
}

void RenderViewContextMenu::ExecuteCommand(int id, int event_flags) {
  RenderViewContextMenuBase::ExecuteCommand(id, event_flags);
  if (command_executed_)
    return;
  command_executed_ = true;

  // Process extension menu items.
  if (ContextMenuMatcher::IsExtensionsCustomCommandId(id)) {
    extension_items_.ExecuteCommand(id, source_web_contents_, params_);
    return;
  }

  if (id >= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST &&
      id <= IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_LAST) {
    ProtocolHandlerRegistry::ProtocolHandlerList handlers =
        GetHandlersForLinkUrl();
    if (handlers.empty())
      return;

    content::RecordAction(
        UserMetricsAction("RegisterProtocolHandler.ContextMenu_Open"));
    int handlerIndex = id - IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_FIRST;
    WindowOpenDisposition disposition =
        ForceNewTabDispositionFromEventFlags(event_flags);
    OpenURL(handlers[handlerIndex].TranslateUrl(params_.link_url),
            GetDocumentURL(params_),
            disposition,
            ui::PAGE_TRANSITION_LINK);
    return;
  }

  if (id >= IDC_OPEN_LINK_IN_PROFILE_FIRST &&
      id <= IDC_OPEN_LINK_IN_PROFILE_LAST) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    const ProfileInfoCache& profile_info_cache =
        profile_manager->GetProfileInfoCache();

    base::FilePath profile_path = profile_info_cache.GetPathOfProfileAtIndex(
        id - IDC_OPEN_LINK_IN_PROFILE_FIRST);
    chrome::HostDesktopType desktop_type =
        chrome::GetHostDesktopTypeForNativeView(
            source_web_contents_->GetNativeView());

    Profile* profile = profile_manager->GetProfileByPath(profile_path);
    UmaEnumOpenLinkAsUser profile_state;
    if (chrome::FindLastActiveWithProfile(profile, desktop_type)) {
      profile_state = OPEN_LINK_AS_USER_ACTIVE_PROFILE_ENUM_ID;
    } else if (multiple_profiles_open_) {
      profile_state =
          OPEN_LINK_AS_USER_INACTIVE_PROFILE_MULTI_PROFILE_SESSION_ENUM_ID;
    } else {
      profile_state =
          OPEN_LINK_AS_USER_INACTIVE_PROFILE_SINGLE_PROFILE_SESSION_ENUM_ID;
    }
    UMA_HISTOGRAM_ENUMERATION("RenderViewContextMenu.OpenLinkAsUser",
                              profile_state, OPEN_LINK_AS_USER_LAST_ENUM_ID);

    profiles::SwitchToProfile(
        profile_path, desktop_type, false,
        base::Bind(OnProfileCreated, desktop_type, params_.link_url,
                   CreateReferrer(params_.link_url, params_)),
        ProfileMetrics::SWITCH_PROFILE_CONTEXT_MENU);
    return;
  }

  switch (id) {
    case IDC_CONTENT_CONTEXT_OPENLINKNEWTAB: {
      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      OpenURL(params_.link_url,
              GetDocumentURL(params_),
              !browser || browser->is_app() ?
                  NEW_FOREGROUND_TAB : NEW_BACKGROUND_TAB,
              ui::PAGE_TRANSITION_LINK);
      break;
    }
    case IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW:
      OpenURL(params_.link_url,
              GetDocumentURL(params_),
              NEW_WINDOW,
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD:
      OpenURL(params_.link_url, GURL(), OFF_THE_RECORD,
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_SAVELINKAS: {
      RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
      const GURL& url = params_.link_url;
      content::Referrer referrer = CreateReferrer(url, params_);
      DownloadManager* dlm =
          BrowserContext::GetDownloadManager(browser_context_);
      scoped_ptr<DownloadUrlParameters> dl_params(
          DownloadUrlParameters::FromWebContents(source_web_contents_, url));
      dl_params->set_referrer(referrer);
      dl_params->set_referrer_encoding(params_.frame_charset);
      dl_params->set_suggested_name(params_.suggested_filename);
      dl_params->set_prompt(true);
      dlm->DownloadUrl(std::move(dl_params));
      break;
    }

    case IDC_CONTENT_CONTEXT_SAVEAVAS:
    case IDC_CONTENT_CONTEXT_SAVEIMAGEAS: {
      bool is_large_data_url = params_.has_image_contents &&
          params_.src_url.is_empty();
      if (params_.media_type == WebContextMenuData::MediaTypeCanvas ||
          (params_.media_type == WebContextMenuData::MediaTypeImage &&
              is_large_data_url)) {
        source_web_contents_->GetRenderViewHost()->SaveImageAt(
          params_.x, params_.y);
      } else {
        RecordDownloadSource(DOWNLOAD_INITIATED_BY_CONTEXT_MENU);
        const GURL& url = params_.src_url;
        content::Referrer referrer = CreateReferrer(url, params_);

        std::string headers;
        DataReductionProxyChromeSettings* settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                browser_context_);
        if (params_.media_type == WebContextMenuData::MediaTypeImage &&
            settings && settings->CanUseDataReductionProxy(params_.src_url)) {
          headers = data_reduction_proxy::kDataReductionPassThroughHeader;
        }

        source_web_contents_->SaveFrameWithHeaders(url, referrer, headers);
      }
      break;
    }

    case IDC_CONTENT_CONTEXT_COPYLINKLOCATION:
      WriteURLToClipboard(params_.unfiltered_link_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYLINKTEXT:
      WriteTextToClipboard(params_.link_text);
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGELOCATION:
    case IDC_CONTENT_CONTEXT_COPYAVLOCATION:
      WriteURLToClipboard(params_.src_url);
      break;

    case IDC_CONTENT_CONTEXT_COPYIMAGE:
      CopyImageAt(params_.x, params_.y);
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFORIMAGE:
      GetImageThumbnailForSearch();
      break;

    case IDC_CONTENT_CONTEXT_OPEN_ORIGINAL_IMAGE_NEW_TAB:
      OpenURLWithExtraHeaders(
          params_.src_url, GetDocumentURL(params_), NEW_BACKGROUND_TAB,
          ui::PAGE_TRANSITION_LINK,
          data_reduction_proxy::kDataReductionPassThroughHeader);
      break;

    case IDC_CONTENT_CONTEXT_LOAD_ORIGINAL_IMAGE:
      LoadOriginalImage();
      break;

    case IDC_CONTENT_CONTEXT_OPENIMAGENEWTAB:
    case IDC_CONTENT_CONTEXT_OPENAVNEWTAB:
      OpenURL(params_.src_url,
              GetDocumentURL(params_),
              NEW_BACKGROUND_TAB,
              ui::PAGE_TRANSITION_LINK);
      break;

    case IDC_CONTENT_CONTEXT_PLAYPAUSE: {
      bool play = !!(params_.media_flags & WebContextMenuData::MediaPaused);
      if (play) {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Play"));
      } else {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Pause"));
      }
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Play, play));
      break;
    }

    case IDC_CONTENT_CONTEXT_MUTE: {
      bool mute = !(params_.media_flags & WebContextMenuData::MediaMuted);
      if (mute) {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Mute"));
      } else {
        content::RecordAction(UserMetricsAction("MediaContextMenu_Unmute"));
      }
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Mute, mute));
      break;
    }

    case IDC_CONTENT_CONTEXT_LOOP:
      content::RecordAction(UserMetricsAction("MediaContextMenu_Loop"));
      MediaPlayerActionAt(gfx::Point(params_.x, params_.y),
                          WebMediaPlayerAction(
                              WebMediaPlayerAction::Loop,
                              !IsCommandIdChecked(IDC_CONTENT_CONTEXT_LOOP)));
      break;

    case IDC_CONTENT_CONTEXT_CONTROLS:
      content::RecordAction(UserMetricsAction("MediaContextMenu_Controls"));
      MediaPlayerActionAt(
          gfx::Point(params_.x, params_.y),
          WebMediaPlayerAction(
              WebMediaPlayerAction::Controls,
              !IsCommandIdChecked(IDC_CONTENT_CONTEXT_CONTROLS)));
      break;

    case IDC_CONTENT_CONTEXT_ROTATECW:
      content::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateClockwise"));
      PluginActionAt(
          gfx::Point(params_.x, params_.y),
          WebPluginAction(WebPluginAction::Rotate90Clockwise, true));
      break;

    case IDC_CONTENT_CONTEXT_ROTATECCW:
      content::RecordAction(
      UserMetricsAction("PluginContextMenu_RotateCounterclockwise"));
      PluginActionAt(
          gfx::Point(params_.x, params_.y),
          WebPluginAction(WebPluginAction::Rotate90Counterclockwise, true));
      break;

    case IDC_BACK:
      embedder_web_contents_->GetController().GoBack();
      break;

    case IDC_FORWARD:
      embedder_web_contents_->GetController().GoForward();
      break;

    case IDC_SAVE_PAGE:
      embedder_web_contents_->OnSavePage();
      break;

    case IDC_RELOAD:
      embedder_web_contents_->GetController().Reload(true);
      break;

    case IDC_CONTENT_CONTEXT_RELOAD_PACKAGED_APP: {
      const Extension* platform_app = GetExtension();
      DCHECK(platform_app);
      DCHECK(platform_app->is_platform_app());

      extensions::ExtensionSystem::Get(browser_context_)
          ->extension_service()
          ->ReloadExtension(platform_app->id());
      break;
    }

    case IDC_CONTENT_CONTEXT_RESTART_PACKAGED_APP: {
      const Extension* platform_app = GetExtension();
      DCHECK(platform_app);
      DCHECK(platform_app->is_platform_app());

      apps::AppLoadService::Get(GetProfile())
          ->RestartApplication(platform_app->id());
      break;
    }

    case IDC_PRINT: {
#if defined(ENABLE_PRINTING)
      if (params_.media_type != WebContextMenuData::MediaTypeNone) {
        RenderFrameHost* render_frame_host = GetRenderFrameHost();
        if (render_frame_host) {
          render_frame_host->Send(new PrintMsg_PrintNodeUnderContextMenu(
              render_frame_host->GetRoutingID()));
        }
        break;
      }

      printing::StartPrint(
          source_web_contents_,
          GetPrefs(browser_context_)->GetBoolean(prefs::kPrintPreviewDisabled),
          !params_.selection_text.empty());
#endif  // defined(ENABLE_PRINTING)
      break;
    }

    case IDC_ROUTE_MEDIA: {
#if defined(ENABLE_MEDIA_ROUTER)
      if (!media_router::MediaRouterEnabled(browser_context_))
        return;

      Browser* browser =
          chrome::FindBrowserWithWebContents(source_web_contents_);
      DCHECK(browser && !browser->profile()->IsOffTheRecord());

      media_router::MediaRouterDialogController* dialog_controller =
          media_router::MediaRouterDialogController::GetOrCreateForWebContents(
              source_web_contents_);
      if (!dialog_controller)
        return;

      dialog_controller->ShowMediaRouterDialog();
      media_router::MediaRouterMetrics::RecordMediaRouterDialogOrigin(
          media_router::MediaRouterDialogOpenOrigin::CONTEXTUAL_MENU);
#endif  // defined(ENABLE_MEDIA_ROUTER)
      break;
    }

    case IDC_VIEW_SOURCE:
      embedder_web_contents_->ViewSource();
      break;

    case IDC_CONTENT_CONTEXT_INSPECTELEMENT:
      Inspect(params_.x, params_.y);
      break;

    case IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE: {
      const Extension* platform_app = GetExtension();
      DCHECK(platform_app);
      DCHECK(platform_app->is_platform_app());

      extensions::devtools_util::InspectBackgroundPage(platform_app,
                                                       GetProfile());
      break;
    }

    case IDC_CONTENT_CONTEXT_VIEWPAGEINFO: {
      NavigationController* controller =
          &embedder_web_contents_->GetController();
      // Important to use GetVisibleEntry to match what's showing in the
      // omnibox.  This may return null.
      NavigationEntry* nav_entry = controller->GetVisibleEntry();
      if (!nav_entry)
        return;
      Browser* browser =
          chrome::FindBrowserWithWebContents(embedder_web_contents_);
      ChromeSecurityStateModelClient* security_model_client =
          ChromeSecurityStateModelClient::FromWebContents(
              embedder_web_contents_);
      DCHECK(security_model_client);
      chrome::ShowWebsiteSettings(browser, embedder_web_contents_,
                                  nav_entry->GetURL(),
                                  security_model_client->GetSecurityInfo());
      break;
    }
    case IDC_CONTENT_CONTEXT_TRANSLATE: {
#if 0
      // A translation might have been triggered by the time the menu got
      // selected, do nothing in that case.
      ChromeTranslateClient* chrome_translate_client =
          ChromeTranslateClient::FromWebContents(embedder_web_contents_);
      if (!chrome_translate_client ||
          chrome_translate_client->GetLanguageState().IsPageTranslated() ||
          chrome_translate_client->GetLanguageState().translation_pending()) {
        return;
      }
      std::string original_lang =
          chrome_translate_client->GetLanguageState().original_language();
      std::string target_lang = g_browser_process->GetApplicationLocale();
      target_lang =
          translate::TranslateDownloadManager::GetLanguageCode(target_lang);
      // Since the user decided to translate for that language and site, clears
      // any preferences for not translating them.
      scoped_ptr<translate::TranslatePrefs> prefs(
          ChromeTranslateClient::CreateTranslatePrefs(
              GetPrefs(browser_context_)));
      prefs->UnblockLanguage(original_lang);
      prefs->RemoveSiteFromBlacklist(params_.page_url.HostNoBrackets());
      translate::TranslateManager* manager =
          chrome_translate_client->GetTranslateManager();
      DCHECK(manager);
      manager->TranslatePage(original_lang, target_lang, true);
#endif
      break;
    }
    case IDC_CONTENT_CONTEXT_RELOADFRAME:
      // We always obey the cache here.
      // TODO(evanm): Perhaps we could allow shift-clicking the menu item to do
      // a cache-ignoring reload of the frame.
      source_web_contents_->ReloadFocusedFrame(false);
      break;

    case IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE:
      source_web_contents_->ViewFrameSource(params_.frame_url,
                                            params_.frame_page_state);
      break;

    case IDC_CONTENT_CONTEXT_UNDO:
      source_web_contents_->Undo();
      break;

    case IDC_CONTENT_CONTEXT_REDO:
      source_web_contents_->Redo();
      break;

    case IDC_CONTENT_CONTEXT_CUT:
      source_web_contents_->Cut();
      break;

    case IDC_CONTENT_CONTEXT_COPY:
      source_web_contents_->Copy();
      break;

    case IDC_CONTENT_CONTEXT_PASTE:
      source_web_contents_->Paste();
      break;

    case IDC_CONTENT_CONTEXT_PASTE_AND_MATCH_STYLE:
      source_web_contents_->PasteAndMatchStyle();
      break;

    case IDC_CONTENT_CONTEXT_DELETE:
      source_web_contents_->Delete();
      break;

    case IDC_CONTENT_CONTEXT_SELECTALL:
      source_web_contents_->SelectAll();
      break;

    case IDC_CONTENT_CONTEXT_SEARCHWEBFOR:
    case IDC_CONTENT_CONTEXT_GOTOURL: {
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      OpenURL(selection_navigation_url_, GURL(), disposition,
              ui::PAGE_TRANSITION_LINK);
      break;
    }
    case IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS: {
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      GURL url = chrome::GetSettingsUrl(chrome::kLanguageOptionsSubPage);
      OpenURL(url, GURL(), disposition, ui::PAGE_TRANSITION_LINK);
      break;
    }

    case IDC_CONTENT_CONTEXT_PROTOCOL_HANDLER_SETTINGS: {
      content::RecordAction(
          UserMetricsAction("RegisterProtocolHandler.ContextMenu_Settings"));
      WindowOpenDisposition disposition =
          ForceNewTabDispositionFromEventFlags(event_flags);
      GURL url = chrome::GetSettingsUrl(chrome::kHandlerSettingsSubPage);
      OpenURL(url, GURL(), disposition, ui::PAGE_TRANSITION_LINK);
      break;
    }

    case IDC_CONTENT_CONTEXT_FORCESAVEPASSWORD:
      ChromePasswordManagerClient::FromWebContents(source_web_contents_)->
          ForceSavePassword();
      break;

    default:
      NOTREACHED();
      break;
  }
}

ProtocolHandlerRegistry::ProtocolHandlerList
    RenderViewContextMenu::GetHandlersForLinkUrl() {
  ProtocolHandlerRegistry::ProtocolHandlerList handlers =
      protocol_handler_registry_->GetHandlersFor(params_.link_url.scheme());
  std::sort(handlers.begin(), handlers.end());
  return handlers;
}

void RenderViewContextMenu::NotifyMenuShown() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
      content::Source<RenderViewContextMenu>(this),
      content::NotificationService::NoDetails());
}

void RenderViewContextMenu::NotifyURLOpened(
    const GURL& url,
    content::WebContents* new_contents) {
  RetargetingDetails details;
  details.source_web_contents = source_web_contents_;
  // Don't use GetRenderFrameHost() as it may be NULL. crbug.com/399789
  details.source_render_frame_id = render_frame_id_;
  details.target_url = url;
  details.target_web_contents = new_contents;
  details.not_yet_in_tabstrip = false;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_RETARGETING,
      content::Source<Profile>(GetProfile()),
      content::Details<RetargetingDetails>(&details));
}

bool RenderViewContextMenu::IsDevCommandEnabled(int id) const {
  if (id == IDC_CONTENT_CONTEXT_INSPECTELEMENT ||
      id == IDC_CONTENT_CONTEXT_INSPECTBACKGROUNDPAGE) {
    if (!GetPrefs(browser_context_)
             ->GetBoolean(prefs::kWebKitJavascriptEnabled))
      return false;

    // Don't enable the web inspector if the developer tools are disabled via
    // the preference dev-tools-disabled.
    if (GetPrefs(browser_context_)->GetBoolean(prefs::kDevToolsDisabled))
      return false;
  }

  return true;
}

base::string16 RenderViewContextMenu::PrintableSelectionText() {
  return gfx::TruncateString(params_.selection_text,
                             kMaxSelectionTextLength,
                             gfx::WORD_BREAK);
}

void RenderViewContextMenu::EscapeAmpersands(base::string16* text) {
  base::ReplaceChars(*text, base::ASCIIToUTF16("&"), base::ASCIIToUTF16("&&"),
                     text);
}

// Controller functions --------------------------------------------------------

void RenderViewContextMenu::CopyImageAt(int x, int y) {
  source_web_contents_->GetRenderViewHost()->CopyImageAt(x, y);
}

void RenderViewContextMenu::LoadOriginalImage() {
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  render_frame_host->Send(new ChromeViewMsg_RequestReloadImageForContextNode(
      render_frame_host->GetRoutingID()));
}

void RenderViewContextMenu::GetImageThumbnailForSearch() {
  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(source_web_contents_);
  if (!core_tab_helper)
    return;
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  core_tab_helper->SearchByImageInNewTab(render_frame_host, params().src_url);
}

void RenderViewContextMenu::Inspect(int x, int y) {
  content::RecordAction(UserMetricsAction("DevTools_InspectElement"));
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (!render_frame_host)
    return;
  DevToolsWindow::InspectElement(render_frame_host, x, y);
}

void RenderViewContextMenu::WriteURLToClipboard(const GURL& url) {
  ::WriteURLToClipboard(
      url, GetPrefs(browser_context_)->GetString(prefs::kAcceptLanguages));
}

void RenderViewContextMenu::MediaPlayerActionAt(
    const gfx::Point& location,
    const WebMediaPlayerAction& action) {
  source_web_contents_->GetRenderViewHost()->
      ExecuteMediaPlayerActionAtLocation(location, action);
}

void RenderViewContextMenu::PluginActionAt(
    const gfx::Point& location,
    const WebPluginAction& action) {
  source_web_contents_->GetRenderViewHost()->
      ExecutePluginActionAtLocation(location, action);
}
