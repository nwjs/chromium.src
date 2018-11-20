// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"

#include <memory>

#include "build/buildflag.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_dialogs_views_mac.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#import "chrome/browser/ui/cocoa/permission_bubble/chooser_bubble_ui_cocoa.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"


void ChooserBubbleUi::CreateAndShowCocoa(
    views::BubbleDialogDelegateView* delegate) {
  // Set |parent_window_| because some valid anchors can become hidden.
  gfx::NativeView parent = nullptr;
  if (browser_)
      parent = platform_util::GetViewForWindow(browser_->window()->GetNativeWindow());
  //DCHECK(parent);
  delegate->set_parent_window(parent);
  views::BubbleDialogDelegateView::CreateBubble(delegate)->Show();
  if (browser_)
  KeepBubbleAnchored(delegate, GetPageInfoDecoration(browser_->window()->GetNativeWindow()));
}

#if !BUILDFLAG(MAC_VIEWS_BROWSER)
void ChooserBubbleUi::CreateAndShow(views::BubbleDialogDelegateView* delegate) {
  CreateAndShowCocoa(delegate);
}

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  if (!chrome::ShowAllDialogsWithViewsToolkit()) {
    return std::make_unique<ChooserBubbleUiCocoa>(
        browser_, std::move(chooser_controller_));
  }
  return std::make_unique<ChooserBubbleUi>(browser_,
                                           std::move(chooser_controller_));
}
#endif
