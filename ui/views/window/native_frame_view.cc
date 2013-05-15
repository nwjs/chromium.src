// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/native_frame_view.h"

#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, public:

NativeFrameView::NativeFrameView(Widget* frame)
    : NonClientFrameView(),
      frame_(frame) {
}

NativeFrameView::~NativeFrameView() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeFrameView, NonClientFrameView overrides:

gfx::Rect NativeFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Rect NativeFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
#if defined(OS_WIN)
  BOOL has_menu = frame_->has_menu_bar() ? TRUE : FALSE;
  return views::GetWindowBoundsForClientBounds(
                                               static_cast<View*>(const_cast<NativeFrameView*>(this)), client_bounds, has_menu);
#else
  // TODO(sad):
  return client_bounds;
#endif
}

int NativeFrameView::NonClientHitTest(const gfx::Point& point) {
  int component = frame_->client_view()->NonClientHitTest(point);

  // If the test is non-client then we decide whether can resize.
  if (component == HTNOWHERE&&
      frame_->widget_delegate() &&
      !frame_->widget_delegate()->CanResize()) {
    // Get what's the component under the mouse.
    POINT temp = point.ToPOINT();
    MapWindowPoints(GetWidget()->GetNativeView(), HWND_DESKTOP, &temp, 1);
    int component = DefWindowProc(GetWidget()->GetNativeView(), WM_NCHITTEST,
                                  0, MAKELPARAM(temp.x, temp.y));

    // Return border if the component is resize handle.
    if (component >= HTLEFT && component <= HTBOTTOMRIGHT)
      return HTBORDER;
  }

  return component;
}

void NativeFrameView::GetWindowMask(const gfx::Size& size,
                                    gfx::Path* window_mask) {
  // Nothing to do, we use the default window mask.
}

void NativeFrameView::ResetWindowControls() {
  // Nothing to do.
}

void NativeFrameView::UpdateWindowIcon() {
  // Nothing to do.
}

void NativeFrameView::UpdateWindowTitle() {
  // Nothing to do.
}

// Returns the client size. On Windows, this is the expected behavior for
// native frames (see |NativeWidgetWin::WidgetSizeIsClientSize()|), while other
// platforms currently always return client bounds from
// |GetWindowBoundsForClientBounds()|.
gfx::Size NativeFrameView::GetPreferredSize() {
  return frame_->client_view()->GetPreferredSize();
}

gfx::Size NativeFrameView::GetMinimumSize() {
  return frame_->client_view()->GetMinimumSize();
}

gfx::Size NativeFrameView::GetMaximumSize() {
  return frame_->client_view()->GetMaximumSize();
}

}  // namespace views
