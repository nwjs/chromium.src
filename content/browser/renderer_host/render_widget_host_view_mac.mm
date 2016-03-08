// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#import <objc/runtime.h>
#include <OpenGL/gl.h>
#include <QuartzCore/QuartzCore.h>
#include <stdint.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_cftyperef.h"
#import "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/trace_event/trace_event.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/bad_message.h"
#import "content/browser/cocoa/system_hotkey_helper_mac.h"
#import "content/browser/cocoa/system_hotkey_map.h"
#include "content/browser/compositor/resize_lock.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#import "content/browser/renderer_host/input/synthetic_gesture_target_mac.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"
#include "content/browser/renderer_host/render_widget_resize_helper_mac.h"
#import "content/browser/renderer_host/text_input_client_mac.h"
#include "content/common/accessibility_messages.h"
#include "content/common/edit_command.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/input_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/common/view_messages.h"
#include "content/common/webplugin_geometry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view_frame_subscriber.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/browser/web_contents.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#import "third_party/mozilla/ComplexTextInputPanel.h"
#include "ui/base/cocoa/animation_utils.h"
#import "ui/base/cocoa/fullscreen_window_manager.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"
#include "ui/base/layout.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_profile.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"
#include "ui/gfx/screen.h"
#include "ui/gl/gl_switches.h"

using content::BrowserAccessibility;
using content::BrowserAccessibilityManager;
using content::EditCommand;
using content::FrameTreeNode;
using content::NativeWebKeyboardEvent;
using content::RenderFrameHost;
using content::RenderViewHost;
using content::RenderViewHostImpl;
using content::RenderWidgetHostImpl;
using content::RenderWidgetHostViewMac;
using content::RenderWidgetHostViewMacEditCommandHelper;
using content::TextInputClientMac;
using content::WebContents;
using content::WebGestureEventBuilder;
using content::WebMouseEventBuilder;
using content::WebMouseWheelEventBuilder;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebGestureEvent;

namespace content {
  extern bool g_support_transparency;
  extern bool g_force_cpu_draw;
}

namespace {

// Whether a keyboard event has been reserved by OSX.
BOOL EventIsReservedBySystem(NSEvent* event) {
  content::SystemHotkeyHelperMac* helper =
      content::SystemHotkeyHelperMac::GetInstance();
  return helper->map()->IsEventReserved(event);
}

RenderWidgetHostViewMac* GetRenderWidgetHostViewToUse(
    RenderWidgetHostViewMac* render_widget_host_view) {
  WebContents* web_contents = render_widget_host_view->GetWebContents();
  if (!web_contents)
    return render_widget_host_view;
  content::BrowserPluginGuestManager* guest_manager =
      web_contents->GetBrowserContext()->GetGuestManager();
  if (!guest_manager)
    return render_widget_host_view;
  content::WebContents* guest =
      guest_manager->GetFullPageGuest(web_contents);
  if (!guest)
    return render_widget_host_view;
  return static_cast<RenderWidgetHostViewMac*>(
      guest->GetRenderWidgetHostView());
}

}  // namespace

// These are not documented, so use only after checking -respondsToSelector:.
@interface NSApplication (UndocumentedSpeechMethods)
- (void)speakString:(NSString*)string;
- (void)stopSpeaking:(id)sender;
- (BOOL)isSpeaking;
@end

// This method will return YES for OS X versions 10.7.3 and later, and NO
// otherwise.
// Used to prevent a crash when building with the 10.7 SDK and accessing the
// notification below. See: http://crbug.com/260595.
static BOOL SupportsBackingPropertiesChangedNotification() {
  // windowDidChangeBackingProperties: method has been added to the
  // NSWindowDelegate protocol in 10.7.3, at the same time as the
  // NSWindowDidChangeBackingPropertiesNotification notification was added.
  // If the protocol contains this method description, the notification should
  // be supported as well.
  Protocol* windowDelegateProtocol = NSProtocolFromString(@"NSWindowDelegate");
  struct objc_method_description methodDescription =
      protocol_getMethodDescription(
          windowDelegateProtocol,
          @selector(windowDidChangeBackingProperties:),
          NO,
          YES);

  // If the protocol does not contain the method, the returned method
  // description is {NULL, NULL}
  return methodDescription.name != NULL || methodDescription.types != NULL;
}

// Private methods:
@interface RenderWidgetHostViewCocoa ()
@property(nonatomic, assign) NSRange selectedRange;
@property(nonatomic, assign) NSRange markedRange;

+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event;
- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r;
- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed;

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;
- (void)windowChangedGlobalFrame:(NSNotification*)notification;
- (void)checkForPluginImeCancellation;
- (void)updateScreenProperties;
- (void)setResponderDelegate:
        (NSObject<RenderWidgetHostViewMacDelegate>*)delegate;
- (void)showLookUpDictionaryOverlayInternal:(NSAttributedString*) string
                              baselinePoint:(NSPoint) baselinePoint
                                 targetView:(NSView*) view;
@end

// A window subclass that allows the fullscreen window to become main and gain
// keyboard focus. This is only used for pepper flash. Normal fullscreen is
// handled by the browser.
@interface PepperFlashFullscreenWindow : UnderlayOpenGLHostingWindow
@end

@implementation PepperFlashFullscreenWindow

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (BOOL)canBecomeMainWindow {
  return YES;
}

@end

@interface RenderWidgetPopupWindow : NSWindow {
   // The event tap that allows monitoring of all events, to properly close with
   // a click outside the bounds of the window.
  id clickEventTap_;
}
@end

@implementation RenderWidgetPopupWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)windowStyle
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  if (self = [super initWithContentRect:contentRect
                              styleMask:windowStyle
                                backing:bufferingType
                                  defer:deferCreation]) {
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self startObservingClicks];
  }
  return self;
}

- (void)close {
  [self stopObservingClicks];
  [super close];
}

// Gets called when the menubar is clicked.
// Needed because the local event monitor doesn't see the click on the menubar.
- (void)beganTracking:(NSNotification*)notification {
  [self close];
}

// Install the callback.
- (void)startObservingClicks {
  clickEventTap_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask
      handler:^NSEvent* (NSEvent* event) {
          if ([event window] == self)
            return event;
          NSEventType eventType = [event type];
          if (eventType == NSLeftMouseDown || eventType == NSRightMouseDown)
            [self close];
          return event;
  }];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
         selector:@selector(beganTracking:)
             name:NSMenuDidBeginTrackingNotification
           object:[NSApp mainMenu]];
}

// Remove the callback.
- (void)stopObservingClicks {
  if (!clickEventTap_)
    return;

  [NSEvent removeMonitor:clickEventTap_];
   clickEventTap_ = nil;

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter removeObserver:self
                name:NSMenuDidBeginTrackingNotification
              object:[NSApp mainMenu]];
}

@end

namespace {

// Maximum number of characters we allow in a tooltip.
const size_t kMaxTooltipLength = 1024;

// TODO(suzhe): Upstream this function.
blink::WebColor WebColorFromNSColor(NSColor *color) {
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];

  return
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * a)), 255)) << 24 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * r)), 255)) << 16 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * g)), 255)) << 8  |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * b)), 255));
}

// Extract underline information from an attributed string. Mostly copied from
// third_party/WebKit/Source/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(
    NSAttributedString* string,
    std::vector<blink::WebCompositionUnderline>* underlines) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
      blink::WebColor color = SK_ColorBLACK;
      if (NSColor *colorAttr =
          [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = WebColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      underlines->push_back(
          blink::WebCompositionUnderline(range.location,
                                         NSMaxRange(range),
                                         color,
                                         [style intValue] > 1,
                                         SK_ColorTRANSPARENT));
    }
    i = range.location + range.length;
  }
}

// EnablePasswordInput() and DisablePasswordInput() are copied from
// enableSecureTextInput() and disableSecureTextInput() functions in
// third_party/WebKit/WebCore/platform/SecureTextInput.cpp
// But we don't call EnableSecureEventInput() and DisableSecureEventInput()
// here, because they are already called in webkit and they are system wide
// functions.
void EnablePasswordInput() {
  CFArrayRef inputSources = TISCreateASCIICapableInputSourceList();
  TSMSetDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag,
                         sizeof(CFArrayRef), &inputSources);
  CFRelease(inputSources);
}

void DisablePasswordInput() {
  TSMRemoveDocumentProperty(0, kTSMDocumentEnabledInputSourcesPropertyTag);
}

// Calls to [NSScreen screens], required by FlipYFromRectToScreen and
// FlipNSRectToRectScreen, can take several milliseconds. Only re-compute this
// value when screen info changes.
// TODO(ccameron): An observer on every RWHVCocoa will set this to false
// on NSApplicationDidChangeScreenParametersNotification. Only one observer
// is necessary.
bool g_screen_info_up_to_date = false;

float FlipYFromRectToScreen(float y, float rect_height) {
  TRACE_EVENT0("browser", "FlipYFromRectToScreen");
  static CGFloat screen_zero_height = 0;
  if (!g_screen_info_up_to_date) {
    if ([[NSScreen screens] count] > 0) {
      screen_zero_height =
          [[[NSScreen screens] firstObject] frame].size.height;
      g_screen_info_up_to_date = true;
    } else {
      return y;
    }
  }
  return screen_zero_height - y - rect_height;
}

// Adjusts an NSRect in Cocoa screen coordinates to have an origin in the upper
// left of the primary screen (Carbon coordinates), and stuffs it into a
// gfx::Rect.
gfx::Rect FlipNSRectToRectScreen(const NSRect& rect) {
  gfx::Rect new_rect(NSRectToCGRect(rect));
  new_rect.set_y(FlipYFromRectToScreen(new_rect.y(), new_rect.height()));
  return new_rect;
}

// Returns the window that visually contains the given view. This is different
// from [view window] in the case of tab dragging, where the view's owning
// window is a floating panel attached to the actual browser window that the tab
// is visually part of.
NSWindow* ApparentWindowForView(NSView* view) {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = [view window];

  // See if this is a tab drag window. The width check is to distinguish that
  // case from extension popup windows.
  NSWindow* ancestor_window = [enclosing_window parentWindow];
  if (ancestor_window && (NSWidth([enclosing_window frame]) ==
                          NSWidth([ancestor_window frame]))) {
    enclosing_window = ancestor_window;
  }

  return enclosing_window;
}

blink::WebScreenInfo GetWebScreenInfo(NSView* view) {
  gfx::Display display =
      gfx::Screen::GetNativeScreen()->GetDisplayNearestWindow(view);

  NSScreen* screen = [NSScreen deepestScreen];

  blink::WebScreenInfo results;

  results.deviceScaleFactor = static_cast<int>(display.device_scale_factor());
  results.depth = NSBitsPerPixelFromDepth([screen depth]);
  results.depthPerComponent = NSBitsPerSampleFromDepth([screen depth]);
  results.isMonochrome =
      [[screen colorSpace] colorSpaceModel] == NSGrayColorSpaceModel;
  results.rect = display.bounds();
  results.availableRect = display.work_area();
  results.orientationAngle = display.RotationAsDegree();
  results.orientationType =
      content::RenderWidgetHostViewBase::GetOrientationTypeForDesktop(display);

  return results;
}

}  // namespace

namespace content {

////////////////////////////////////////////////////////////////////////////////
// DelegatedFrameHost, public:

ui::Layer* RenderWidgetHostViewMac::DelegatedFrameHostGetLayer() const {
  return root_layer_.get();
}

bool RenderWidgetHostViewMac::DelegatedFrameHostIsVisible() const {
  return !render_widget_host_->is_hidden();
}

gfx::Size RenderWidgetHostViewMac::DelegatedFrameHostDesiredSizeInDIP() const {
  return GetViewBounds().size();
}

bool RenderWidgetHostViewMac::DelegatedFrameCanCreateResizeLock() const {
  // Mac uses the RenderWidgetResizeHelper instead of a resize lock.
  return false;
}

scoped_ptr<ResizeLock>
RenderWidgetHostViewMac::DelegatedFrameHostCreateResizeLock(
    bool defer_compositor_lock) {
  NOTREACHED();
  return scoped_ptr<ResizeLock>();
}

void RenderWidgetHostViewMac::DelegatedFrameHostResizeLockWasReleased() {
  NOTREACHED();
}

void RenderWidgetHostViewMac::DelegatedFrameHostSendCompositorSwapAck(
    int output_surface_id,
    const cc::CompositorFrameAck& ack) {
  render_widget_host_->Send(new ViewMsg_SwapCompositorFrameAck(
      render_widget_host_->GetRoutingID(), output_surface_id, ack));
}

void RenderWidgetHostViewMac::DelegatedFrameHostSendReclaimCompositorResources(
    int output_surface_id,
    const cc::CompositorFrameAck& ack) {
  render_widget_host_->Send(new ViewMsg_ReclaimCompositorResources(
      render_widget_host_->GetRoutingID(), output_surface_id, ack));
}

void RenderWidgetHostViewMac::DelegatedFrameHostOnLostCompositorResources() {
  render_widget_host_->ScheduleComposite();
}

void RenderWidgetHostViewMac::DelegatedFrameHostUpdateVSyncParameters(
    const base::TimeTicks& timebase,
    const base::TimeDelta& interval) {
  render_widget_host_->UpdateVSyncParameters(timebase, interval);
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMacNSView, public:

NSView* RenderWidgetHostViewMac::AcceleratedWidgetGetNSView() const {
  return cocoa_view_;
}

void RenderWidgetHostViewMac::AcceleratedWidgetGetVSyncParameters(
    base::TimeTicks* timebase, base::TimeDelta* interval) const {
  if (display_link_ &&
      display_link_->GetVSyncParameters(timebase, interval))
    return;
  *timebase = base::TimeTicks();
  *interval = base::TimeDelta();
}

void RenderWidgetHostViewMac::AcceleratedWidgetSwapCompleted() {
  if (display_link_)
    display_link_->NotifyCurrentTime(base::TimeTicks::Now());
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewBase, public:

// static
void RenderWidgetHostViewBase::GetDefaultScreenInfo(
    blink::WebScreenInfo* results) {
  *results = GetWebScreenInfo(NULL);
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, public:

RenderWidgetHostViewMac::RenderWidgetHostViewMac(RenderWidgetHost* widget,
                                                 bool is_guest_view_hack)
    : render_widget_host_(RenderWidgetHostImpl::From(widget)),
      text_input_type_(ui::TEXT_INPUT_TYPE_NONE),
      can_compose_inline_(true),
      browser_compositor_state_(BrowserCompositorDestroyed),
      browser_compositor_placeholder_(new BrowserCompositorMacPlaceholder),
      page_at_minimum_scale_(true),
      is_loading_(false),
      allow_pause_for_resize_or_repaint_(true),
      is_guest_view_hack_(is_guest_view_hack),
      fullscreen_parent_host_view_(NULL),
      weak_factory_(this) {
  // |cocoa_view_| owns us and we will be deleted when |cocoa_view_|
  // goes away.  Since we autorelease it, our caller must put
  // |GetNativeView()| into the view hierarchy right after calling us.
  cocoa_view_ = [[[RenderWidgetHostViewCocoa alloc]
                  initWithRenderWidgetHostViewMac:this] autorelease];

  // Paint this view host with |background_color_| when there is no content
  // ready to draw.
  background_layer_.reset([[CALayer alloc] init]);
  // Set the default color to be white. This is the wrong thing to do, but many
  // UI components expect this view to be opaque.
  bool isOpaque = [cocoa_view_ isOpaque];
  [background_layer_ setBackgroundColor: (isOpaque || !content::g_support_transparency) ?
     CGColorGetConstantColor(kCGColorWhite) : CGColorGetConstantColor(kCGColorClear)];
  [cocoa_view_ setLayer:background_layer_];
  [cocoa_view_ setWantsLayer:!content::g_force_cpu_draw];

  root_layer_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
  delegated_frame_host_.reset(new DelegatedFrameHost(this));

  gfx::Screen::GetScreenFor(cocoa_view_)->AddObserver(this);

  if (!is_guest_view_hack_)
    render_widget_host_->SetView(this);

  // Let the page-level input event router know about our surface ID
  // namespace for surface-based hit testing.
  if (UseSurfacesEnabled() && render_widget_host_->delegate() &&
      render_widget_host_->delegate()->GetInputEventRouter()) {
    render_widget_host_->delegate()
        ->GetInputEventRouter()
        ->AddSurfaceIdNamespaceOwner(GetSurfaceIdNamespace(), this);
  }
}

RenderWidgetHostViewMac::~RenderWidgetHostViewMac() {
  gfx::Screen::GetScreenFor(cocoa_view_)->RemoveObserver(this);

  // This is being called from |cocoa_view_|'s destructor, so invalidate the
  // pointer.
  cocoa_view_ = nil;

  UnlockMouse();

  if (UseSurfacesEnabled() && render_widget_host_ &&
      render_widget_host_->delegate() &&
      render_widget_host_->delegate()->GetInputEventRouter()) {
    render_widget_host_->delegate()
        ->GetInputEventRouter()
        ->RemoveSurfaceIdNamespaceOwner(GetSurfaceIdNamespace());
  }

  // Ensure that the browser compositor is destroyed in a safe order.
  ShutdownBrowserCompositor();

  // We are owned by RenderWidgetHostViewCocoa, so if we go away before the
  // RenderWidgetHost does we need to tell it not to hold a stale pointer to
  // us.
  if (render_widget_host_) {
    // If this is a RenderWidgetHostViewGuest's platform_view_, we're not the
    // RWH's view, the RenderWidgetHostViewGuest is. So don't reset the RWH's
    // view, the RenderWidgetHostViewGuest will do it.
    if (!is_guest_view_hack_)
      render_widget_host_->SetView(NULL);
  }
}

void RenderWidgetHostViewMac::SetDelegate(
    NSObject<RenderWidgetHostViewMacDelegate>* delegate) {
  [cocoa_view_ setResponderDelegate:delegate];
}

void RenderWidgetHostViewMac::SetAllowPauseForResizeOrRepaint(bool allow) {
  allow_pause_for_resize_or_repaint_ = allow;
}

///////////////////////////////////////////////////////////////////////////////
// RenderWidgetHostViewMac, RenderWidgetHostView implementation:

void RenderWidgetHostViewMac::EnsureBrowserCompositorView() {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::EnsureBrowserCompositorView");

  // Create the view, to transition from Destroyed -> Suspended.
  if (browser_compositor_state_ == BrowserCompositorDestroyed) {
    browser_compositor_ = BrowserCompositorMac::Create();
    browser_compositor_->compositor()->SetRootLayer(root_layer_.get());
    browser_compositor_->compositor()->SetHostHasTransparentBackground(
        !GetBackgroundOpaque());
    browser_compositor_->accelerated_widget_mac()->SetNSView(this);
    browser_compositor_state_ = BrowserCompositorSuspended;
  }

  // Show the DelegatedFrameHost to transition from Suspended -> Active.
  if (browser_compositor_state_ == BrowserCompositorSuspended) {
    delegated_frame_host_->SetCompositor(browser_compositor_->compositor());
    delegated_frame_host_->WasShown(ui::LatencyInfo());
    // Unsuspend the browser compositor after showing the delegated frame host.
    // If there is not a saved delegated frame, then the delegated frame host
    // will keep the compositor locked until a delegated frame is swapped.
    float scale_factor = ViewScaleFactor();
    browser_compositor_->compositor()->SetScaleAndSize(
        scale_factor,
        gfx::ConvertSizeToPixel(scale_factor, GetViewBounds().size()));
    browser_compositor_->Unsuspend();
    browser_compositor_state_ = BrowserCompositorActive;
  }
  if (content::g_support_transparency)
    root_layer_->GetCompositor()->SetHostHasTransparentBackground(!cocoa_view_.isOpaque);
}

void RenderWidgetHostViewMac::SuspendBrowserCompositorView() {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::SuspendBrowserCompositorView");

  // Hide the DelegatedFrameHost to transition from Active -> Suspended.
  if (browser_compositor_state_ == BrowserCompositorActive) {
    // Ensure that any changes made to the ui::Compositor do not result in new
    // frames being produced.
    browser_compositor_->Suspend();
    // Marking the DelegatedFrameHost as removed from the window hierarchy is
    // necessary to remove all connections to its old ui::Compositor.
    delegated_frame_host_->WasHidden();
    delegated_frame_host_->ResetCompositor();
    browser_compositor_state_ = BrowserCompositorSuspended;
  }
}

void RenderWidgetHostViewMac::DestroyBrowserCompositorView() {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::DestroyBrowserCompositorView");

  // Transition from Active -> Suspended if need be.
  SuspendBrowserCompositorView();

  // Destroy the BrowserCompositorView to transition Suspended -> Destroyed.
  if (browser_compositor_state_ == BrowserCompositorSuspended) {
    browser_compositor_->accelerated_widget_mac()->ResetNSView();
    browser_compositor_->compositor()->SetScaleAndSize(1.0, gfx::Size(0, 0));
    browser_compositor_->compositor()->SetRootLayer(nullptr);
    BrowserCompositorMac::Recycle(std::move(browser_compositor_));
    browser_compositor_state_ = BrowserCompositorDestroyed;
  }
}

void RenderWidgetHostViewMac::DestroySuspendedBrowserCompositorViewIfNeeded() {
  if (browser_compositor_state_ != BrowserCompositorSuspended)
    return;

  // If this view is in a window that is visible, keep around the suspended
  // BrowserCompositorView in case |cocoa_view_| is suddenly revealed (so that
  // we don't flash white).
  NSWindow* window = [cocoa_view_ window];
  if (window)
    return;

  // This should only be reached if |render_widget_host_| is hidden, destroyed,
  // or in the process of being destroyed.
  DestroyBrowserCompositorView();
}

bool RenderWidgetHostViewMac::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetHostViewMac, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_PluginFocusChanged, OnPluginFocusChanged)
    IPC_MESSAGE_HANDLER(ViewHostMsg_StartPluginIme, OnStartPluginIme)
    IPC_MESSAGE_HANDLER(ViewMsg_GetRenderedTextCompleted,
        OnGetRenderedTextCompleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RenderWidgetHostViewMac::InitAsChild(
    gfx::NativeView parent_view) {
}

void RenderWidgetHostViewMac::InitAsPopup(
    RenderWidgetHostView* parent_host_view,
    const gfx::Rect& pos) {
  bool activatable = popup_type_ == blink::WebPopupTypeNone;
  [cocoa_view_ setCloseOnDeactivate:YES];
  [cocoa_view_ setCanBeKeyView:activatable ? YES : NO];

  NSPoint origin_global = NSPointFromCGPoint(pos.origin().ToCGPoint());
  origin_global.y = FlipYFromRectToScreen(origin_global.y, pos.height());

  popup_window_.reset([[RenderWidgetPopupWindow alloc]
      initWithContentRect:NSMakeRect(origin_global.x, origin_global.y,
                                     pos.width(), pos.height())
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [popup_window_ setLevel:NSPopUpMenuWindowLevel];
  [popup_window_ setReleasedWhenClosed:NO];
  [popup_window_ makeKeyAndOrderFront:nil];
  [[popup_window_ contentView] addSubview:cocoa_view_];
  [cocoa_view_ setFrame:[[popup_window_ contentView] bounds]];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [[NSNotificationCenter defaultCenter]
      addObserver:cocoa_view_
         selector:@selector(popupWindowWillClose:)
             name:NSWindowWillCloseNotification
           object:popup_window_];
}

// This function creates the fullscreen window and hides the dock and menubar if
// necessary. Note, this codepath is only used for pepper flash when
// pp::FlashFullScreen::SetFullscreen() is called. If
// pp::FullScreen::SetFullscreen() is called then the entire browser window
// will enter fullscreen instead.
void RenderWidgetHostViewMac::InitAsFullscreen(
    RenderWidgetHostView* reference_host_view) {
  fullscreen_parent_host_view_ =
      static_cast<RenderWidgetHostViewMac*>(reference_host_view);
  NSWindow* parent_window = nil;
  if (reference_host_view)
    parent_window = [reference_host_view->GetNativeView() window];
  NSScreen* screen = [parent_window screen];
  if (!screen)
    screen = [NSScreen mainScreen];

  pepper_fullscreen_window_.reset([[PepperFlashFullscreenWindow alloc]
      initWithContentRect:[screen frame]
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [pepper_fullscreen_window_ setLevel:NSFloatingWindowLevel];
  [pepper_fullscreen_window_ setReleasedWhenClosed:NO];
  [cocoa_view_ setCanBeKeyView:YES];
  [cocoa_view_ setFrame:[[pepper_fullscreen_window_ contentView] bounds]];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  // If the pepper fullscreen window isn't opaque then there are performance
  // issues when it's on the discrete GPU and the Chrome window is being drawn
  // to. http://crbug.com/171911
  [pepper_fullscreen_window_ setOpaque:YES];

  // Note that this forms a reference cycle between the fullscreen window and
  // the rwhvmac: The PepperFlashFullscreenWindow retains cocoa_view_,
  // but cocoa_view_ keeps pepper_fullscreen_window_ in an instance variable.
  // This cycle is normally broken when -keyEvent: receives an <esc> key, which
  // explicitly calls Shutdown on the render_widget_host_, which calls
  // Destroy() on RWHVMac, which drops the reference to
  // pepper_fullscreen_window_.
  [[pepper_fullscreen_window_ contentView] addSubview:cocoa_view_];

  // Note that this keeps another reference to pepper_fullscreen_window_.
  fullscreen_window_manager_.reset([[FullscreenWindowManager alloc]
      initWithWindow:pepper_fullscreen_window_.get()
       desiredScreen:screen]);
  [fullscreen_window_manager_ enterFullscreenMode];
  [pepper_fullscreen_window_ makeKeyAndOrderFront:nil];
}

void RenderWidgetHostViewMac::release_pepper_fullscreen_window_for_testing() {
  // See comment in InitAsFullscreen(): There is a reference cycle between
  // rwhvmac and fullscreen window, which is usually broken by hitting <esc>.
  // Tests that test pepper fullscreen mode without sending an <esc> event
  // need to call this method to break the reference cycle.
  [fullscreen_window_manager_ exitFullscreenMode];
  fullscreen_window_manager_.reset();
  [pepper_fullscreen_window_ close];
  pepper_fullscreen_window_.reset();
}

int RenderWidgetHostViewMac::window_number() const {
  NSWindow* window = [cocoa_view_ window];
  if (!window)
    return -1;
  return [window windowNumber];
}

float RenderWidgetHostViewMac::ViewScaleFactor() const {
  return ui::GetScaleFactorForNativeView(cocoa_view_);
}

void RenderWidgetHostViewMac::UpdateDisplayLink() {
  static bool is_vsync_disabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuVsync);
  if (is_vsync_disabled)
    return;

  NSScreen* screen = [[cocoa_view_ window] screen];
  NSDictionary* screen_description = [screen deviceDescription];
  NSNumber* screen_number = [screen_description objectForKey:@"NSScreenNumber"];
  CGDirectDisplayID display_id = [screen_number unsignedIntValue];

  display_link_ = ui::DisplayLinkMac::GetForDisplay(display_id);
  if (!display_link_.get()) {
    // Note that on some headless systems, the display link will fail to be
    // created, so this should not be a fatal error.
    LOG(ERROR) << "Failed to create display link.";
  }
}

void RenderWidgetHostViewMac::SendVSyncParametersToRenderer() {
  if (!render_widget_host_ || !display_link_.get())
    return;

  if (!display_link_->GetVSyncParameters(&vsync_timebase_, &vsync_interval_)) {
    vsync_timebase_ = base::TimeTicks();
    vsync_interval_ = base::TimeDelta();
    return;
  }

  if (browser_compositor_) {
    browser_compositor_->compositor()->vsync_manager()->UpdateVSyncParameters(
        vsync_timebase_, vsync_interval_);
  }
}

void RenderWidgetHostViewMac::SpeakText(const std::string& text) {
  [NSApp speakString:base::SysUTF8ToNSString(text)];
}

void RenderWidgetHostViewMac::UpdateBackingStoreProperties() {
  if (!render_widget_host_)
    return;
  render_widget_host_->NotifyScreenInfoChanged();
}

RenderWidgetHost* RenderWidgetHostViewMac::GetRenderWidgetHost() const {
  return render_widget_host_;
}

void RenderWidgetHostViewMac::Show() {
  ScopedCAActionDisabler disabler;
  [cocoa_view_ setHidden:NO];
  if (!render_widget_host_->is_hidden())
    return;

  // Re-create the browser compositor. If the DelegatedFrameHost has a cached
  // frame from the last time it was visible, then it will immediately be
  // drawn. If not, then the compositor will remain locked until a new delegated
  // frame is swapped.
  EnsureBrowserCompositorView();

  WasUnOccluded();

  // If there is not a frame being currently drawn, kick one, so that the below
  // pause will have a frame to wait on.
  render_widget_host_->ScheduleComposite();
  PauseForPendingResizeOrRepaintsAndDraw();
}

void RenderWidgetHostViewMac::Hide() {
  ScopedCAActionDisabler disabler;
  [cocoa_view_ setHidden:YES];
  WasOccluded();
  DestroySuspendedBrowserCompositorViewIfNeeded();
}

void RenderWidgetHostViewMac::WasUnOccluded() {
  if (!render_widget_host_->is_hidden())
    return;

  ui::LatencyInfo renderer_latency_info;
  renderer_latency_info.AddLatencyNumber(
      ui::TAB_SHOW_COMPONENT,
      render_widget_host_->GetLatencyComponentId(),
      0);
  render_widget_host_->WasShown(renderer_latency_info);
}

void RenderWidgetHostViewMac::WasOccluded() {
  if (render_widget_host_->is_hidden())
    return;

  // Note that the following call to WasHidden() can trigger thumbnail
  // generation on behalf of the NTP, and that cannot succeed if the browser
  // compositor view has been suspended. Therefore these two statements must
  // occur in this specific order. However, because thumbnail generation is
  // asychronous, that operation won't run before SuspendBrowserCompositorView()
  // completes. As a result you won't get a thumbnail for the page unless you
  // happen to switch back to it. See http://crbug.com/530707 .
  render_widget_host_->WasHidden();
  SuspendBrowserCompositorView();
}

void RenderWidgetHostViewMac::SetSize(const gfx::Size& size) {
  gfx::Rect rect = GetViewBounds();
  rect.set_size(size);
  SetBounds(rect);
}

void RenderWidgetHostViewMac::SetBounds(const gfx::Rect& rect) {
  // |rect.size()| is view coordinates, |rect.origin| is screen coordinates,
  // TODO(thakis): fix, http://crbug.com/73362
  if (render_widget_host_->is_hidden())
    return;

  // During the initial creation of the RenderWidgetHostView in
  // WebContentsImpl::CreateRenderViewForRenderManager, SetSize is called with
  // an empty size. In the Windows code flow, it is not ignored because
  // subsequent sizing calls from the OS flow through TCVW::WasSized which calls
  // SetSize() again. On Cocoa, we rely on the Cocoa view struture and resizer
  // flags to keep things sized properly. On the other hand, if the size is not
  // empty then this is a valid request for a pop-up.
  if (rect.size().IsEmpty())
    return;

  // Ignore the position of |rect| for non-popup rwhvs. This is because
  // background tabs do not have a window, but the window is required for the
  // coordinate conversions. Popups are always for a visible tab.
  //
  // Note: If |cocoa_view_| has been removed from the view hierarchy, it's still
  // valid for resizing to be requested (e.g., during tab capture, to size the
  // view to screen-capture resolution). In this case, simply treat the view as
  // relative to the screen.
  BOOL isRelativeToScreen = IsPopup() ||
      ![[cocoa_view_ superview] isKindOfClass:[BaseView class]];
  if (isRelativeToScreen) {
    // The position of |rect| is screen coordinate system and we have to
    // consider Cocoa coordinate system is upside-down and also multi-screen.
    NSPoint origin_global = NSPointFromCGPoint(rect.origin().ToCGPoint());
    NSSize size = NSMakeSize(rect.width(), rect.height());
    size = [cocoa_view_ convertSize:size toView:nil];
    origin_global.y = FlipYFromRectToScreen(origin_global.y, size.height);
    NSRect frame = NSMakeRect(origin_global.x, origin_global.y,
                              size.width, size.height);
    if (IsPopup())
      [popup_window_ setFrame:frame display:YES];
    else
      [cocoa_view_ setFrame:frame];
  } else {
    BaseView* superview = static_cast<BaseView*>([cocoa_view_ superview]);
    gfx::Rect rect2 = [superview flipNSRectToRect:[cocoa_view_ frame]];
    rect2.set_width(rect.width());
    rect2.set_height(rect.height());
    [cocoa_view_ setFrame:[superview flipRectToNSRect:rect2]];
  }
}

gfx::Vector2dF RenderWidgetHostViewMac::GetLastScrollOffset() const {
  return last_scroll_offset_;
}

gfx::NativeView RenderWidgetHostViewMac::GetNativeView() const {
  return cocoa_view_;
}

gfx::NativeViewId RenderWidgetHostViewMac::GetNativeViewId() const {
  return reinterpret_cast<gfx::NativeViewId>(GetNativeView());
}

gfx::NativeViewAccessible RenderWidgetHostViewMac::GetNativeViewAccessible() {
  return cocoa_view_;
}

void RenderWidgetHostViewMac::MovePluginWindows(
    const std::vector<WebPluginGeometry>& moves) {
  // Must be overridden, but unused on this platform. Core Animation
  // plugins are drawn by the GPU process (through the compositor),
  // and Core Graphics plugins are drawn by the renderer process.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void RenderWidgetHostViewMac::Focus() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
}

bool RenderWidgetHostViewMac::HasFocus() const {
  return [[cocoa_view_ window] firstResponder] == cocoa_view_;
}

bool RenderWidgetHostViewMac::IsSurfaceAvailableForCopy() const {
  DCHECK(delegated_frame_host_);
  return delegated_frame_host_->CanCopyToBitmap();
}

bool RenderWidgetHostViewMac::IsShowing() {
  return ![cocoa_view_ isHidden];
}

gfx::Rect RenderWidgetHostViewMac::GetViewBounds() const {
  NSRect bounds = [cocoa_view_ bounds];
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return gfx::Rect(gfx::Size(NSWidth(bounds), NSHeight(bounds)));

  bounds = [cocoa_view_ convertRect:bounds toView:nil];
  bounds.origin = [enclosing_window convertBaseToScreen:bounds.origin];
  return FlipNSRectToRectScreen(bounds);
}

void RenderWidgetHostViewMac::UpdateCursor(const WebCursor& cursor) {
  WebCursor web_cursor = cursor;
  [cocoa_view_ updateCursor:web_cursor.GetNativeCursor()];
}

void RenderWidgetHostViewMac::SetIsLoading(bool is_loading) {
  is_loading_ = is_loading;
  // If we ever decide to show the waiting cursor while the page is loading
  // like Chrome does on Windows, call |UpdateCursor()| here.
}

void RenderWidgetHostViewMac::TextInputStateChanged(
const ViewHostMsg_TextInputState_Params& params) {
  if (text_input_type_ != params.type
      || can_compose_inline_ != params.can_compose_inline) {
    text_input_type_ = params.type;
    can_compose_inline_ = params.can_compose_inline;
    if (HasFocus()) {
      SetTextInputActive(true);

      // Let AppKit cache the new input context to make IMEs happy.
      // See http://crbug.com/73039.
      [NSApp updateWindows];

#ifndef __LP64__
      UseInputWindow(TSMGetActiveDocument(), !can_compose_inline_);
#endif
    }
  }
}

void RenderWidgetHostViewMac::ImeCancelComposition() {
  [cocoa_view_ cancelComposition];
}

void RenderWidgetHostViewMac::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds) {
  // The RangeChanged message is only sent with valid values. The current
  // caret position (start == end) will be sent if there is no IME range.
  [cocoa_view_ setMarkedRange:range.ToNSRange()];
  composition_range_ = range;
  composition_bounds_ = character_bounds;
}

void RenderWidgetHostViewMac::RenderProcessGone(base::TerminationStatus status,
                                                int error_code) {
  Destroy();
}

void RenderWidgetHostViewMac::Destroy() {
  [[NSNotificationCenter defaultCenter]
      removeObserver:cocoa_view_
                name:NSWindowWillCloseNotification
              object:popup_window_];

  // We've been told to destroy.
  [cocoa_view_ retain];
  [cocoa_view_ removeFromSuperview];
  [cocoa_view_ autorelease];

  [popup_window_ close];
  popup_window_.autorelease();

  [fullscreen_window_manager_ exitFullscreenMode];
  fullscreen_window_manager_.reset();
  [pepper_fullscreen_window_ close];

  // This can be called as part of processing the window's responder
  // chain, for instance |-performKeyEquivalent:|.  In that case the
  // object needs to survive until the stack unwinds.
  pepper_fullscreen_window_.autorelease();

  // Clear SurfaceID namespace ownership before we shutdown the
  // compositor.
  if (UseSurfacesEnabled() && render_widget_host_ &&
      render_widget_host_->delegate() &&
      render_widget_host_->delegate()->GetInputEventRouter()) {
    render_widget_host_->delegate()
        ->GetInputEventRouter()
        ->RemoveSurfaceIdNamespaceOwner(GetSurfaceIdNamespace());
  }

  // Delete the delegated frame state, which will reach back into
  // render_widget_host_.
  ShutdownBrowserCompositor();

  // We get this call just before |render_widget_host_| deletes
  // itself.  But we are owned by |cocoa_view_|, which may be retained
  // by some other code.  Examples are WebContentsViewMac's
  // |latent_focus_view_| and TabWindowController's
  // |cachedContentView_|.
  render_widget_host_ = NULL;
}

// Called from the renderer to tell us what the tooltip text should be. It
// calls us frequently so we need to cache the value to prevent doing a lot
// of repeat work.
void RenderWidgetHostViewMac::SetTooltipText(
    const base::string16& tooltip_text) {
  if (tooltip_text != tooltip_text_ && [[cocoa_view_ window] isKeyWindow]) {
    tooltip_text_ = tooltip_text;

    // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
    // Windows; we're just trying to be polite. Don't persist the trimmed
    // string, as then the comparison above will always fail and we'll try to
    // set it again every single time the mouse moves.
    base::string16 display_text = tooltip_text_;
    if (tooltip_text_.length() > kMaxTooltipLength)
      display_text = tooltip_text_.substr(0, kMaxTooltipLength);

    NSString* tooltip_nsstring = base::SysUTF16ToNSString(display_text);
    [cocoa_view_ setToolTipAtMousePoint:tooltip_nsstring];
  }
}

bool RenderWidgetHostViewMac::SupportsSpeech() const {
  return [NSApp respondsToSelector:@selector(speakString:)] &&
         [NSApp respondsToSelector:@selector(stopSpeaking:)];
}

void RenderWidgetHostViewMac::SpeakSelection() {
  if (![NSApp respondsToSelector:@selector(speakString:)])
    return;

  if (selected_text_.empty() && render_widget_host_) {
    // If there's no selection, speak all text. Send an asynchronous IPC
    // request for fetching all the text for a webcontent.
    // ViewMsg_GetRenderedTextCompleted is sent back to IPC Message receiver.
    render_widget_host_->Send(new ViewMsg_GetRenderedText(
        render_widget_host_->GetRoutingID()));
    return;
  }

  SpeakText(selected_text_);
}

bool RenderWidgetHostViewMac::IsSpeaking() const {
  return [NSApp respondsToSelector:@selector(isSpeaking)] &&
         [NSApp isSpeaking];
}

void RenderWidgetHostViewMac::StopSpeaking() {
  if ([NSApp respondsToSelector:@selector(stopSpeaking:)])
    [NSApp stopSpeaking:cocoa_view_];
}

//
// RenderWidgetHostViewCocoa uses the stored selection text,
// which implements NSServicesRequests protocol.
//
void RenderWidgetHostViewMac::SelectionChanged(const base::string16& text,
                                               size_t offset,
                                               const gfx::Range& range) {
  if (range.is_empty() || text.empty()) {
    selected_text_.clear();
  } else {
    size_t pos = range.GetMin() - offset;
    size_t n = range.length();

    DCHECK(pos + n <= text.length()) << "The text can not fully cover range.";
    if (pos >= text.length()) {
      DCHECK(false) << "The text can not cover range.";
      return;
    }
    selected_text_ = base::UTF16ToUTF8(text.substr(pos, n));
  }

  [cocoa_view_ setSelectedRange:range.ToNSRange()];
  // Updates markedRange when there is no marked text so that retrieving
  // markedRange immediately after calling setMarkdText: returns the current
  // caret position.
  if (![cocoa_view_ hasMarkedText]) {
    [cocoa_view_ setMarkedRange:range.ToNSRange()];
  }

  RenderWidgetHostViewBase::SelectionChanged(text, offset, range);
}

void RenderWidgetHostViewMac::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params) {
  if (params.anchor_rect == params.focus_rect)
    caret_rect_ = params.anchor_rect;
  first_selection_rect_ = params.anchor_rect;
}

void RenderWidgetHostViewMac::SetShowingContextMenu(bool showing) {
  RenderWidgetHostViewBase::SetShowingContextMenu(showing);

  // Create a fake mouse event to inform the render widget that the mouse
  // left or entered.
  NSWindow* window = [cocoa_view_ window];
  // TODO(asvitkine): If the location outside of the event stream doesn't
  // correspond to the current event (due to delayed event processing), then
  // this may result in a cursor flicker if there are later mouse move events
  // in the pipeline. Find a way to use the mouse location from the event that
  // dismissed the context menu.
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:location
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:window_number()
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0];
  WebMouseEvent web_event = WebMouseEventBuilder::Build(event, cocoa_view_);
  if (showing)
    web_event.type = WebInputEvent::MouseLeave;
  ForwardMouseEvent(web_event);
}

bool RenderWidgetHostViewMac::IsPopup() const {
  return popup_type_ != blink::WebPopupTypeNone;
}

void RenderWidgetHostViewMac::CopyFromCompositingSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    const ReadbackRequestCallback& callback,
    const SkColorType preferred_color_type) {
  DCHECK(delegated_frame_host_);
  delegated_frame_host_->CopyFromCompositingSurface(
      src_subrect, dst_size, callback, preferred_color_type);
}

void RenderWidgetHostViewMac::CopyFromCompositingSurfaceToVideoFrame(
    const gfx::Rect& src_subrect,
    const scoped_refptr<media::VideoFrame>& target,
    const base::Callback<void(const gfx::Rect&, bool)>& callback) {
  DCHECK(delegated_frame_host_);
  delegated_frame_host_->CopyFromCompositingSurfaceToVideoFrame(
      src_subrect, target, callback);
}

bool RenderWidgetHostViewMac::CanCopyToVideoFrame() const {
  DCHECK(delegated_frame_host_);
  return delegated_frame_host_->CanCopyToVideoFrame();
}

void RenderWidgetHostViewMac::BeginFrameSubscription(
    scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber) {
  DCHECK(delegated_frame_host_);
  delegated_frame_host_->BeginFrameSubscription(std::move(subscriber));
}

void RenderWidgetHostViewMac::EndFrameSubscription() {
  DCHECK(delegated_frame_host_);
  delegated_frame_host_->EndFrameSubscription();
}

void RenderWidgetHostViewMac::ForwardMouseEvent(const WebMouseEvent& event) {
  if (render_widget_host_)
    render_widget_host_->ForwardMouseEvent(event);

  if (event.type == WebInputEvent::MouseLeave) {
    [cocoa_view_ setToolTipAtMousePoint:nil];
    tooltip_text_.clear();
  }
}

void RenderWidgetHostViewMac::KillSelf() {
  if (!weak_factory_.HasWeakPtrs()) {
    [cocoa_view_ setHidden:YES];
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&RenderWidgetHostViewMac::ShutdownHost,
                   weak_factory_.GetWeakPtr()));
  }
}

bool RenderWidgetHostViewMac::PostProcessEventForPluginIme(
    const NativeWebKeyboardEvent& event) {
  // Check WebInputEvent type since multiple types of events can be sent into
  // WebKit for the same OS event (e.g., RawKeyDown and Char), so filtering is
  // necessary to avoid double processing.
  // Also check the native type, since NSFlagsChanged is considered a key event
  // for WebKit purposes, but isn't considered a key event by the OS.
  if (event.type == WebInputEvent::RawKeyDown &&
      [event.os_event type] == NSKeyDown)
    return [cocoa_view_ postProcessEventForPluginIme:event.os_event];
  return false;
}

void RenderWidgetHostViewMac::PluginImeCompositionCompleted(
    const base::string16& text, int plugin_id) {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_PluginImeCompositionCompleted(
        render_widget_host_->GetRoutingID(), text, plugin_id));
  }
}

bool RenderWidgetHostViewMac::GetLineBreakIndex(
    const std::vector<gfx::Rect>& bounds,
    const gfx::Range& range,
    size_t* line_break_point) {
  DCHECK(line_break_point);
  if (range.start() >= bounds.size() || range.is_reversed() || range.is_empty())
    return false;

  // We can't check line breaking completely from only rectangle array. Thus we
  // assume the line breaking as the next character's y offset is larger than
  // a threshold. Currently the threshold is determined as minimum y offset plus
  // 75% of maximum height.
  // TODO(nona): Check the threshold is reliable or not.
  // TODO(nona): Bidi support.
  const size_t loop_end_idx = std::min(bounds.size(), range.end());
  int max_height = 0;
  int min_y_offset = std::numeric_limits<int32_t>::max();
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    max_height = std::max(max_height, bounds[idx].height());
    min_y_offset = std::min(min_y_offset, bounds[idx].y());
  }
  int line_break_threshold = min_y_offset + (max_height * 3 / 4);
  for (size_t idx = range.start(); idx < loop_end_idx; ++idx) {
    if (bounds[idx].y() > line_break_threshold) {
      *line_break_point = idx;
      return true;
    }
  }
  return false;
}

gfx::Rect RenderWidgetHostViewMac::GetFirstRectForCompositionRange(
    const gfx::Range& range,
    gfx::Range* actual_range) {
  DCHECK(actual_range);
  DCHECK(!composition_bounds_.empty());
  DCHECK(range.start() <= composition_bounds_.size());
  DCHECK(range.end() <= composition_bounds_.size());

  if (range.is_empty()) {
    *actual_range = range;
    if (range.start() == composition_bounds_.size()) {
      return gfx::Rect(composition_bounds_[range.start() - 1].right(),
                       composition_bounds_[range.start() - 1].y(),
                       0,
                       composition_bounds_[range.start() - 1].height());
    } else {
      return gfx::Rect(composition_bounds_[range.start()].x(),
                       composition_bounds_[range.start()].y(),
                       0,
                       composition_bounds_[range.start()].height());
    }
  }

  size_t end_idx;
  if (!GetLineBreakIndex(composition_bounds_, range, &end_idx)) {
    end_idx = range.end();
  }
  *actual_range = gfx::Range(range.start(), end_idx);
  gfx::Rect rect = composition_bounds_[range.start()];
  for (size_t i = range.start() + 1; i < end_idx; ++i) {
    rect.Union(composition_bounds_[i]);
  }
  return rect;
}

gfx::Range RenderWidgetHostViewMac::ConvertCharacterRangeToCompositionRange(
    const gfx::Range& request_range) {
  if (composition_range_.is_empty())
    return gfx::Range::InvalidRange();

  if (request_range.is_reversed())
    return gfx::Range::InvalidRange();

  if (request_range.start() < composition_range_.start() ||
      request_range.start() > composition_range_.end() ||
      request_range.end() > composition_range_.end()) {
    return gfx::Range::InvalidRange();
  }

  return gfx::Range(
      request_range.start() - composition_range_.start(),
      request_range.end() - composition_range_.start());
}

WebContents* RenderWidgetHostViewMac::GetWebContents() {
  return WebContents::FromRenderViewHost(
      RenderViewHost::From(render_widget_host_));
}

bool RenderWidgetHostViewMac::GetCachedFirstRectForCharacterRange(
    NSRange range,
    NSRect* rect,
    NSRange* actual_range) {
  DCHECK(rect);
  // This exists to make IMEs more responsive, see http://crbug.com/115920
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewMac::GetFirstRectForCharacterRange");

  const gfx::Range requested_range(range);
  // If requested range is same as caret location, we can just return it.
  if (selection_range_.is_empty() && requested_range == selection_range_) {
    if (actual_range)
      *actual_range = range;
    *rect = NSRectFromCGRect(caret_rect_.ToCGRect());
    return true;
  }

  if (composition_range_.is_empty()) {
    if (!selection_range_.Contains(requested_range))
      return false;
    if (actual_range)
      *actual_range = selection_range_.ToNSRange();
    *rect = NSRectFromCGRect(first_selection_rect_.ToCGRect());
    return true;
  }

  const gfx::Range request_range_in_composition =
      ConvertCharacterRangeToCompositionRange(requested_range);
  if (request_range_in_composition == gfx::Range::InvalidRange())
    return false;

  // If firstRectForCharacterRange in WebFrame is failed in renderer,
  // ImeCompositionRangeChanged will be sent with empty vector.
  if (composition_bounds_.empty())
    return false;
  DCHECK_EQ(composition_bounds_.size(), composition_range_.length());

  gfx::Range ui_actual_range;
  *rect = NSRectFromCGRect(GetFirstRectForCompositionRange(
                               request_range_in_composition,
                               &ui_actual_range).ToCGRect());
  if (actual_range) {
    *actual_range = gfx::Range(
        composition_range_.start() + ui_actual_range.start(),
        composition_range_.start() + ui_actual_range.end()).ToNSRange();
  }
  return true;
}

bool RenderWidgetHostViewMac::HasAcceleratedSurface(
      const gfx::Size& desired_size) {
  if (browser_compositor_) {
    return browser_compositor_->accelerated_widget_mac()->HasFrameOfSize(
        desired_size);
  }
  return false;
}

void RenderWidgetHostViewMac::OnSwapCompositorFrame(
    uint32_t output_surface_id,
    scoped_ptr<cc::CompositorFrame> frame) {
  TRACE_EVENT0("browser", "RenderWidgetHostViewMac::OnSwapCompositorFrame");

  last_scroll_offset_ = frame->metadata.root_scroll_offset;

  page_at_minimum_scale_ = frame->metadata.page_scale_factor ==
                           frame->metadata.min_page_scale_factor;

  if (frame->delegated_frame_data) {
    float scale_factor = frame->metadata.device_scale_factor;

    // Compute the frame size based on the root render pass rect size.
    cc::RenderPass* root_pass =
        frame->delegated_frame_data->render_pass_list.back().get();
    gfx::Size pixel_size = root_pass->output_rect.size();
    gfx::Size dip_size = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

    root_layer_->SetBounds(gfx::Rect(dip_size));
    if (!render_widget_host_->is_hidden()) {
      EnsureBrowserCompositorView();
      browser_compositor_->compositor()->SetScaleAndSize(
          scale_factor, pixel_size);
    }

    SendVSyncParametersToRenderer();

    delegated_frame_host_->SwapDelegatedFrame(output_surface_id,
                                              std::move(frame));
  } else {
    DLOG(ERROR) << "Received unexpected frame type.";
    bad_message::ReceivedBadMessage(render_widget_host_->GetProcess(),
                                    bad_message::RWHVM_UNEXPECTED_FRAME_TYPE);
  }
}

void RenderWidgetHostViewMac::ClearCompositorFrame() {
  delegated_frame_host_->ClearDelegatedFrame();
}

void RenderWidgetHostViewMac::GetScreenInfo(blink::WebScreenInfo* results) {
  *results = GetWebScreenInfo(GetNativeView());
}

bool RenderWidgetHostViewMac::GetScreenColorProfile(
    std::vector<char>* color_profile) {
  DCHECK(color_profile->empty());
  NSWindow* window = GetWebContents()->GetTopLevelNativeWindow();
  return gfx::GetDisplayColorProfile(window, color_profile);
}

gfx::Rect RenderWidgetHostViewMac::GetBoundsInRootWindow() {
  // TODO(shess): In case of !window, the view has been removed from
  // the view hierarchy because the tab isn't main.  Could retrieve
  // the information from the main tab for our window.
  NSWindow* enclosing_window = ApparentWindowForView(cocoa_view_);
  if (!enclosing_window)
    return gfx::Rect();

  NSRect bounds = [enclosing_window frame];
  return FlipNSRectToRectScreen(bounds);
}

void RenderWidgetHostViewMac::LockCompositingSurface() {
  NOTIMPLEMENTED();
}

void RenderWidgetHostViewMac::UnlockCompositingSurface() {
  NOTIMPLEMENTED();
}

bool RenderWidgetHostViewMac::LockMouse() {
  if (mouse_locked_)
    return true;

  mouse_locked_ = true;

  // Lock position of mouse cursor and hide it.
  CGAssociateMouseAndMouseCursorPosition(NO);
  [NSCursor hide];

  // Clear the tooltip window.
  SetTooltipText(base::string16());

  return true;
}

void RenderWidgetHostViewMac::UnlockMouse() {
  if (!mouse_locked_)
    return;
  mouse_locked_ = false;

  // Unlock position of mouse cursor and unhide it.
  CGAssociateMouseAndMouseCursorPosition(YES);
  [NSCursor unhide];

  if (render_widget_host_)
    render_widget_host_->LostMouseLock();
}

void RenderWidgetHostViewMac::WheelEventAck(
    const blink::WebMouseWheelEvent& event,
    InputEventAckState ack_result) {
  bool consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
  // Only record a wheel event as unhandled if JavaScript handlers got a chance
  // to see it (no-op wheel events are ignored by the event dispatcher)
  if (event.deltaX || event.deltaY)
    [cocoa_view_ processedWheelEvent:event consumed:consumed];
}

scoped_ptr<SyntheticGestureTarget>
RenderWidgetHostViewMac::CreateSyntheticGestureTarget() {
  RenderWidgetHostImpl* host =
      RenderWidgetHostImpl::From(GetRenderWidgetHost());
  return scoped_ptr<SyntheticGestureTarget>(
      new SyntheticGestureTargetMac(host, cocoa_view_));
}

uint32_t RenderWidgetHostViewMac::GetSurfaceIdNamespace() {
  DCHECK(delegated_frame_host_);
  return delegated_frame_host_->GetSurfaceIdNamespace();
}

uint32_t RenderWidgetHostViewMac::SurfaceIdNamespaceAtPoint(
    const gfx::Point& point,
    gfx::Point* transformed_point) {
  // The surface hittest happens in device pixels, so we need to convert the
  // |point| from DIPs to pixels before hittesting.
  float scale_factor = gfx::Screen::GetScreenFor(cocoa_view_)
                           ->GetDisplayNearestWindow(cocoa_view_)
                           .device_scale_factor();
  gfx::Point point_in_pixels = gfx::ConvertPointToPixel(scale_factor, point);
  cc::SurfaceId id = delegated_frame_host_->SurfaceIdAtPoint(point_in_pixels,
                                                             transformed_point);
  *transformed_point = gfx::ConvertPointToDIP(scale_factor, *transformed_point);

  // It is possible that the renderer has not yet produced a surface, in which
  // case we return our current namespace.
  if (id.is_null())
    return GetSurfaceIdNamespace();
  return cc::SurfaceIdAllocator::NamespaceForId(id);
}

bool RenderWidgetHostViewMac::ShouldRouteEvent(
    const WebInputEvent& event) const {
  // See also RenderWidgetHostViewAura::ShouldRouteEvent.
  // TODO(wjmaclean): Update this function if RenderWidgetHostViewMac implements
  // OnTouchEvent(), to match what we are doing in RenderWidgetHostViewAura.
  DCHECK(WebInputEvent::isMouseEventType(event.type) ||
         event.type == WebInputEvent::MouseWheel);
  return render_widget_host_->delegate() &&
         render_widget_host_->delegate()->GetInputEventRouter() &&
         SiteIsolationPolicy::AreCrossProcessFramesPossible();
}

void RenderWidgetHostViewMac::ProcessMouseEvent(
    const blink::WebMouseEvent& event) {
  render_widget_host_->ForwardMouseEvent(event);
}
void RenderWidgetHostViewMac::ProcessMouseWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  render_widget_host_->ForwardWheelEvent(event);
}

void RenderWidgetHostViewMac::TransformPointToLocalCoordSpace(
    const gfx::Point& point,
    cc::SurfaceId original_surface,
    gfx::Point* transformed_point) {
  delegated_frame_host_->TransformPointToLocalCoordSpace(
      point, original_surface, transformed_point);
}

bool RenderWidgetHostViewMac::Send(IPC::Message* message) {
  if (render_widget_host_)
    return render_widget_host_->Send(message);
  delete message;
  return false;
}

void RenderWidgetHostViewMac::ShutdownHost() {
  weak_factory_.InvalidateWeakPtrs();
  render_widget_host_->ShutdownAndDestroyWidget(true);
  // Do not touch any members at this point, |this| has been deleted.
}

void RenderWidgetHostViewMac::ShutdownBrowserCompositor() {
  DestroyBrowserCompositorView();
  delegated_frame_host_.reset();
  root_layer_.reset();
  browser_compositor_placeholder_.reset();
}

void RenderWidgetHostViewMac::SetActive(bool active) {
  if (render_widget_host_) {
    render_widget_host_->SetActive(active);
    if (active) {
      if (HasFocus())
        render_widget_host_->Focus();
    } else {
      render_widget_host_->Blur();
    }
  }
  if (HasFocus())
    SetTextInputActive(active);
  if (!active) {
    [cocoa_view_ setPluginImeActive:NO];
    UnlockMouse();
  }
}

void RenderWidgetHostViewMac::SetWindowVisibility(bool visible) {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_SetWindowVisibility(
        render_widget_host_->GetRoutingID(), visible));
  }
}

void RenderWidgetHostViewMac::WindowFrameChanged() {
  if (render_widget_host_) {
    render_widget_host_->Send(new ViewMsg_WindowFrameChanged(
        render_widget_host_->GetRoutingID(), GetBoundsInRootWindow(),
        GetViewBounds()));
  }
}

void RenderWidgetHostViewMac::ShowDefinitionForSelection() {
  RenderWidgetHostViewMacDictionaryHelper helper(this);
  helper.ShowDefinitionForSelection();
}

void RenderWidgetHostViewMac::SetBackgroundColor(SkColor color) {
  RenderWidgetHostViewBase::SetBackgroundColor(color);
  bool opaque = GetBackgroundOpaque();

  if (render_widget_host_)
    render_widget_host_->SetBackgroundOpaque(opaque);

  [cocoa_view_ setOpaque:opaque];
  if (browser_compositor_state_ != BrowserCompositorDestroyed)
    browser_compositor_->compositor()->SetHostHasTransparentBackground(!opaque);
}

BrowserAccessibilityManager*
    RenderWidgetHostViewMac::CreateBrowserAccessibilityManager(
        BrowserAccessibilityDelegate* delegate) {
  return new BrowserAccessibilityManagerMac(
      cocoa_view_,
      BrowserAccessibilityManagerMac::GetEmptyDocument(),
      delegate);
}

gfx::Point RenderWidgetHostViewMac::AccessibilityOriginInScreen(
    const gfx::Rect& bounds) {
  NSPoint origin = NSMakePoint(bounds.x(), bounds.y());
  NSSize size = NSMakeSize(bounds.width(), bounds.height());
  origin.y = NSHeight([cocoa_view_ bounds]) - origin.y;
  NSPoint originInWindow = [cocoa_view_ convertPoint:origin toView:nil];
  NSPoint originInScreen =
      [[cocoa_view_ window] convertBaseToScreen:originInWindow];
  originInScreen.y = originInScreen.y - size.height;
  return gfx::Point(originInScreen.x, originInScreen.y);
}

void RenderWidgetHostViewMac::SetTextInputActive(bool active) {
  if (active) {
    if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD)
      EnablePasswordInput();
    else
      DisablePasswordInput();
  } else {
    if (text_input_type_ == ui::TEXT_INPUT_TYPE_PASSWORD)
      DisablePasswordInput();
  }
}

void RenderWidgetHostViewMac::OnPluginFocusChanged(bool focused,
                                                   int plugin_id) {
  [cocoa_view_ pluginFocusChanged:(focused ? YES : NO) forPlugin:plugin_id];
}

void RenderWidgetHostViewMac::OnStartPluginIme() {
  [cocoa_view_ setPluginImeActive:YES];
}

void RenderWidgetHostViewMac::OnGetRenderedTextCompleted(
    const std::string& text) {
  SpeakText(text);
}

void RenderWidgetHostViewMac::PauseForPendingResizeOrRepaintsAndDraw() {
  if (!render_widget_host_ || render_widget_host_->is_hidden())
    return;

  // Pausing for one view prevents others from receiving frames.
  // This may lead to large delays, causing overlaps. See crbug.com/352020.
  if (!allow_pause_for_resize_or_repaint_)
    return;

  // Wait for a frame of the right size to come in.
  render_widget_host_->PauseForPendingResizeOrRepaints();
}

////////////////////////////////////////////////////////////////////////////////
// gfx::DisplayObserver, public:

void RenderWidgetHostViewMac::OnDisplayAdded(const gfx::Display& display) {
}

void RenderWidgetHostViewMac::OnDisplayRemoved(const gfx::Display& display) {
}

void RenderWidgetHostViewMac::OnDisplayMetricsChanged(
    const gfx::Display& display, uint32_t metrics) {
  gfx::Screen* screen = gfx::Screen::GetScreenFor(cocoa_view_);
  if (display.id() != screen->GetDisplayNearestWindow(cocoa_view_).id())
    return;

  UpdateScreenInfo(cocoa_view_);
}

}  // namespace content

// RenderWidgetHostViewCocoa ---------------------------------------------------

@implementation RenderWidgetHostViewCocoa
@synthesize selectedRange = selectedRange_;
@synthesize suppressNextEscapeKeyUp = suppressNextEscapeKeyUp_;
@synthesize markedRange = markedRange_;

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)r {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    self.acceptsTouchEvents = YES;
    editCommand_helper_.reset(new RenderWidgetHostViewMacEditCommandHelper);
    editCommand_helper_->AddEditingSelectorsToClass([self class]);

    renderWidgetHostView_.reset(r);
    canBeKeyView_ = YES;
    opaque_ = YES;
    focusedPluginIdentifier_ = -1;
    pinchHasReachedZoomThreshold_ = false;

    // OpenGL support:
    if ([self respondsToSelector:
        @selector(setWantsBestResolutionOpenGLSurface:)]) {
      [self setWantsBestResolutionOpenGLSurface:YES];
    }
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didChangeScreenParameters:)
               name:NSApplicationDidChangeScreenParametersNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(viewGone:)])
    [responderDelegate_ viewGone:self];
  responderDelegate_.reset();

  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

- (void)didChangeScreenParameters:(NSNotification*)notify {
  g_screen_info_up_to_date = false;
}

- (void)setResponderDelegate:
            (NSObject<RenderWidgetHostViewMacDelegate>*)delegate {
  DCHECK(!responderDelegate_);
  responderDelegate_.reset([delegate retain]);
}

- (void)resetCursorRects {
  if (currentCursor_) {
    [self addCursorRect:[self visibleRect] cursor:currentCursor_];
    [currentCursor_ setOnMouseEntered:YES];
  }
}

- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed {
  [responderDelegate_ rendererHandledWheelEvent:event consumed:consumed];
}

- (BOOL)respondsToSelector:(SEL)selector {
  // Trickiness: this doesn't mean "does this object's superclass respond to
  // this selector" but rather "does the -respondsToSelector impl from the
  // superclass say that this class responds to the selector".
  if ([super respondsToSelector:selector])
    return YES;

  if (responderDelegate_)
    return [responderDelegate_ respondsToSelector:selector];

  return NO;
}

- (id)forwardingTargetForSelector:(SEL)selector {
  if ([responderDelegate_ respondsToSelector:selector])
    return responderDelegate_.get();

  return [super forwardingTargetForSelector:selector];
}

- (void)drawRect:(NSRect)dirty {
  if (content::g_force_cpu_draw) {
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextClipToRect(ctx, NSRectToCGRect(dirty));
    [[self layer] renderInContext:ctx];
  } else {
    [super drawRect:dirty];
  }
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (BOOL)acceptsMouseEventsWhenInactive {
  // Some types of windows (balloons, always-on-top panels) want to accept mouse
  // clicks w/o the first click being treated as 'activation'. Same applies to
  // mouse move events.
  return [[self window] level] > NSNormalWindowLevel;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return [self acceptsMouseEventsWhenInactive];
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (void)setOpaque:(BOOL)opaque {
  opaque_ = opaque;
}

- (BOOL)shouldIgnoreMouseEvent:(NSEvent*)theEvent {
  NSWindow* window = [self window];
  // If this is a background window, don't handle mouse movement events. This
  // is the expected behavior on the Mac as evidenced by other applications.
  if ([theEvent type] == NSMouseMoved &&
      ![self acceptsMouseEventsWhenInactive] &&
      ![window isKeyWindow]) {
    return YES;
  }

  // Use hitTest to check whether the mouse is over a nonWebContentView - in
  // which case the mouse event should not be handled by the render host.
  const SEL nonWebContentViewSelector = @selector(nonWebContentView);
  NSView* contentView = [window contentView];
  NSView* view = [contentView hitTest:[theEvent locationInWindow]];
  // Traverse the superview hierarchy as the hitTest will return the frontmost
  // view, such as an NSTextView, while nonWebContentView may be specified by
  // its parent view.
  while (view) {
    if ([view respondsToSelector:nonWebContentViewSelector] &&
        [view performSelector:nonWebContentViewSelector]) {
      // The cursor is over a nonWebContentView - ignore this mouse event.
      return YES;
    }
    if ([view isKindOfClass:[self class]] && ![view isEqual:self] &&
        !hasOpenMouseDown_) {
      // The cursor is over an overlapping render widget. This check is done by
      // both views so the one that's returned by -hitTest: will end up
      // processing the event.
      // Note that while dragging, we only get events for the render view where
      // drag started, even if mouse is  actually over another view or outside
      // the window. Cocoa does this for us. We should handle these events and
      // not ignore (since there is no other render view to handle them). Thus
      // the |!hasOpenMouseDown_| check above.
      return YES;
    }
    view = [view superview];
  }
  return NO;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::mouseEvent");
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return;
  }

  if ([self shouldIgnoreMouseEvent:theEvent]) {
    // If this is the first such event, send a mouse exit to the host view.
    if (!mouseEventWasIgnored_ && renderWidgetHostView_->render_widget_host_) {
      WebMouseEvent exitEvent = WebMouseEventBuilder::Build(theEvent, self);
      exitEvent.type = WebInputEvent::MouseLeave;
      exitEvent.button = WebMouseEvent::ButtonNone;
      renderWidgetHostView_->ForwardMouseEvent(exitEvent);
    }
    mouseEventWasIgnored_ = YES;
    return;
  }

  if (mouseEventWasIgnored_) {
    // If this is the first mouse event after a previous event that was ignored
    // due to the hitTest, send a mouse enter event to the host view.
    if (renderWidgetHostView_->render_widget_host_) {
      WebMouseEvent enterEvent = WebMouseEventBuilder::Build(theEvent, self);
      enterEvent.type = WebInputEvent::MouseMove;
      enterEvent.button = WebMouseEvent::ButtonNone;
      if (renderWidgetHostView_->ShouldRouteEvent(enterEvent)) {
        renderWidgetHostView_->render_widget_host_->delegate()
            ->GetInputEventRouter()
            ->RouteMouseEvent(renderWidgetHostView_.get(), &enterEvent);
      } else {
        renderWidgetHostView_->ProcessMouseEvent(enterEvent);
      }
    }
  }
  mouseEventWasIgnored_ = NO;

  // Don't cancel child popups; killing them on a mouse click would prevent the
  // user from positioning the insertion point in the text field spawning the
  // popup. A click outside the text field would cause the text field to drop
  // the focus, and then EditorClientImpl::textFieldDidEndEditing() would cancel
  // the popup anyway, so we're OK.

  NSEventType type = [theEvent type];
  if (type == NSLeftMouseDown)
    hasOpenMouseDown_ = YES;
  else if (type == NSLeftMouseUp)
    hasOpenMouseDown_ = NO;

  // TODO(suzhe): We should send mouse events to the input method first if it
  // wants to handle them. But it won't work without implementing method
  // - (NSUInteger)characterIndexForPoint:.
  // See: http://code.google.com/p/chromium/issues/detail?id=47141
  // Instead of sending mouse events to the input method first, we now just
  // simply confirm all ongoing composition here.
  if (type == NSLeftMouseDown || type == NSRightMouseDown ||
      type == NSOtherMouseDown) {
    [self confirmComposition];
  }

  WebMouseEvent event = WebMouseEventBuilder::Build(theEvent, self);
  if (renderWidgetHostView_->ShouldRouteEvent(event)) {
    renderWidgetHostView_->render_widget_host_->delegate()
        ->GetInputEventRouter()
        ->RouteMouseEvent(renderWidgetHostView_.get(), &event);
  } else {
    renderWidgetHostView_->ProcessMouseEvent(event);
  }
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  // |performKeyEquivalent:| is sent to all views of a window, not only down the
  // responder chain (cf. "Handling Key Equivalents" in
  // http://developer.apple.com/mac/library/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html
  // ). We only want to handle key equivalents if we're first responder.
  if ([[self window] firstResponder] != self)
    return NO;

  // If the event is reserved by the system, then do not pass it to web content.
  if (EventIsReservedBySystem(theEvent))
    return NO;

  // If we return |NO| from this function, cocoa will send the key event to
  // the menu and only if the menu does not process the event to |keyDown:|. We
  // want to send the event to a renderer _before_ sending it to the menu, so
  // we need to return |YES| for all events that might be swallowed by the menu.
  // We do not return |YES| for every keypress because we don't get |keyDown:|
  // events for keys that we handle this way.
  NSUInteger modifierFlags = [theEvent modifierFlags];
  if ((modifierFlags & NSCommandKeyMask) == 0) {
    // Make sure the menu does not contain key equivalents that don't
    // contain cmd.
    DCHECK(![[NSApp mainMenu] performKeyEquivalent:theEvent]);
    return NO;
  }

  // Command key combinations are sent via performKeyEquivalent rather than
  // keyDown:. We just forward this on and if WebCore doesn't want to handle
  // it, we let the WebContentsView figure out how to reinject it.
  [self keyEvent:theEvent wasKeyEquivalent:YES];
  return YES;
}

- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  // This is a SPI that AppKit apparently calls after |performKeyEquivalent:|
  // returned NO. If this function returns |YES|, Cocoa sends the event to
  // |keyDown:| instead of doing other things with it. Ctrl-tab will be sent
  // to us instead of doing key view loop control, ctrl-left/right get handled
  // correctly, etc.
  // (However, there are still some keys that Cocoa swallows, e.g. the key
  // equivalent that Cocoa uses for toggling the input language. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return kEventHandled;
  }

  [self keyEvent:theEvent wasKeyEquivalent:NO];
  return kEventHandled;
}

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::keyEvent");

  // If the user changes the system hotkey mapping after Chrome has been
  // launched, then it is possible that a formerly reserved system hotkey is no
  // longer reserved. The hotkey would have skipped the renderer, but would
  // also have not been handled by the system. If this is the case, immediately
  // return.
  // TODO(erikchen): SystemHotkeyHelperMac should use the File System Events
  // api to monitor changes to system hotkeys. This logic will have to be
  // updated.
  // http://crbug.com/383558.
  if (EventIsReservedBySystem(theEvent))
    return;

  DCHECK([theEvent type] != NSKeyDown ||
         !equiv == !([theEvent modifierFlags] & NSCommandKeyMask));

  if ([theEvent type] == NSFlagsChanged) {
    // Ignore NSFlagsChanged events from the NumLock and Fn keys as
    // Safari does in -[WebHTMLView flagsChanged:] (of "WebHTMLView.mm").
    int keyCode = [theEvent keyCode];
    if (!keyCode || keyCode == 10 || keyCode == 63)
      return;
  }

  // Don't cancel child popups; the key events are probably what's triggering
  // the popup in the first place.

  RenderWidgetHostImpl* widgetHost = renderWidgetHostView_->render_widget_host_;
  DCHECK(widgetHost);

  NativeWebKeyboardEvent event(theEvent);

  // Force fullscreen windows to close on Escape so they won't keep the keyboard
  // grabbed or be stuck onscreen if the renderer is hanging.
  if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE &&
      renderWidgetHostView_->pepper_fullscreen_window()) {
    RenderWidgetHostViewMac* parent =
        renderWidgetHostView_->fullscreen_parent_host_view();
    if (parent)
      parent->cocoa_view()->suppressNextEscapeKeyUp_ = YES;
    widgetHost->ShutdownAndDestroyWidget(true);
    return;
  }

  // If there are multiple widgets on the page (such as when there are
  // out-of-process iframes), pick the one that should process this event.
  if (widgetHost->delegate())
    widgetHost = widgetHost->delegate()->GetFocusedRenderWidgetHost(widgetHost);
  if (!widgetHost)
    return;

  // Suppress the escape key up event if necessary.
  if (event.windowsKeyCode == ui::VKEY_ESCAPE && suppressNextEscapeKeyUp_) {
    if (event.type == NativeWebKeyboardEvent::KeyUp)
      suppressNextEscapeKeyUp_ = NO;
    return;
  }

  // Do not forward key up events unless preceded by a matching key down,
  // otherwise we might get an event from releasing the return key in the
  // omnibox (http://crbug.com/338736).
  if ([theEvent type] == NSKeyUp) {
    auto numErased = keyDownCodes_.erase([theEvent keyCode]);
    if (numErased < 1)
      return;
  }

  // We only handle key down events and just simply forward other events.
  if ([theEvent type] != NSKeyDown) {
    widgetHost->ForwardKeyboardEvent(event);

    // Possibly autohide the cursor.
    if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
      [NSCursor setHiddenUntilMouseMoves:YES];

    return;
  }

  keyDownCodes_.insert([theEvent keyCode]);

  base::scoped_nsobject<RenderWidgetHostViewCocoa> keepSelfAlive([self retain]);

  // Records the current marked text state, so that we can know if the marked
  // text was deleted or not after handling the key down event.
  BOOL oldHasMarkedText = hasMarkedText_;

  // This method should not be called recursively.
  DCHECK(!handlingKeyDown_);

  // Tells insertText: and doCommandBySelector: that we are handling a key
  // down event.
  handlingKeyDown_ = YES;

  // These variables might be set when handling the keyboard event.
  // Clear them here so that we can know whether they have changed afterwards.
  textToBeInserted_.clear();
  markedText_.clear();
  markedTextSelectedRange_ = NSMakeRange(NSNotFound, 0);
  underlines_.clear();
  unmarkTextCalled_ = NO;
  hasEditCommands_ = NO;
  editCommands_.clear();

  // Before doing anything with a key down, check to see if plugin IME has been
  // cancelled, since the plugin host needs to be informed of that before
  // receiving the keydown.
  if ([theEvent type] == NSKeyDown)
    [self checkForPluginImeCancellation];

  // Sends key down events to input method first, then we can decide what should
  // be done according to input method's feedback.
  // If a plugin is active, bypass this step since events are forwarded directly
  // to the plugin IME.
  if (focusedPluginIdentifier_ == -1)
    [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];

  handlingKeyDown_ = NO;

  // Indicates if we should send the key event and corresponding editor commands
  // after processing the input method result.
  BOOL delayEventUntilAfterImeCompostion = NO;

  // To emulate Windows, over-write |event.windowsKeyCode| to VK_PROCESSKEY
  // while an input method is composing or inserting a text.
  // Gmail checks this code in its onkeydown handler to stop auto-completing
  // e-mail addresses while composing a CJK text.
  // If the text to be inserted has only one character, then we don't need this
  // trick, because we'll send the text as a key press event instead.
  if (hasMarkedText_ || oldHasMarkedText || textToBeInserted_.length() > 1) {
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.windowsKeyCode = 0xE5;  // VKEY_PROCESSKEY
    fakeEvent.setKeyIdentifierFromWindowsKeyCode();
    fakeEvent.skip_in_browser = true;
    widgetHost->ForwardKeyboardEvent(fakeEvent);
    // If this key event was handled by the input method, but
    // -doCommandBySelector: (invoked by the call to -interpretKeyEvents: above)
    // enqueued edit commands, then in order to let webkit handle them
    // correctly, we need to send the real key event and corresponding edit
    // commands after processing the input method result.
    // We shouldn't do this if a new marked text was set by the input method,
    // otherwise the new marked text might be cancelled by webkit.
    if (hasEditCommands_ && !hasMarkedText_)
      delayEventUntilAfterImeCompostion = YES;
  } else {
    if (!editCommands_.empty()) {
      widgetHost->Send(new InputMsg_SetEditCommandsForNextKeyEvent(
          widgetHost->GetRoutingID(), editCommands_));
    }
    widgetHost->ForwardKeyboardEvent(event);
  }

  // Calling ForwardKeyboardEvent() could have destroyed the widget. When the
  // widget was destroyed, |renderWidgetHostView_->render_widget_host_| will
  // be set to NULL. So we check it here and return immediately if it's NULL.
  if (!renderWidgetHostView_->render_widget_host_)
    return;

  // Then send keypress and/or composition related events.
  // If there was a marked text or the text to be inserted is longer than 1
  // character, then we send the text by calling ConfirmComposition().
  // Otherwise, if the text to be inserted only contains 1 character, then we
  // can just send a keypress event which is fabricated by changing the type of
  // the keydown event, so that we can retain all necessary informations, such
  // as unmodifiedText, etc. And we need to set event.skip_in_browser to true to
  // prevent the browser from handling it again.
  // Note that, |textToBeInserted_| is a UTF-16 string, but it's fine to only
  // handle BMP characters here, as we can always insert non-BMP characters as
  // text.
  BOOL textInserted = NO;
  if (textToBeInserted_.length() >
      ((hasMarkedText_ || oldHasMarkedText) ? 0u : 1u)) {
    widgetHost->ImeConfirmComposition(
        textToBeInserted_, gfx::Range::InvalidRange(), false);
    textInserted = YES;
  }

  // Updates or cancels the composition. If some text has been inserted, then
  // we don't need to cancel the composition explicitly.
  if (hasMarkedText_ && markedText_.length()) {
    // Sends the updated marked text to the renderer so it can update the
    // composition node in WebKit.
    // When marked text is available, |markedTextSelectedRange_| will be the
    // range being selected inside the marked text.
    widgetHost->ImeSetComposition(markedText_, underlines_,
                                  markedTextSelectedRange_.location,
                                  NSMaxRange(markedTextSelectedRange_));
  } else if (oldHasMarkedText && !hasMarkedText_ && !textInserted) {
    if (unmarkTextCalled_) {
      widgetHost->ImeConfirmComposition(
          base::string16(), gfx::Range::InvalidRange(), false);
    } else {
      widgetHost->ImeCancelComposition();
    }
  }

  // If the key event was handled by the input method but it also generated some
  // edit commands, then we need to send the real key event and corresponding
  // edit commands here. This usually occurs when the input method wants to
  // finish current composition session but still wants the application to
  // handle the key event. See http://crbug.com/48161 for reference.
  if (delayEventUntilAfterImeCompostion) {
    // If |delayEventUntilAfterImeCompostion| is YES, then a fake key down event
    // with windowsKeyCode == 0xE5 has already been sent to webkit.
    // So before sending the real key down event, we need to send a fake key up
    // event to balance it.
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.type = blink::WebInputEvent::KeyUp;
    fakeEvent.skip_in_browser = true;
    widgetHost->ForwardKeyboardEvent(fakeEvent);
    // Not checking |renderWidgetHostView_->render_widget_host_| here because
    // a key event with |skip_in_browser| == true won't be handled by browser,
    // thus it won't destroy the widget.

    if (!editCommands_.empty()) {
      widgetHost->Send(new InputMsg_SetEditCommandsForNextKeyEvent(
          widgetHost->GetRoutingID(), editCommands_));
    }
    widgetHost->ForwardKeyboardEvent(event);

    // Calling ForwardKeyboardEvent() could have destroyed the widget. When the
    // widget was destroyed, |renderWidgetHostView_->render_widget_host_| will
    // be set to NULL. So we check it here and return immediately if it's NULL.
    if (!renderWidgetHostView_->render_widget_host_)
      return;
  }

  const NSUInteger kCtrlCmdKeyMask = NSControlKeyMask | NSCommandKeyMask;
  // Only send a corresponding key press event if there is no marked text.
  if (!hasMarkedText_) {
    if (!textInserted && textToBeInserted_.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      event.type = blink::WebInputEvent::Char;
      event.text[0] = textToBeInserted_[0];
      event.text[1] = 0;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEvent(event);
    } else if ((!textInserted || delayEventUntilAfterImeCompostion) &&
               event.text[0] != '\0' &&
               (([theEvent modifierFlags] & kCtrlCmdKeyMask) ||
                (hasEditCommands_ && editCommands_.empty()))) {
      // We don't get insertText: calls if ctrl or cmd is down, or the key event
      // generates an insert command. So synthesize a keypress event for these
      // cases, unless the key event generated any other command.
      event.type = blink::WebInputEvent::Char;
      event.skip_in_browser = true;
      widgetHost->ForwardKeyboardEvent(event);
    }
  }

  // Possibly autohide the cursor.
  if ([RenderWidgetHostViewCocoa shouldAutohideCursorForEvent:theEvent])
    [NSCursor setHiddenUntilMouseMoves:YES];
}

- (void)forceTouchEvent:(NSEvent*)theEvent {
  [self quickLookWithEvent:theEvent];
}

- (void)shortCircuitScrollWheelEvent:(NSEvent*)event {
  DCHECK(base::mac::IsOSLionOrLater());

  if ([event phase] != NSEventPhaseEnded &&
      [event phase] != NSEventPhaseCancelled) {
    return;
  }

  if (renderWidgetHostView_->render_widget_host_) {
    // History-swiping is not possible if the logic reaches this point.
    // Allow rubber-banding in both directions.
    bool canRubberbandLeft = true;
    bool canRubberbandRight = true;
    WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(
        event, self, canRubberbandLeft, canRubberbandRight);
    webEvent.railsMode = mouseWheelFilter_.UpdateRailsMode(webEvent);
    renderWidgetHostView_->render_widget_host_->ForwardWheelEvent(webEvent);
  }

  if (endWheelMonitor_) {
    [NSEvent removeMonitor:endWheelMonitor_];
    endWheelMonitor_ = nil;
  }
}

- (void)beginGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ beginGestureWithEvent:event];
  gestureBeginEvent_.reset(
      new WebGestureEvent(WebGestureEventBuilder::Build(event, self)));

  // If the page is at the minimum zoom level, require a threshold be reached
  // before the pinch has an effect.
  if (renderWidgetHostView_->page_at_minimum_scale_) {
    pinchHasReachedZoomThreshold_ = false;
    pinchUnusedAmount_ = 1;
  }
}

- (void)endGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ endGestureWithEvent:event];
  gestureBeginEvent_.reset();

  if (!renderWidgetHostView_->render_widget_host_)
    return;

  if (gestureBeginPinchSent_) {
    WebGestureEvent endEvent(WebGestureEventBuilder::Build(event, self));
    endEvent.type = WebInputEvent::GesturePinchEnd;
    renderWidgetHostView_->render_widget_host_->ForwardGestureEvent(endEvent);
    gestureBeginPinchSent_ = NO;
  }
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesEndedWithEvent:event];
}

- (void)smartMagnifyWithEvent:(NSEvent*)event {
  const WebGestureEvent& smartMagnifyEvent =
      WebGestureEventBuilder::Build(event, self);
  if (renderWidgetHostView_ && renderWidgetHostView_->render_widget_host_) {
    renderWidgetHostView_->render_widget_host_->ForwardGestureEvent(
        smartMagnifyEvent);
  }
}

- (void)showLookUpDictionaryOverlayInternal:(NSAttributedString*) string
                              baselinePoint:(NSPoint) baselinePoint
                                 targetView:(NSView*) view {
  if ([string length] == 0) {
    // The PDF plugin does not support getting the attributed string at point.
    // Until it does, use NSPerformService(), which opens Dictionary.app.
    // TODO(shuchen): Support GetStringAtPoint() & GetStringFromRange() for PDF.
    // See crbug.com/152438.
    NSString* text = base::SysUTF8ToNSString(
        renderWidgetHostView_->selected_text());
    if ([text length] == 0)
      return;
    NSPasteboard* pasteboard = [NSPasteboard pasteboardWithUniqueName];
    NSArray* types = [NSArray arrayWithObject:NSStringPboardType];
    [pasteboard declareTypes:types owner:nil];
    if ([pasteboard setString:text forType:NSStringPboardType])
      NSPerformService(@"Look Up in Dictionary", pasteboard);
    return;
  }
  dispatch_async(dispatch_get_main_queue(), ^{
    [view showDefinitionForAttributedString:string
                                    atPoint:baselinePoint];
  });
}

- (void)showLookUpDictionaryOverlayFromRange:(NSRange)range
                                  targetView:(NSView*)targetView {
  TextInputClientMac::GetInstance()->GetStringFromRange(
      renderWidgetHostView_->render_widget_host_, range,
      ^(NSAttributedString* string, NSPoint baselinePoint) {
        [self showLookUpDictionaryOverlayInternal:string
                                    baselinePoint:baselinePoint
                                       targetView:targetView];
      }
  );
}

- (void)showLookUpDictionaryOverlayAtPoint:(NSPoint)point {
  TextInputClientMac::GetInstance()->GetStringAtPoint(
      renderWidgetHostView_->render_widget_host_,
      gfx::Point(point.x, NSHeight([self frame]) - point.y),
      ^(NSAttributedString* string, NSPoint baselinePoint) {
        [self showLookUpDictionaryOverlayInternal:string
                                    baselinePoint:baselinePoint
                                       targetView:self];
      }
  );
}

// This is invoked only on 10.8 or newer when the user taps a word using
// three fingers.
- (void)quickLookWithEvent:(NSEvent*)event {
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  [self showLookUpDictionaryOverlayAtPoint:point];
}

// This method handles 2 different types of hardware events.
// (Apple does not distinguish between them).
//  a. Scrolling the middle wheel of a mouse.
//  b. Swiping on the track pad.
//
// This method is responsible for 2 types of behavior:
//  a. Scrolling the content of window.
//  b. Navigating forwards/backwards in history.
//
// This is a brief description of the logic:
//  1. If the content can be scrolled, scroll the content.
//     (This requires a roundtrip to blink to determine whether the content
//      can be scrolled.)
//     Once this logic is triggered, the navigate logic cannot be triggered
//     until the gesture finishes.
//  2. If the user is making a horizontal swipe, start the navigate
//     forward/backwards UI.
//     Once this logic is triggered, the user can either cancel or complete
//     the gesture. If the user completes the gesture, all remaining touches
//     are swallowed, and not allowed to scroll the content. If the user
//     cancels the gesture, all remaining touches are forwarded to the content
//     scroll logic. The user cannot trigger the navigation logic again.
- (void)scrollWheel:(NSEvent*)event {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:event];
    if (handled)
      return;
  }

  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the view
  // when the scrolling ends (e.g. if the tab was switched). This is necessary
  // for ending rubber-banding in such cases.
  if (base::mac::IsOSLionOrLater() && [event phase] == NSEventPhaseBegan &&
      !endWheelMonitor_) {
    endWheelMonitor_ =
      [NSEvent addLocalMonitorForEventsMatchingMask:NSScrollWheelMask
      handler:^(NSEvent* blockEvent) {
          [self shortCircuitScrollWheelEvent:blockEvent];
          return blockEvent;
      }];
  }

  // This is responsible for content scrolling!
  if (renderWidgetHostView_->render_widget_host_) {
    BOOL canRubberbandLeft = [responderDelegate_ canRubberbandLeft:self];
    BOOL canRubberbandRight = [responderDelegate_ canRubberbandRight:self];
    WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(
        event, self, canRubberbandLeft, canRubberbandRight);
    webEvent.railsMode = mouseWheelFilter_.UpdateRailsMode(webEvent);
    if (renderWidgetHostView_->ShouldRouteEvent(webEvent)) {
      renderWidgetHostView_->render_widget_host_->delegate()
          ->GetInputEventRouter()
          ->RouteMouseWheelEvent(renderWidgetHostView_.get(), &webEvent);
    } else {
      renderWidgetHostView_->ProcessMouseWheelEvent(webEvent);
    }
  }
}

// Called repeatedly during a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
  if (!renderWidgetHostView_->render_widget_host_)
    return;

  // If, due to nesting of multiple gestures (e.g, from multiple touch
  // devices), the beginning of the gesture has been lost, skip the remainder
  // of the gesture.
  if (!gestureBeginEvent_)
    return;

  if (!pinchHasReachedZoomThreshold_) {
      pinchUnusedAmount_ *= (1 + [event magnification]);
      if (pinchUnusedAmount_ < 0.667 || pinchUnusedAmount_ > 1.5)
          pinchHasReachedZoomThreshold_ = true;
  }

  // Send a GesturePinchBegin event if none has been sent yet.
  if (!gestureBeginPinchSent_) {
    WebGestureEvent beginEvent(*gestureBeginEvent_);
    beginEvent.type = WebInputEvent::GesturePinchBegin;
    renderWidgetHostView_->render_widget_host_->ForwardGestureEvent(beginEvent);
    gestureBeginPinchSent_ = YES;
  }

  // Send a GesturePinchUpdate event.
  WebGestureEvent updateEvent = WebGestureEventBuilder::Build(event, self);
  updateEvent.data.pinchUpdate.zoomDisabled = !pinchHasReachedZoomThreshold_;
  renderWidgetHostView_->render_widget_host_->ForwardGestureEvent(updateEvent);
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSWindow* oldWindow = [self window];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  // Backing property notifications crash on 10.6 when building with the 10.7
  // SDK, see http://crbug.com/260595.
  static BOOL supportsBackingPropertiesNotification =
      SupportsBackingPropertiesChangedNotification();

  if (oldWindow) {
    if (supportsBackingPropertiesNotification) {
      [notificationCenter
          removeObserver:self
                    name:NSWindowDidChangeBackingPropertiesNotification
                  object:oldWindow];
    }
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidMoveNotification
                object:oldWindow];
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidEndLiveResizeNotification
                object:oldWindow];
  }
  if (newWindow) {
    if (supportsBackingPropertiesNotification) {
      [notificationCenter
          addObserver:self
             selector:@selector(windowDidChangeBackingProperties:)
                 name:NSWindowDidChangeBackingPropertiesNotification
               object:newWindow];
    }
    [notificationCenter
        addObserver:self
           selector:@selector(windowChangedGlobalFrame:)
               name:NSWindowDidMoveNotification
             object:newWindow];
    [notificationCenter
        addObserver:self
           selector:@selector(windowChangedGlobalFrame:)
               name:NSWindowDidEndLiveResizeNotification
             object:newWindow];
  }
}

- (void)updateScreenProperties{
  renderWidgetHostView_->UpdateBackingStoreProperties();
  renderWidgetHostView_->UpdateDisplayLink();
}

// http://developer.apple.com/library/mac/#documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/CapturingScreenContents/CapturingScreenContents.html#//apple_ref/doc/uid/TP40012302-CH10-SW4
- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  // Background tabs check if their screen scale factor, color profile, and
  // vsync properties changed when they are added to a window.

  // Allocating a CGLayerRef with the current scale factor immediately from
  // this handler doesn't work. Schedule the backing store update on the
  // next runloop cycle, then things are read for CGLayerRef allocations to
  // work.
  [self performSelector:@selector(updateScreenProperties)
             withObject:nil
             afterDelay:0];
}

- (void)windowChangedGlobalFrame:(NSNotification*)notification {
  renderWidgetHostView_->UpdateScreenInfo(
      renderWidgetHostView_->GetNativeView());
}

- (void)setFrameSize:(NSSize)newSize {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::setFrameSize");

  // NB: -[NSView setFrame:] calls through -setFrameSize:, so overriding
  // -setFrame: isn't neccessary.
  [super setFrameSize:newSize];

  if (!renderWidgetHostView_->render_widget_host_)
    return;

  renderWidgetHostView_->render_widget_host_->SendScreenRects();
  renderWidgetHostView_->render_widget_host_->WasResized();
  if (renderWidgetHostView_->delegated_frame_host_)
    renderWidgetHostView_->delegated_frame_host_->WasResized();

  // Wait for the frame that WasResize might have requested. If the view is
  // being made visible at a new size, then this call will have no effect
  // because the view widget is still hidden, and the pause call in WasShown
  // will have this effect for us.
  renderWidgetHostView_->PauseForPendingResizeOrRepaintsAndDraw();
}

- (BOOL)canBecomeKeyView {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  return canBeKeyView_;
}

- (BOOL)becomeFirstResponder {
  if (!renderWidgetHostView_->render_widget_host_)
    return NO;

  renderWidgetHostView_->render_widget_host_->Focus();
  renderWidgetHostView_->SetTextInputActive(true);

  // Cancel any onging composition text which was left before we lost focus.
  // TODO(suzhe): We should do it in -resignFirstResponder: method, but
  // somehow that method won't be called when switching among different tabs.
  // See http://crbug.com/47209
  [self cancelComposition];

  NSNumber* direction = [NSNumber numberWithUnsignedInteger:
      [[self window] keyViewSelectionDirection]];
  NSDictionary* userInfo =
      [NSDictionary dictionaryWithObject:direction
                                  forKey:kSelectionDirection];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kViewDidBecomeFirstResponder
                    object:self
                  userInfo:userInfo];

  return YES;
}

- (BOOL)resignFirstResponder {
  renderWidgetHostView_->SetTextInputActive(false);
  if (!renderWidgetHostView_->render_widget_host_)
    return YES;

  if (closeOnDeactivate_)
    renderWidgetHostView_->KillSelf();

  renderWidgetHostView_->render_widget_host_->Blur();

  // We should cancel any onging composition whenever RWH's Blur() method gets
  // called, because in this case, webkit will confirm the ongoing composition
  // internally.
  [self cancelComposition];

  return YES;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  if (responderDelegate_ &&
      [responderDelegate_
          respondsToSelector:@selector(validateUserInterfaceItem:
                                                     isValidItem:)]) {
    BOOL valid;
    BOOL known =
        [responderDelegate_ validateUserInterfaceItem:item isValidItem:&valid];
    if (known)
      return valid;
  }

  SEL action = [item action];
  BOOL is_render_view =
      RenderViewHost::From(renderWidgetHostView_->render_widget_host_) !=
      nullptr;

  if (action == @selector(stopSpeaking:))
    return is_render_view && renderWidgetHostView_->IsSpeaking();

  if (action == @selector(startSpeaking:))
    return is_render_view && renderWidgetHostView_->SupportsSpeech();

  // For now, these actions are always enabled for render view,
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(undo:) ||
      action == @selector(redo:) ||
      action == @selector(cut:) ||
      action == @selector(copy:) ||
      action == @selector(copyToFindPboard:) ||
      action == @selector(paste:) ||
      action == @selector(pasteAndMatchStyle:)) {
    return is_render_view;
  }

  return editCommand_helper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return renderWidgetHostView_.get();
}

// Determine whether we should autohide the cursor (i.e., hide it until mouse
// move) for the given event. Customize here to be more selective about which
// key presses to autohide on.
+ (BOOL)shouldAutohideCursorForEvent:(NSEvent*)event {
  return ([event type] == NSKeyDown &&
             !([event modifierFlags] & NSCommandKeyMask)) ? YES : NO;
}

- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute
                                         index:(NSUInteger)index
                                      maxCount:(NSUInteger)maxCount {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  NSUInteger totalLength = [fullArray count];
  if (index >= totalLength)
    return nil;
  NSUInteger length = MIN(totalLength - index, maxCount);
  return [fullArray subarrayWithRange:NSMakeRange(index, length)];
}

- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute {
  NSArray* fullArray = [self accessibilityAttributeValue:attribute];
  return [fullArray count];
}

- (id)accessibilityAttributeValue:(NSString *)attribute {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->render_widget_host_
          ->GetRootBrowserAccessibilityManager();

  // Contents specifies document view of RenderWidgetHostViewCocoa provided by
  // BrowserAccessibilityManager. Children includes all subviews in addition to
  // contents. Currently we do not have subviews besides the document view.
  if (([attribute isEqualToString:NSAccessibilityChildrenAttribute] ||
          [attribute isEqualToString:NSAccessibilityContentsAttribute]) &&
      manager) {
    return [NSArray arrayWithObjects:manager->
        GetRoot()->ToBrowserAccessibilityCocoa(), nil];
  } else if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return NSAccessibilityScrollAreaRole;
  }
  id ret = [super accessibilityAttributeValue:attribute];
  return ret;
}

- (NSArray*)accessibilityAttributeNames {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [ret addObject:NSAccessibilityContentsAttribute];
  [ret addObjectsFromArray:[super accessibilityAttributeNames]];
  return ret;
}

- (id)accessibilityHitTest:(NSPoint)point {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->render_widget_host_
          ->GetRootBrowserAccessibilityManager();
  if (!manager)
    return self;
  NSPoint pointInWindow = [[self window] convertScreenToBase:point];
  NSPoint localPoint = [self convertPoint:pointInWindow fromView:nil];
  localPoint.y = NSHeight([self bounds]) - localPoint.y;
  BrowserAccessibilityCocoa* root =
      manager->GetRoot()->ToBrowserAccessibilityCocoa();
  id obj = [root accessibilityHitTest:localPoint];
  return obj;
}

- (BOOL)accessibilityIsIgnored {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->render_widget_host_
          ->GetRootBrowserAccessibilityManager();
  return !manager;
}

- (NSUInteger)accessibilityGetIndexOf:(id)child {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->render_widget_host_
          ->GetRootBrowserAccessibilityManager();
  // Only child is root.
  if (manager &&
      manager->GetRoot()->ToBrowserAccessibilityCocoa() == child) {
    return 0;
  } else {
    return NSNotFound;
  }
}

- (id)accessibilityFocusedUIElement {
  BrowserAccessibilityManager* manager =
      renderWidgetHostView_->render_widget_host_
          ->GetRootBrowserAccessibilityManager();
  if (manager) {
    BrowserAccessibility* focused_item = manager->GetFocus(NULL);
    DCHECK(focused_item);
    if (focused_item) {
      BrowserAccessibilityCocoa* focused_item_cocoa =
          focused_item->ToBrowserAccessibilityCocoa();
      DCHECK(focused_item_cocoa);
      if (focused_item_cocoa)
        return focused_item_cocoa;
    }
  }
  return [super accessibilityFocusedUIElement];
}

// Below is our NSTextInputClient implementation.
//
// When WebHTMLView receives a NSKeyDown event, WebHTMLView calls the following
// functions to process this event.
//
// [WebHTMLView keyDown] ->
//     EventHandler::keyEvent() ->
//     ...
//     [WebEditorClient handleKeyboardEvent] ->
//     [WebHTMLView _interceptEditingKeyEvent] ->
//     [NSResponder interpretKeyEvents] ->
//     [WebHTMLView insertText] ->
//     Editor::insertText()
//
// Unfortunately, it is hard for Chromium to use this implementation because
// it causes key-typing jank.
// RenderWidgetHostViewMac is running in a browser process. On the other
// hand, Editor and EventHandler are running in a renderer process.
// So, if we used this implementation, a NSKeyDown event is dispatched to
// the following functions of Chromium.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Sync IPC (KeyDown)| (*1) ->
//     EventHandler::keyEvent() (renderer) ->
//     ...
//     EditorClientImpl::handleKeyboardEvent() (renderer) ->
//     |Sync IPC| (*2) ->
//     [RenderWidgetHostViewMac _interceptEditingKeyEvent] (browser) ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC| ->
//     Editor::insertText() (renderer)
//
// (*1) we need to wait until this call finishes since WebHTMLView uses the
// result of EventHandler::keyEvent().
// (*2) we need to wait until this call finishes since WebEditorClient uses
// the result of [WebHTMLView _interceptEditingKeyEvent].
//
// This needs many sync IPC messages sent between a browser and a renderer for
// each key event, which would probably result in key-typing jank.
// To avoid this problem, this implementation processes key events (and input
// method events) totally in a browser process and sends asynchronous input
// events, almost same as KeyboardEvents (and TextEvents) of DOM Level 3, to a
// renderer process.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Async IPC (RawKeyDown)| ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC (Char)| ->
//     Editor::insertText() (renderer)
//
// Since this implementation doesn't have to wait any IPC calls, this doesn't
// make any key-typing jank. --hbono 7/23/09
//
extern "C" {
extern NSString *NSTextInputReplacementRangeAttributeName;
}

- (NSArray *)validAttributesForMarkedText {
  // This code is just copied from WebKit except renaming variables.
  if (!validAttributesForMarkedText_) {
    validAttributesForMarkedText_.reset([[NSArray alloc] initWithObjects:
        NSUnderlineStyleAttributeName,
        NSUnderlineColorAttributeName,
        NSMarkedClauseSegmentAttributeName,
        NSTextInputReplacementRangeAttributeName,
        nil]);
  }
  return validAttributesForMarkedText_.get();
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  DCHECK([self window]);
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.
  thePoint = [[self window] convertScreenToBase:thePoint];
  thePoint = [self convertPoint:thePoint fromView:nil];
  thePoint.y = NSHeight([self frame]) - thePoint.y;

  NSUInteger index =
      TextInputClientMac::GetInstance()->GetCharacterIndexAtPoint(
          renderWidgetHostView_->render_widget_host_,
          gfx::Point(thePoint.x, thePoint.y));
  return index;
}

- (NSRect)firstViewRectForCharacterRange:(NSRange)theRange
                             actualRange:(NSRangePointer)actualRange {
  NSRect rect;
  if (!renderWidgetHostView_->GetCachedFirstRectForCharacterRange(
          theRange,
          &rect,
          actualRange)) {
    rect = TextInputClientMac::GetInstance()->GetFirstRectForRange(
        renderWidgetHostView_->render_widget_host_, theRange);

    // TODO(thakis): Pipe |actualRange| through TextInputClientMac machinery.
    if (actualRange)
      *actualRange = theRange;
  }

  // The returned rectangle is in WebKit coordinates (upper left origin), so
  // flip the coordinate system.
  NSRect viewFrame = [self frame];
  rect.origin.y = NSHeight(viewFrame) - NSMaxY(rect);
  return rect;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer)actualRange {
  // During tab closure, events can arrive after RenderWidgetHostViewMac::
  // Destroy() is called, which will have set |render_widget_host_| to null.
  if (!renderWidgetHostView_->render_widget_host_) {
    [self cancelComposition];
    return NSZeroRect;
  }

  NSRect rect = [self firstViewRectForCharacterRange:theRange
                                         actualRange:actualRange];

  // Convert into screen coordinates for return.
  rect = [self convertRect:rect toView:nil];
  rect.origin = [[self window] convertBaseToScreen:rect.origin];
  return rect;
}

- (NSRange)markedRange {
  // An input method calls this method to check if an application really has
  // a text being composed when hasMarkedText call returns true.
  // Returns the range saved in the setMarkedText method so the input method
  // calls the setMarkedText method and we can update the composition node
  // there. (When this method returns an empty range, the input method doesn't
  // call the setMarkedText method.)
  return hasMarkedText_ ? markedRange_ : NSMakeRange(NSNotFound, 0);
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
    actualRange:(NSRangePointer)actualRange {
  // TODO(thakis): Pipe |actualRange| through TextInputClientMac machinery.
  if (actualRange)
    *actualRange = range;

  const gfx::Range requested_range(range);
  if (requested_range.is_reversed())
    return nil;

  gfx::Range expected_range;
  const base::string16* expected_text;

  if (!renderWidgetHostView_->composition_range().is_empty()) {
    expected_text = &markedText_;
    expected_range = renderWidgetHostView_->composition_range();
  } else {
    expected_text = &renderWidgetHostView_->selection_text();
    size_t offset = renderWidgetHostView_->selection_text_offset();
    expected_range = gfx::Range(offset, offset + expected_text->size());
  }

  if (!expected_range.Contains(requested_range))
    return nil;

  // Gets the raw bytes to avoid unnecessary string copies for generating
  // NSString.
  const base::char16* bytes =
      &(*expected_text)[requested_range.start() - expected_range.start()];
  // Avoid integer overflow.
  base::CheckedNumeric<size_t> requested_len = requested_range.length();
  requested_len *= sizeof(base::char16);
  NSUInteger bytes_len = base::strict_cast<NSUInteger, size_t>(
      requested_len.ValueOrDefault(0));
  base::scoped_nsobject<NSString> ns_string(
      [[NSString alloc] initWithBytes:bytes
                               length:bytes_len
                             encoding:NSUTF16LittleEndianStringEncoding]);
  return [[[NSAttributedString alloc] initWithString:ns_string] autorelease];
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

// Each RenderWidgetHostViewCocoa has its own input context, but we return
// nil when the caret is in non-editable content or password box to avoid
// making input methods do their work.
- (NSTextInputContext *)inputContext {
  if (focusedPluginIdentifier_ != -1)
    return [[ComplexTextInputPanel sharedComplexTextInputPanel] inputContext];

  switch(renderWidgetHostView_->text_input_type_) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return nil;
    default:
      return [super inputContext];
  }
}

- (BOOL)hasMarkedText {
  // An input method calls this function to figure out whether or not an
  // application is really composing a text. If it is composing, it calls
  // the markedRange method, and maybe calls the setMarkedText method.
  // It seems an input method usually calls this function when it is about to
  // cancel an ongoing composition. If an application has a non-empty marked
  // range, it calls the setMarkedText method to delete the range.
  return hasMarkedText_;
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  hasMarkedText_ = NO;
  markedText_.clear();
  markedTextSelectedRange_ = NSMakeRange(NSNotFound, 0);
  underlines_.clear();

  // If we are handling a key down event, then ConfirmComposition() will be
  // called in keyEvent: method.
  if (!handlingKeyDown_) {
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::string16(), gfx::Range::InvalidRange(), false);
  } else {
    unmarkTextCalled_ = YES;
  }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
                              replacementRange:(NSRange)replacementRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];

  // |markedRange_| will get set on a callback from ImeSetComposition().
  markedTextSelectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);

  underlines_.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &underlines_);
  } else {
    // Use a thin black underline by default.
    underlines_.push_back(blink::WebCompositionUnderline(
        0, length, SK_ColorBLACK, false, SK_ColorTRANSPARENT));
  }

  // If we are handling a key down event, then SetComposition() will be
  // called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend can
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if (!handlingKeyDown_) {
    renderWidgetHostView_->render_widget_host_->ImeSetComposition(
        markedText_, underlines_,
        newSelRange.location, NSMaxRange(newSelRange));
  }
}

- (void)doCommandBySelector:(SEL)selector {
  // An input method calls this function to dispatch an editing command to be
  // handled by this view.
  if (selector == @selector(noop:))
    return;

  std::string command(
      [RenderWidgetHostViewMacEditCommandHelper::
          CommandNameForSelector(selector) UTF8String]);

  // If this method is called when handling a key down event, then we need to
  // handle the command in the key event handler. Otherwise we can just handle
  // it here.
  if (handlingKeyDown_) {
    hasEditCommands_ = YES;
    // We ignore commands that insert characters, because this was causing
    // strange behavior (e.g. tab always inserted a tab rather than moving to
    // the next field on the page).
    if (!base::StartsWith(command, "insert",
                          base::CompareCase::INSENSITIVE_ASCII))
      editCommands_.push_back(EditCommand(command, ""));
  } else {
    RenderWidgetHostImpl* rwh = renderWidgetHostView_->render_widget_host_;
    rwh->Send(new InputMsg_ExecuteEditCommand(rwh->GetRoutingID(),
                                              command, ""));
  }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  // An input method has characters to be inserted.
  // Same as Linux, Mac calls this method not only:
  // * when an input method finishs composing text, but also;
  // * when we type an ASCII character (without using input methods).
  // When we aren't using input methods, we should send the given character as
  // a Char event so it is dispatched to an onkeypress() event handler of
  // JavaScript.
  // On the other hand, when we are using input methods, we should send the
  // given characters as an input method event and prevent the characters from
  // being dispatched to onkeypress() event handlers.
  // Text inserting might be initiated by other source instead of keyboard
  // events, such as the Characters dialog. In this case the text should be
  // sent as an input method event as well.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
  } else {
    gfx::Range replacement_range(replacementRange);
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::SysNSStringToUTF16(im_text), replacement_range, false);
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

- (void)insertText:(id)string {
  [self insertText:string replacementRange:NSMakeRange(NSNotFound, 0)];
}

- (void)viewDidMoveToWindow {
  if ([self window]) {
    [self updateScreenProperties];
  } else {
    // If the RenderWidgetHostViewCocoa is being removed from its window, tear
    // down its browser compositor resources, if needed.
    renderWidgetHostView_->DestroySuspendedBrowserCompositorViewIfNeeded();
  }

  if (canBeKeyView_) {
    NSWindow* newWindow = [self window];
    // Pointer comparison only, since we don't know if lastWindow_ is still
    // valid.
    if (newWindow) {
      // If we move into a new window, refresh the frame information. We
      // don't need to do it if it was the same window as it used to be in,
      // since that case is covered by WasShown(). We only want to do this for
      // real browser views, not popups.
      if (newWindow != lastWindow_) {
        lastWindow_ = newWindow;
        renderWidgetHostView_->WindowFrameChanged();
      }
    }
  }

  // If we switch windows (or are removed from the view hierarchy), cancel any
  // open mouse-downs.
  if (hasOpenMouseDown_) {
    WebMouseEvent event;
    event.type = WebInputEvent::MouseUp;
    event.button = WebMouseEvent::ButtonLeft;
    renderWidgetHostView_->ForwardMouseEvent(event);

    hasOpenMouseDown_ = NO;
  }
}

- (void)undo:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Undo();
}

- (void)redo:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->Redo();
}

- (void)cut:(id)sender {
  content::RenderWidgetHostDelegate* render_widget_host_delegate =
      renderWidgetHostView_->render_widget_host_->delegate();
  if (render_widget_host_delegate)
    render_widget_host_delegate->Cut();
}

- (void)copy:(id)sender {
  content::RenderWidgetHostDelegate* render_widget_host_delegate =
      renderWidgetHostView_->render_widget_host_->delegate();
  if (render_widget_host_delegate)
    render_widget_host_delegate->Copy();
}

- (void)copyToFindPboard:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->CopyToFindPboard();
}

- (void)paste:(id)sender {
  content::RenderWidgetHostDelegate* render_widget_host_delegate =
      renderWidgetHostView_->render_widget_host_->delegate();
  if (render_widget_host_delegate)
    render_widget_host_delegate->Paste();
}

- (void)pasteAndMatchStyle:(id)sender {
  WebContents* web_contents = renderWidgetHostView_->GetWebContents();
  if (web_contents)
    web_contents->PasteAndMatchStyle();
}

- (void)selectAll:(id)sender {
  // editCommand_helper_ adds implementations for most NSResponder methods
  // dynamically. But the renderer side only sends selection results back to
  // the browser if they were triggered by a keyboard event or went through
  // one of the Select methods on RWH. Since selectAll: is called from the
  // menu handler, neither is true.
  // Explicitly call SelectAll() here to make sure the renderer returns
  // selection results.
  content::RenderWidgetHostDelegate* render_widget_host_delegate =
      renderWidgetHostView_->render_widget_host_->delegate();
  if (render_widget_host_delegate)
    render_widget_host_delegate->SelectAll();
}

- (void)startSpeaking:(id)sender {
  GetRenderWidgetHostViewToUse(renderWidgetHostView_.get())->SpeakSelection();
}

- (void)stopSpeaking:(id)sender {
  GetRenderWidgetHostViewToUse(renderWidgetHostView_.get())->StopSpeaking();
}

- (void)cancelComposition {
  if (!hasMarkedText_)
    return;

  // Cancel the ongoing composition. [NSInputManager markedTextAbandoned:]
  // doesn't call any NSTextInput functions, such as setMarkedText or
  // insertText. So, we need to send an IPC message to a renderer so it can
  // delete the composition node.
  // TODO(erikchen): NSInputManager is deprecated since OSX 10.6. Switch to
  // NSTextInputContext. http://www.crbug.com/479010.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSInputManager *currentInputManager = [NSInputManager currentInputManager];
  [currentInputManager markedTextAbandoned:self];
#pragma clang diagnostic pop

  hasMarkedText_ = NO;
  // Should not call [self unmarkText] here, because it'll send unnecessary
  // cancel composition IPC message to the renderer.
}

- (void)confirmComposition {
  if (!hasMarkedText_)
    return;

  if (renderWidgetHostView_->render_widget_host_)
    renderWidgetHostView_->render_widget_host_->ImeConfirmComposition(
        base::string16(), gfx::Range::InvalidRange(), false);

  [self cancelComposition];
}

- (void)setPluginImeActive:(BOOL)active {
  if (active == pluginImeActive_)
    return;

  pluginImeActive_ = active;
  if (!active) {
    [[ComplexTextInputPanel sharedComplexTextInputPanel] cancelComposition];
    renderWidgetHostView_->PluginImeCompositionCompleted(
        base::string16(), focusedPluginIdentifier_);
  }
}

- (void)pluginFocusChanged:(BOOL)focused forPlugin:(int)pluginId {
  if (focused)
    focusedPluginIdentifier_ = pluginId;
  else if (focusedPluginIdentifier_ == pluginId)
    focusedPluginIdentifier_ = -1;

  // Whenever plugin focus changes, plugin IME resets.
  [self setPluginImeActive:NO];
}

- (BOOL)postProcessEventForPluginIme:(NSEvent*)event {
  if (!pluginImeActive_)
    return false;

  ComplexTextInputPanel* inputPanel =
      [ComplexTextInputPanel sharedComplexTextInputPanel];
  NSString* composited_string = nil;
  BOOL handled = [inputPanel interpretKeyEvent:event
                                        string:&composited_string];
  if (composited_string) {
    renderWidgetHostView_->PluginImeCompositionCompleted(
        base::SysNSStringToUTF16(composited_string), focusedPluginIdentifier_);
    pluginImeActive_ = NO;
  }
  return handled;
}

- (void)checkForPluginImeCancellation {
  if (pluginImeActive_ &&
      ![[ComplexTextInputPanel sharedComplexTextInputPanel] inComposition]) {
    renderWidgetHostView_->PluginImeCompositionCompleted(
        base::string16(), focusedPluginIdentifier_);
    pluginImeActive_ = NO;
  }
}

// Overriding a NSResponder method to support application services.

- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  id requestor = nil;
  BOOL sendTypeIsString = [sendType isEqual:NSStringPboardType];
  BOOL returnTypeIsString = [returnType isEqual:NSStringPboardType];
  BOOL hasText = !renderWidgetHostView_->selected_text().empty();
  BOOL takesText =
      renderWidgetHostView_->text_input_type_ != ui::TEXT_INPUT_TYPE_NONE;

  if (sendTypeIsString && hasText && !returnType) {
    requestor = self;
  } else if (!sendType && returnTypeIsString && takesText) {
    requestor = self;
  } else if (sendTypeIsString && returnTypeIsString && hasText && takesText) {
    requestor = self;
  } else {
    requestor = [super validRequestorForSendType:sendType
                                      returnType:returnType];
  }
  return requestor;
}

- (void)viewWillStartLiveResize {
  [super viewWillStartLiveResize];
  RenderWidgetHostImpl* widget = renderWidgetHostView_->render_widget_host_;
  if (widget)
    widget->Send(new ViewMsg_SetInLiveResize(widget->GetRoutingID(), true));
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];
  RenderWidgetHostImpl* widget = renderWidgetHostView_->render_widget_host_;
  if (widget)
    widget->Send(new ViewMsg_SetInLiveResize(widget->GetRoutingID(), false));
}

- (void)updateCursor:(NSCursor*)cursor {
  if (currentCursor_ == cursor)
    return;

  currentCursor_.reset([cursor retain]);
  [[self window] invalidateCursorRectsForView:self];
}

- (void)popupWindowWillClose:(NSNotification *)notification {
  renderWidgetHostView_->KillSelf();
}

@end

//
// Supporting application services
//
@implementation RenderWidgetHostViewCocoa(NSServicesRequests)

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard
                             types:(NSArray*)types {
  const std::string& str = renderWidgetHostView_->selected_text();
  if (![types containsObject:NSStringPboardType] || str.empty()) return NO;

  base::scoped_nsobject<NSString> text(
      [[NSString alloc] initWithUTF8String:str.c_str()]);
  NSArray* toDeclare = [NSArray arrayWithObject:NSStringPboardType];
  [pboard declareTypes:toDeclare owner:nil];
  return [pboard setString:text forType:NSStringPboardType];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSString *string = [pboard stringForType:NSStringPboardType];
  if (!string) return NO;

  // If the user is currently using an IME, confirm the IME input,
  // and then insert the text from the service, the same as TextEdit and Safari.
  [self confirmComposition];
  [self insertText:string];
  return YES;
}

- (BOOL)isOpaque {
  return content::g_support_transparency ? [super isOpaque] : opaque_;
}

// "-webkit-app-region: drag | no-drag" is implemented on Mac by excluding
// regions that are not draggable. (See ControlRegionView in
// native_app_window_cocoa.mm). This requires the render host view to be
// draggable by default.
- (BOOL)mouseDownCanMoveWindow {
  return YES;
}

@end
