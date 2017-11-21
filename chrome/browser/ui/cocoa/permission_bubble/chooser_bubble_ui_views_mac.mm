// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permission_bubble/chooser_bubble_ui.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/bubble_anchor_helper_views.h"
#import "chrome/browser/ui/cocoa/permission_bubble/chooser_bubble_ui_cocoa.h"
#include "chrome/browser/ui/permission_bubble/chooser_bubble_delegate.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

std::unique_ptr<BubbleUi> ChooserBubbleDelegate::BuildBubbleUi() {
  if (!ui::MaterialDesignController::IsSecondaryUiMaterial()) {
    return base::MakeUnique<ChooserBubbleUiCocoa>(
        browser_, app_window_, std::move(chooser_controller_));
  }
  return base::MakeUnique<ChooserBubbleUi>(browser_, app_window_,
                                           std::move(chooser_controller_));
}

void ChooserBubbleUi::CreateAndShow(views::BubbleDialogDelegateView* delegate) {
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
