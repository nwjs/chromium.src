// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"

#include "extensions/components/native_app_window/native_app_window_views.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "ui/views/controls/image_view.h"

// The Views browser implementation of ChooserBubbleUi's anchor methods.
// Views browsers have a native View to anchor the bubble to, which these
// functions provide.

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  return base::MakeUnique<ChooserBubbleUi>(browser_, app_window_,
                                           std::move(chooser_controller_));
}

void ChooserBubbleUi::CreateAndShow(views::BubbleDialogDelegateView* delegate) {
  // Set |parent_window_| because some valid anchors can become hidden.
  views::Widget* widget = nullptr;
  if (browser_) {
    widget = views::Widget::GetWidgetForNativeWindow(
      browser_->window()->GetNativeWindow());
  gfx::NativeView parent = widget->GetNativeView();
  DCHECK(parent);
  delegate->set_parent_window(parent);
  }
  views::BubbleDialogDelegateView::CreateBubble(delegate)->Show();
}

views::View* ChooserBubbleUi::GetAnchorView() {
  if (browser_) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  if (browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR)) {
    return browser_view->GetLocationBarView()
        ->location_icon_view()
        ->GetImageView();
  }
  if (browser_view->IsFullscreenBubbleVisible())
    return browser_view->exclusive_access_bubble()->GetView();

  return browser_view->top_container();
  } else if (app_window_) {
    native_app_window::NativeAppWindowViews* native_app_window_views =
      static_cast<native_app_window::NativeAppWindowViews*>(app_window_->GetBaseWindow());
    return native_app_window_views->web_view();
  }
  return nullptr;
}

gfx::Point ChooserBubbleUi::GetAnchorPoint() {
  return gfx::Point();
}

views::BubbleBorder::Arrow ChooserBubbleUi::GetAnchorArrow() {
  if (browser_ && browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return views::BubbleBorder::TOP_LEFT;
  return views::BubbleBorder::NONE;
}
