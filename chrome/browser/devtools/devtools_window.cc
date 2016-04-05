// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/task_management/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/syncable_prefs/pref_service_syncable.h"
#include "components/ui/zoom/page_zoom.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "net/base/escape.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::DictionaryValue;
using blink::WebInputEvent;
using content::BrowserThread;
using content::DevToolsAgentHost;
using content::WebContents;

namespace {

typedef std::vector<DevToolsWindow*> DevToolsWindows;
base::LazyInstance<DevToolsWindows>::Leaky g_instances =
    LAZY_INSTANCE_INITIALIZER;

static const char kKeyUpEventName[] = "keyup";
static const char kKeyDownEventName[] = "keydown";

bool FindInspectedBrowserAndTabIndex(
    WebContents* inspected_web_contents, Browser** browser, int* tab) {
  if (!inspected_web_contents)
    return false;

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    int tab_index = it->tab_strip_model()->GetIndexOfWebContents(
        inspected_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      *browser = *it;
      *tab = tab_index;
      return true;
    }
  }
  return false;
}

// DevToolsToolboxDelegate ----------------------------------------------------

class DevToolsToolboxDelegate
    : public content::WebContentsObserver,
      public content::WebContentsDelegate {
 public:
  DevToolsToolboxDelegate(
      WebContents* toolbox_contents,
      DevToolsWindow::ObserverWithAccessor* web_contents_observer);
  ~DevToolsToolboxDelegate() override;

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  bool PreHandleKeyboardEvent(content::WebContents* source,
                              const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) override;
  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override;
  void WebContentsDestroyed() override;

 private:
  BrowserWindow* GetInspectedBrowserWindow();
  DevToolsWindow::ObserverWithAccessor* inspected_contents_observer_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsToolboxDelegate);
};

DevToolsToolboxDelegate::DevToolsToolboxDelegate(
    WebContents* toolbox_contents,
    DevToolsWindow::ObserverWithAccessor* web_contents_observer)
    : WebContentsObserver(toolbox_contents),
      inspected_contents_observer_(web_contents_observer) {
}

DevToolsToolboxDelegate::~DevToolsToolboxDelegate() {
}

content::WebContents* DevToolsToolboxDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == web_contents());
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme))
    return NULL;
  content::NavigationController::LoadURLParams load_url_params(params.url);
  source->GetController().LoadURLWithParams(load_url_params);
  return source;
}

bool DevToolsToolboxDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    return window->PreHandleKeyboardEvent(event, is_keyboard_shortcut);
  return false;
}

void DevToolsToolboxDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windowsKeyCode == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return;
  }
  BrowserWindow* window = GetInspectedBrowserWindow();
  if (window)
    window->HandleKeyboardEvent(event);
}

void DevToolsToolboxDelegate::WebContentsDestroyed() {
  delete this;
}

BrowserWindow* DevToolsToolboxDelegate::GetInspectedBrowserWindow() {
  WebContents* inspected_contents =
      inspected_contents_observer_->web_contents();
  if (!inspected_contents)
    return NULL;
  Browser* browser = NULL;
  int tab = 0;
  if (FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return browser->window();
  return NULL;
}

// static
GURL DecorateFrontendURL(const GURL& base_url) {
  std::string frontend_url = base_url.spec();
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
      "dockSide=undocked"); // TODO(dgozman): remove this support in M38.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDevToolsExperiments))
    url_string += "&experiments=true";
#if defined(DEBUG_DEVTOOLS)
  url_string += "&debugFrontend=true";
#endif  // defined(DEBUG_DEVTOOLS)
  return GURL(url_string);
}

}  // namespace

// DevToolsEventForwarder -----------------------------------------------------

class DevToolsEventForwarder {
 public:
  explicit DevToolsEventForwarder(DevToolsWindow* window)
     : devtools_window_(window) {}

  // Registers whitelisted shortcuts with the forwarder.
  // Only registered keys will be forwarded to the DevTools frontend.
  void SetWhitelistedShortcuts(const std::string& message);

  // Forwards a keyboard event to the DevTools frontend if it is whitelisted.
  // Returns |true| if the event has been forwarded, |false| otherwise.
  bool ForwardEvent(const content::NativeWebKeyboardEvent& event);

 private:
  static bool KeyWhitelistingAllowed(int key_code, int modifiers);
  static int CombineKeyCodeAndModifiers(int key_code, int modifiers);

  DevToolsWindow* devtools_window_;
  std::set<int> whitelisted_keys_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsEventForwarder);
};

void DevToolsEventForwarder::SetWhitelistedShortcuts(
    const std::string& message) {
  scoped_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  base::ListValue* shortcut_list;
  if (!parsed_message->GetAsList(&shortcut_list))
      return;
  base::ListValue::iterator it = shortcut_list->begin();
  for (; it != shortcut_list->end(); ++it) {
    base::DictionaryValue* dictionary;
    if (!(*it)->GetAsDictionary(&dictionary))
      continue;
    int key_code = 0;
    dictionary->GetInteger("keyCode", &key_code);
    if (key_code == 0)
      continue;
    int modifiers = 0;
    dictionary->GetInteger("modifiers", &modifiers);
    if (!KeyWhitelistingAllowed(key_code, modifiers)) {
      LOG(WARNING) << "Key whitelisting forbidden: "
                   << "(" << key_code << "," << modifiers << ")";
      continue;
    }
    whitelisted_keys_.insert(CombineKeyCodeAndModifiers(key_code, modifiers));
  }
}

bool DevToolsEventForwarder::ForwardEvent(
    const content::NativeWebKeyboardEvent& event) {
  std::string event_type;
  switch (event.type) {
    case WebInputEvent::KeyDown:
    case WebInputEvent::RawKeyDown:
      event_type = kKeyDownEventName;
      break;
    case WebInputEvent::KeyUp:
      event_type = kKeyUpEventName;
      break;
    default:
      return false;
  }

  int key_code = ui::LocatedToNonLocatedKeyboardCode(
      static_cast<ui::KeyboardCode>(event.windowsKeyCode));
  int modifiers = event.modifiers & (WebInputEvent::ShiftKey |
                                     WebInputEvent::ControlKey |
                                     WebInputEvent::AltKey |
                                     WebInputEvent::MetaKey);
  int key = CombineKeyCodeAndModifiers(key_code, modifiers);
  if (whitelisted_keys_.find(key) == whitelisted_keys_.end())
    return false;

  base::DictionaryValue event_data;
  event_data.SetString("type", event_type);
  event_data.SetString("keyIdentifier", event.keyIdentifier);
  event_data.SetInteger("keyCode", key_code);
  event_data.SetInteger("modifiers", modifiers);
  devtools_window_->bindings_->CallClientFunction(
      "DevToolsAPI.keyEventUnhandled", &event_data, NULL, NULL);
  return true;
}

int DevToolsEventForwarder::CombineKeyCodeAndModifiers(int key_code,
                                                       int modifiers) {
  return key_code | (modifiers << 16);
}

bool DevToolsEventForwarder::KeyWhitelistingAllowed(int key_code,
                                                    int modifiers) {
  return (ui::VKEY_F1 <= key_code && key_code <= ui::VKEY_F12) ||
      modifiers != 0;
}

// DevToolsWindow::ObserverWithAccessor -------------------------------

DevToolsWindow::ObserverWithAccessor::ObserverWithAccessor(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

DevToolsWindow::ObserverWithAccessor::~ObserverWithAccessor() {
}

// DevToolsWindow -------------------------------------------------------------

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

DevToolsWindow::~DevToolsWindow() {
  life_stage_ = kClosing;

  UpdateBrowserWindow();
  UpdateBrowserToolbar();

  if (toolbox_web_contents_)
    delete toolbox_web_contents_;

  DevToolsWindows* instances = g_instances.Pointer();
  DevToolsWindows::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);

  if (!close_callback_.is_null()) {
    close_callback_.Run();
    close_callback_ = base::Closure();
  }
}

// static
void DevToolsWindow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDevToolsEditedFiles);
  registry->RegisterDictionaryPref(prefs::kDevToolsFileSystemPaths);
  registry->RegisterStringPref(prefs::kDevToolsAdbKey, std::string());

  registry->RegisterBooleanPref(prefs::kDevToolsDiscoverUsbDevicesEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kDevToolsPortForwardingEnabled, false);
  registry->RegisterBooleanPref(prefs::kDevToolsPortForwardingDefaultSet,
                                false);
  registry->RegisterDictionaryPref(prefs::kDevToolsPortForwardingConfig);
  registry->RegisterDictionaryPref(prefs::kDevToolsPreferences);
}

// static
content::WebContents* DevToolsWindow::GetInTabWebContents(
    WebContents* inspected_web_contents,
    DevToolsContentsResizingStrategy* out_strategy) {
  DevToolsWindow* window = GetInstanceForInspectedWebContents(
      inspected_web_contents);
  if (!window || window->life_stage_ == kClosing)
    return NULL;

  // Not yet loaded window is treated as docked, but we should not present it
  // until we decided on docking.
  bool is_docked_set = window->life_stage_ == kLoadCompleted ||
      window->life_stage_ == kIsDockedSet;
  if (!is_docked_set)
    return NULL;

  // Undocked window should have toolbox web contents.
  if (!window->is_docked_ && !window->toolbox_web_contents_)
    return NULL;

  if (out_strategy)
    out_strategy->CopyFrom(window->contents_resizing_strategy_);

  return window->is_docked_ ? window->main_web_contents_ :
      window->toolbox_web_contents_;
}

// static
DevToolsWindow* DevToolsWindow::GetInstanceForInspectedWebContents(
    WebContents* inspected_web_contents) {
  if (!inspected_web_contents || g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->GetInspectedWebContents() == inspected_web_contents)
      return *it;
  }
  return NULL;
}

// static
bool DevToolsWindow::IsDevToolsWindow(content::WebContents* web_contents) {
  if (!web_contents || g_instances == NULL)
    return false;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->main_web_contents_ == web_contents ||
        (*it)->toolbox_web_contents_ == web_contents)
      return true;
  }
  return false;
}

// static
void DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    const scoped_refptr<DevToolsAgentHost>& worker_agent) {
  DevToolsWindow* window = FindDevToolsWindow(worker_agent.get());
  if (!window) {
    window = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    if (!window)
      return;
    window->bindings_->AttachTo(worker_agent);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindowForWorker(
    Profile* profile) {
  content::RecordAction(base::UserMetricsAction("DevTools_InspectWorker"));
  return Create(profile, GURL(), NULL, true, std::string(), false, "");
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents) {
  ToggleDevToolsWindow(
        inspected_web_contents, true, DevToolsToggleAction::Show(), "");
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    content::WebContents* inspected_web_contents,
    const DevToolsToggleAction& action) {
  ToggleDevToolsWindow(inspected_web_contents, true, action, "");
}

// static
void DevToolsWindow::OpenDevToolsWindow(
    Profile* profile,
    const scoped_refptr<content::DevToolsAgentHost>& agent_host) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host.get());
  if (!window) {
    window = DevToolsWindow::Create(
        profile, GURL(), nullptr, false, std::string(), false, std::string());
    if (!window)
      return;
    window->bindings_->AttachTo(agent_host);
  }
  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
void DevToolsWindow::ToggleDevToolsWindow(
    Browser* browser,
    const DevToolsToggleAction& action) {
  if (action.type() == DevToolsToggleAction::kToggle &&
      browser->is_devtools()) {
    browser->tab_strip_model()->CloseAllTabs();
    return;
  }

  ToggleDevToolsWindow(
      browser->tab_strip_model()->GetActiveWebContents(),
      action.type() == DevToolsToggleAction::kInspect,
      action, "");
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    const scoped_refptr<content::DevToolsAgentHost>& agent_host,
    bool isWorker) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host.get());
  if (!window) {
    window = Create(profile, GURL(), nullptr, isWorker,
        DevToolsUI::GetProxyURL(frontend_url).spec(), false, std::string());
    if (!window)
      return;
    window->bindings_->AttachTo(agent_host);
  }

  window->ScheduleShow(DevToolsToggleAction::Show());
}

// static
void DevToolsWindow::ToggleDevToolsWindow(
    content::WebContents* inspected_web_contents,
    bool force_open,
    const DevToolsToggleAction& action,
    const std::string& settings) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_web_contents));
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_web_contents->GetBrowserContext());
    content::RecordAction(
        base::UserMetricsAction("DevTools_InspectRenderer"));
    window = Create(profile, GURL(), inspected_web_contents,
                    false, std::string(), true, settings);
    if (!window)
      return;
    window->bindings_->AttachTo(agent.get());
    do_open = true;
  }

  // Update toolbar to reflect DevTools changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it.
  if (!window->is_docked_ || do_open)
    window->ScheduleShow(action);
  else
    window->CloseWindow();
}

// static
void DevToolsWindow::InspectElement(
    content::RenderFrameHost* inspected_frame_host,
    int x,
    int y) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_frame_host));
  bool should_measure_time = FindDevToolsWindow(agent.get()) == NULL;
  base::TimeTicks start_time = base::TimeTicks::Now();
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  if (agent->GetType() == content::DevToolsAgentHost::TYPE_WEB_CONTENTS) {
    OpenDevToolsWindow(agent->GetWebContents());
  } else {
    OpenDevToolsWindow(Profile::FromBrowserContext(agent->GetBrowserContext()),
                       agent);
  }

  agent->InspectElement(x, y);

  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  if (should_measure_time && window)
    window->inspect_element_start_time_ = start_time;
}

// static
content::DevToolsExternalAgentProxyDelegate*
DevToolsWindow::CreateWebSocketAPIChannel(const std::string& path) {
  if (path.find("/devtools/frontend_api") != 0)
    return nullptr;

  return DevToolsUIBindings::CreateWebSocketAPIChannel();
}

void DevToolsWindow::ScheduleShow(const DevToolsToggleAction& action) {
  if (life_stage_ == kLoadCompleted) {
    Show(action);
    return;
  }

  // Action will be done only after load completed.
  action_on_load_ = action;

  if (!can_dock_) {
    // No harm to show always-undocked window right away.
    is_docked_ = false;
    Show(DevToolsToggleAction::Show());
  }
}

void DevToolsWindow::Show(const DevToolsToggleAction& action) {
  if (life_stage_ == kClosing)
    return;

  if (action.type() == DevToolsToggleAction::kNoOp)
    return;

  if (is_docked_) {
    DCHECK(can_dock_);
    Browser* inspected_browser = NULL;
    int inspected_tab_index = -1;
    FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                    &inspected_browser,
                                    &inspected_tab_index);
    DCHECK(inspected_browser);
    DCHECK(inspected_tab_index != -1);

    // Tell inspected browser to update splitter and switch to inspected panel.
    BrowserWindow* inspected_window = inspected_browser->window();
    main_web_contents_->SetDelegate(this);

    TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
    tab_strip_model->ActivateTabAt(inspected_tab_index, true);

    inspected_window->UpdateDevTools();
    main_web_contents_->SetInitialFocus();
    inspected_window->Show();
    // On Aura, focusing once is not enough. Do it again.
    // Note that focusing only here but not before isn't enough either. We just
    // need to focus twice.
    main_web_contents_->SetInitialFocus();

    PrefsTabHelper::CreateForWebContents(main_web_contents_);
    main_web_contents_->GetRenderViewHost()->SyncRendererPrefs();

    DoAction(action);
    return;
  }

  // Avoid consecutive window switching if the devtools window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || (action.type() != DevToolsToggleAction::kInspect);

  should_show_window = should_show_window && !headless_;

  if (!browser_)
    CreateDevToolsBrowser();

  if (should_show_window) {
    browser_->window()->Show();
    main_web_contents_->SetInitialFocus();
  }
  if (toolbox_web_contents_)
    UpdateBrowserWindow();

  DoAction(action);
}

// static
bool DevToolsWindow::HandleBeforeUnload(WebContents* frontend_contents,
    bool proceed, bool* proceed_to_fire_unload) {
  DevToolsWindow* window = AsDevToolsWindow(frontend_contents);
  if (!window)
    return false;
  if (!window->intercepted_page_beforeunload_)
    return false;
  window->BeforeUnloadFired(frontend_contents, proceed,
      proceed_to_fire_unload);
  return true;
}

// static
bool DevToolsWindow::InterceptPageBeforeUnload(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window || window->intercepted_page_beforeunload_)
    return false;

  // Not yet loaded frontend will not handle beforeunload.
  if (window->life_stage_ != kLoadCompleted)
    return false;

  window->intercepted_page_beforeunload_ = true;
  // Handle case of devtools inspecting another devtools instance by passing
  // the call up to the inspecting devtools instance.
  if (!DevToolsWindow::InterceptPageBeforeUnload(window->main_web_contents_)) {
    window->main_web_contents_->DispatchBeforeUnload(false);
  }
  return true;
}

// static
bool DevToolsWindow::NeedsToInterceptBeforeUnload(
    WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  return window && !window->intercepted_page_beforeunload_;
}

// static
bool DevToolsWindow::HasFiredBeforeUnloadEventForDevToolsBrowser(
    Browser* browser) {
  DCHECK(browser->is_devtools());
  // When FastUnloadController is used, devtools frontend will be detached
  // from the browser window at this point which means we've already fired
  // beforeunload.
  if (browser->tab_strip_model()->empty())
    return true;
  WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  DevToolsWindow* window = AsDevToolsWindow(contents);
  if (!window)
    return false;
  return window->intercepted_page_beforeunload_;
}

// static
void DevToolsWindow::OnPageCloseCanceled(WebContents* contents) {
  DevToolsWindow* window =
      DevToolsWindow::GetInstanceForInspectedWebContents(contents);
  if (!window)
    return;
  window->intercepted_page_beforeunload_ = false;
  // Propagate to devtools opened on devtools if any.
  DevToolsWindow::OnPageCloseCanceled(window->main_web_contents_);
}

DevToolsWindow::DevToolsWindow(Profile* profile,
                               WebContents* main_web_contents,
                               DevToolsUIBindings* bindings,
                               WebContents* inspected_web_contents,
                               bool can_dock,
                               bool headless)
    : profile_(profile),
      main_web_contents_(main_web_contents),
      toolbox_web_contents_(nullptr),
      bindings_(bindings),
      browser_(nullptr),
      is_docked_(true),
      can_dock_(can_dock),
      headless_(headless),
      // This initialization allows external front-end to work without changes.
      // We don't wait for docking call, but instead immediately show undocked.
      // Passing "dockSide=undocked" parameter ensures proper UI.
      life_stage_(can_dock ? kNotLoaded : kIsDockedSet),
      action_on_load_(DevToolsToggleAction::NoOp()),
      intercepted_page_beforeunload_(false) {
  // Set up delegate, so we get fully-functional window immediately.
  // It will not appear in UI though until |life_stage_ == kLoadCompleted|.
  main_web_contents_->SetDelegate(this);
  // Bindings take ownership over devtools as its delegate.
  bindings_->SetDelegate(this);
  // DevTools uses PageZoom::Zoom(), so main_web_contents_ requires a
  // ZoomController.
  ui_zoom::ZoomController::CreateForWebContents(main_web_contents_);
  ui_zoom::ZoomController::FromWebContents(main_web_contents_)
      ->SetShowsNotificationBubble(false);

  g_instances.Get().push_back(this);

  // There is no inspected_web_contents in case of various workers.
  if (inspected_web_contents)
    inspected_contents_observer_.reset(
        new ObserverWithAccessor(inspected_web_contents));

  // Initialize docked page to be of the right size.
  if (can_dock_ && inspected_web_contents) {
    content::RenderWidgetHostView* inspected_view =
        inspected_web_contents->GetRenderWidgetHostView();
    if (inspected_view && main_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size = inspected_view->GetViewBounds().size();
      main_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
  }

  event_forwarder_.reset(new DevToolsEventForwarder(this));

  // Tag the DevTools main WebContents with its TaskManager specific UserData
  // so that it shows up in the task manager.
  task_management::WebContentsTags::CreateForDevToolsContents(
      main_web_contents_);
}

// static
DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    const GURL& frontend_url,
    content::WebContents* inspected_web_contents,
    bool shared_worker_frontend,
    const std::string& remote_frontend,
    bool can_dock,
    const std::string& settings,
    content::WebContents* cdt_web_contents) {
  if (profile->GetPrefs()->GetBoolean(prefs::kDevToolsDisabled) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return nullptr;

  if (inspected_web_contents) {
    // Check for a place to dock.
    Browser* browser = NULL;
    int tab;
    if (!FindInspectedBrowserAndTabIndex(inspected_web_contents,
                                         &browser, &tab) ||
        browser->is_type_popup()) {
      can_dock = false;
    }
  }

  // Create WebContents with devtools.
  GURL url(GetDevToolsURL(profile, frontend_url,
                          shared_worker_frontend,
                          remote_frontend,
                          can_dock, settings));

  if (cdt_web_contents) {
    cdt_web_contents->GetController().LoadURL(
      DecorateFrontendURL(url), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
    DevToolsUIBindings* bindings =
      DevToolsUIBindings::ForWebContents(cdt_web_contents);
    if (!bindings)
      return nullptr;

    return new DevToolsWindow(profile, cdt_web_contents, bindings,
                              inspected_web_contents, can_dock, true);
  }
  scoped_ptr<WebContents> main_web_contents(
      WebContents::Create(WebContents::CreateParams(profile)));
  main_web_contents->GetController().LoadURL(
      DecorateFrontendURL(url), content::Referrer(),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());
  DevToolsUIBindings* bindings =
      DevToolsUIBindings::ForWebContents(main_web_contents.get());
  if (!bindings)
    return nullptr;

  return new DevToolsWindow(profile, main_web_contents.release(), bindings,
                            inspected_web_contents, can_dock);
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    const GURL& base_url,
                                    bool shared_worker_frontend,
                                    const std::string& remote_frontend,
                                    bool can_dock,
                                    const std::string& settings) {
  // Compatibility errors are encoded with data urls, pass them
  // through with no decoration.
  if (base_url.SchemeIs("data"))
    return base_url;

  std::string frontend_url(
      !remote_frontend.empty() ?
          remote_frontend :
          base_url.is_empty() ? chrome::kChromeUIDevToolsURL : base_url.spec());
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&"));
  if (shared_worker_frontend)
    url_string += "&isSharedWorker=true";
  if (remote_frontend.size()) {
    url_string += "&remoteFrontend=true";
  } else {
    url_string += "&remoteBase=" + DevToolsUI::GetRemoteBaseURL().spec();
  }
  if (can_dock)
    url_string += "&can_dock=true";
  if (settings.size())
    url_string += "&settings=" + settings;
  return GURL(url_string);
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  if (!agent_host || g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->bindings_->IsAttachedTo(agent_host))
      return *it;
  }
  return NULL;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    content::WebContents* web_contents) {
  if (!web_contents || g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = g_instances.Pointer();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->main_web_contents_ == web_contents)
      return *it;
  }
  return NULL;
}

WebContents* DevToolsWindow::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(source == main_web_contents_);
  if (!params.url.SchemeIs(content::kChromeDevToolsScheme)) {
    WebContents* inspected_web_contents = GetInspectedWebContents();
    return inspected_web_contents ?
        inspected_web_contents->OpenURL(params) : NULL;
  }

  bindings_->Reattach();

  content::NavigationController::LoadURLParams load_url_params(params.url);
  main_web_contents_->GetController().LoadURLWithParams(load_url_params);
  return main_web_contents_;
}

void DevToolsWindow::ShowCertificateViewer(int certificate_id) {
  WebContents* inspected_contents = is_docked_ ?
      GetInspectedWebContents() : main_web_contents_;
  Browser* browser = NULL;
  int tab = 0;
  if (!FindInspectedBrowserAndTabIndex(inspected_contents, &browser, &tab))
    return;
  gfx::NativeWindow parent = browser->window()->GetNativeWindow();
  ::ShowCertificateViewerByID(inspected_contents, parent, certificate_id);
}

void DevToolsWindow::ActivateContents(WebContents* contents) {
  if (is_docked_) {
    WebContents* inspected_tab = GetInspectedWebContents();
    if (inspected_tab)
      inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else if (browser_) {
    browser_->window()->Activate();
  }
}

void DevToolsWindow::AddNewContents(WebContents* source,
                                    WebContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_rect,
                                    bool user_gesture,
                                    bool* was_blocked) {
  if (new_contents == toolbox_web_contents_) {
    toolbox_web_contents_->SetDelegate(
        new DevToolsToolboxDelegate(toolbox_web_contents_,
                                    inspected_contents_observer_.get()));
    if (main_web_contents_->GetRenderWidgetHostView() &&
        toolbox_web_contents_->GetRenderWidgetHostView()) {
      gfx::Size size =
          main_web_contents_->GetRenderWidgetHostView()->GetViewBounds().size();
      toolbox_web_contents_->GetRenderWidgetHostView()->SetSize(size);
    }
    UpdateBrowserWindow();
    return;
  }

  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, new_contents, disposition, initial_rect, user_gesture,
        was_blocked);
  }
}

void DevToolsWindow::WebContentsCreated(WebContents* source_contents,
                                        int opener_render_frame_id,
                                        const std::string& frame_name,
                                        const GURL& target_url,
                                        WebContents* new_contents,
                                        const base::string16& nw_window_manifest) {
  if (target_url.SchemeIs(content::kChromeDevToolsScheme) &&
      target_url.path().rfind("toolbox.html") != std::string::npos) {
    CHECK(can_dock_);
    if (toolbox_web_contents_)
      delete toolbox_web_contents_;
    toolbox_web_contents_ = new_contents;

    // Tag the DevTools toolbox WebContents with its TaskManager specific
    // UserData so that it shows up in the task manager.
    task_management::WebContentsTags::CreateForDevToolsContents(
        toolbox_web_contents_);
  }
}

void DevToolsWindow::CloseContents(WebContents* source) {
  CHECK(is_docked_);
  life_stage_ = kClosing;
  UpdateBrowserWindow();
  // In case of docked main_web_contents_, we own it so delete here.
  // Embedding DevTools window will be deleted as a result of
  // DevToolsUIBindings destruction.
  delete main_web_contents_;
}

void DevToolsWindow::ContentsZoomChange(bool zoom_in) {
  DCHECK(is_docked_);
  ui_zoom::PageZoom::Zoom(main_web_contents_, zoom_in ? content::PAGE_ZOOM_IN
                                                      : content::PAGE_ZOOM_OUT);
}

void DevToolsWindow::BeforeUnloadFired(WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (!intercepted_page_beforeunload_) {
    // Docked devtools window closed directly.
    if (proceed)
      bindings_->Detach();
    *proceed_to_fire_unload = proceed;
  } else {
    // Inspected page is attempting to close.
    WebContents* inspected_web_contents = GetInspectedWebContents();
    if (proceed) {
      inspected_web_contents->DispatchBeforeUnload(false);
    } else {
      bool should_proceed;
      inspected_web_contents->GetDelegate()->BeforeUnloadFired(
          inspected_web_contents, false, &should_proceed);
      DCHECK(!should_proceed);
    }
    *proceed_to_fire_unload = false;
  }
}

bool DevToolsWindow::PreHandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window) {
    return inspected_window->PreHandleKeyboardEvent(event,
                                                    is_keyboard_shortcut);
  }
  return false;
}

void DevToolsWindow::HandleKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (event.windowsKeyCode == 0x08) {
    // Do not navigate back in history on Windows (http://crbug.com/74156).
    return;
  }
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->HandleKeyboardEvent(event);
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager(
    WebContents* source) {
  WebContents* inspected_web_contents = GetInspectedWebContents();
  return (inspected_web_contents && inspected_web_contents->GetDelegate())
             ? inspected_web_contents->GetDelegate()
                   ->GetJavaScriptDialogManager(inspected_web_contents)
             : content::WebContentsDelegate::GetJavaScriptDialogManager(source);
}

content::ColorChooser* DevToolsWindow::OpenColorChooser(
    WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void DevToolsWindow::RunFileChooser(WebContents* web_contents,
                                    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

bool DevToolsWindow::PreHandleGestureEvent(
    WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return event.type == blink::WebGestureEvent::GesturePinchBegin ||
      event.type == blink::WebGestureEvent::GesturePinchUpdate ||
      event.type == blink::WebGestureEvent::GesturePinchEnd;
}

void DevToolsWindow::ActivateWindow() {
  if (life_stage_ != kLoadCompleted || headless_)
    return;
  if (is_docked_ && GetInspectedBrowserWindow())
    main_web_contents_->Focus();
  else if (!is_docked_ && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void DevToolsWindow::CloseWindow() {
  DCHECK(is_docked_);
  life_stage_ = kClosing;
  main_web_contents_->DispatchBeforeUnload(false);
}

void DevToolsWindow::SetInspectedPageBounds(const gfx::Rect& rect) {
  DevToolsContentsResizingStrategy strategy(rect);
  if (contents_resizing_strategy_.Equals(strategy))
    return;

  contents_resizing_strategy_.CopyFrom(strategy);
  UpdateBrowserWindow();
}

void DevToolsWindow::InspectElementCompleted() {
  if (!inspect_element_start_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("DevTools.InspectElement",
        base::TimeTicks::Now() - inspect_element_start_time_);
    inspect_element_start_time_ = base::TimeTicks();
  }
}

void DevToolsWindow::SetIsDocked(bool dock_requested) {
  if (life_stage_ == kClosing)
    return;

  DCHECK(can_dock_ || !dock_requested);
  if (!can_dock_)
    dock_requested = false;

  bool was_docked = is_docked_;
  is_docked_ = dock_requested;

  if (life_stage_ != kLoadCompleted) {
    // This is a first time call we waited for to initialize.
    life_stage_ = life_stage_ == kOnLoadFired ? kLoadCompleted : kIsDockedSet;
    if (life_stage_ == kLoadCompleted)
      LoadCompleted();
    return;
  }

  if (dock_requested == was_docked)
    return;

  if (dock_requested && !was_docked) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    tab_strip_model->DetachWebContentsAt(
        tab_strip_model->GetIndexOfWebContents(main_web_contents_));
    browser_ = NULL;
  } else if (!dock_requested && was_docked) {
    UpdateBrowserWindow();
  }

  Show(DevToolsToggleAction::Show());
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false);
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents || !inspected_web_contents->OpenURL(params)) {
    chrome::HostDesktopType host_desktop_type =
        browser_ ? browser_->host_desktop_type() : chrome::GetActiveDesktop();

    chrome::ScopedTabbedBrowserDisplayer displayer(profile_, host_desktop_type);
    chrome::AddSelectedTabWithURL(displayer.browser(), GURL(url),
                                  ui::PAGE_TRANSITION_LINK);
  }
}

void DevToolsWindow::SetWhitelistedShortcuts(
    const std::string& message) {
  event_forwarder_->SetWhitelistedShortcuts(message);
}

void DevToolsWindow::InspectedContentsClosing() {
  intercepted_page_beforeunload_ = false;
  life_stage_ = kClosing;
  main_web_contents_->ClosePage();
}

InfoBarService* DevToolsWindow::GetInfoBarService() {
  return is_docked_ ?
      InfoBarService::FromWebContents(GetInspectedWebContents()) :
      InfoBarService::FromWebContents(main_web_contents_);
}

void DevToolsWindow::RenderProcessGone(bool crashed) {
  // Docked DevToolsWindow owns its main_web_contents_ and must delete it.
  // Undocked main_web_contents_ are owned and handled by browser.
  // see crbug.com/369932
  if (is_docked_) {
    CloseContents(main_web_contents_);
  } else if (browser_ && crashed) {
    browser_->window()->Close();
  }
}

void DevToolsWindow::Close() {
  browser_->window()->Close();
}

void DevToolsWindow::OnLoadCompleted() {
  // First seed inspected tab id for extension APIs.
  WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      base::FundamentalValue tabId(session_tab_helper->session_id().id());
      bindings_->CallClientFunction("DevToolsAPI.setInspectedTabId",
                                    &tabId, NULL, NULL);
    }
  }

  if (life_stage_ == kClosing)
    return;

  // We could be in kLoadCompleted state already if frontend reloads itself.
  if (life_stage_ != kLoadCompleted) {
    // Load is completed when both kIsDockedSet and kOnLoadFired happened.
    // Here we set kOnLoadFired.
    life_stage_ = life_stage_ == kIsDockedSet ? kLoadCompleted : kOnLoadFired;
  }
  if (life_stage_ == kLoadCompleted)
    LoadCompleted();
}

void DevToolsWindow::CreateDevToolsBrowser() {
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetDictionary(prefs::kAppWindowPlacement)->HasKey(kDevToolsApp)) {
    DictionaryPrefUpdate update(prefs, prefs::kAppWindowPlacement);
    base::DictionaryValue* wp_prefs = update.Get();
    base::DictionaryValue* dev_tools_defaults = new base::DictionaryValue;
    wp_prefs->Set(kDevToolsApp, dev_tools_defaults);
    dev_tools_defaults->SetInteger("left", 100);
    dev_tools_defaults->SetInteger("top", 100);
    dev_tools_defaults->SetInteger("right", 740);
    dev_tools_defaults->SetInteger("bottom", 740);
    dev_tools_defaults->SetBoolean("maximized", false);
    dev_tools_defaults->SetBoolean("always_on_top", false);
  }

  browser_ = new Browser(Browser::CreateParams::CreateForDevTools(
      profile_,
      chrome::GetHostDesktopTypeForNativeView(
          main_web_contents_->GetNativeView())));
  browser_->tab_strip_model()->AddWebContents(
      main_web_contents_, -1, ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
      TabStripModel::ADD_ACTIVE);
  main_web_contents_->GetRenderViewHost()->SyncRendererPrefs();
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  Browser* browser = NULL;
  int tab;
  return FindInspectedBrowserAndTabIndex(GetInspectedWebContents(),
                                         &browser, &tab) ?
      browser->window() : NULL;
}

void DevToolsWindow::DoAction(const DevToolsToggleAction& action) {
  switch (action.type()) {
    case DevToolsToggleAction::kShowConsole: {
      base::StringValue panel_name("console");
      bindings_->CallClientFunction("DevToolsAPI.showPanel", &panel_name, NULL,
                                    NULL);
      break;
    }
    case DevToolsToggleAction::kShowSecurityPanel: {
      base::StringValue panel_name("security");
      bindings_->CallClientFunction("DevToolsAPI.showPanel", &panel_name, NULL,
                                    NULL);
      break;
    }
    case DevToolsToggleAction::kInspect:
      bindings_->CallClientFunction(
          "DevToolsAPI.enterInspectElementMode", NULL, NULL, NULL);
      break;

    case DevToolsToggleAction::kShow:
    case DevToolsToggleAction::kToggle:
      // Do nothing.
      break;

    case DevToolsToggleAction::kReveal: {
      const DevToolsToggleAction::RevealParams* params =
          action.params();
      CHECK(params);
      base::StringValue url_value(params->url);
      base::FundamentalValue line_value(static_cast<int>(params->line_number));
      base::FundamentalValue column_value(
          static_cast<int>(params->column_number));
      bindings_->CallClientFunction("DevToolsAPI.revealSourceLine",
                                    &url_value, &line_value, &column_value);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(NULL);
}

void DevToolsWindow::UpdateBrowserWindow() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
}

WebContents* DevToolsWindow::GetInspectedWebContents() {
  return inspected_contents_observer_
             ? inspected_contents_observer_->web_contents()
             : NULL;
}

void DevToolsWindow::LoadCompleted() {
  Show(action_on_load_);
  action_on_load_ = DevToolsToggleAction::NoOp();
  if (!load_completed_callback_.is_null()) {
    load_completed_callback_.Run();
    load_completed_callback_ = base::Closure();
  }
}

void DevToolsWindow::SetLoadCompletedCallback(const base::Closure& closure) {
  if (life_stage_ == kLoadCompleted || life_stage_ == kClosing) {
    if (!closure.is_null())
      closure.Run();
    return;
  }
  load_completed_callback_ = closure;
}

bool DevToolsWindow::ForwardKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return event_forwarder_->ForwardEvent(event);
}

bool DevToolsWindow::ReloadInspectedWebContents(bool ignore_cache) {
  // Only route reload via front-end if the agent is attached.
  WebContents* wc = GetInspectedWebContents();
  if (!wc || wc->GetCrashedStatus() != base::TERMINATION_STATUS_STILL_RUNNING)
    return false;
  base::FundamentalValue ignore_cache_value(ignore_cache);
  bindings_->CallClientFunction("DevToolsAPI.reloadInspectedPage",
                                &ignore_cache_value, nullptr, nullptr);
  return true;
}
