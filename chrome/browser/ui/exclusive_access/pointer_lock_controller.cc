// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;
using content::WebContents;


PointerLockController::PointerLockController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager),
      pointer_lock_state_(POINTERLOCK_UNLOCKED),
      fake_pointer_lock_for_test_(false) {}

PointerLockController::~PointerLockController() = default;

bool PointerLockController::IsPointerLocked() const {
  return pointer_lock_state_ == POINTERLOCK_LOCKED ||
         pointer_lock_state_ == POINTERLOCK_LOCKED_SILENTLY;
}

bool PointerLockController::IsPointerLockedSilently() const {
  return pointer_lock_state_ == POINTERLOCK_LOCKED_SILENTLY;
}

void PointerLockController::RequestToLockPointer(WebContents* web_contents,
                                                 bool user_gesture,
                                                 bool last_unlocked_by_target) {
  DCHECK(!IsPointerLocked());

  // To prevent misbehaving sites from constantly re-locking the pointer, the
  // lock-requesting frame must have transient user activation. The only
  // exception is when the target frame previously unlocked itself (i.e. not
  // the user).
  if (!last_unlocked_by_target) {
    if (!user_gesture) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
      if (lock_state_callback_for_test_) {
        std::move(lock_state_callback_for_test_).Run();
      }
      return;
    }
  }

  LockPointer(web_contents->GetWeakPtr(), last_unlocked_by_target);
}

void PointerLockController::ExitExclusiveAccessIfNecessary() {
  NotifyTabExclusiveAccessLost();
}

void PointerLockController::NotifyTabExclusiveAccessLost() {
  WebContents* tab = exclusive_access_tab();
  if (tab) {
    UnlockPointer();
    SetTabWithExclusiveAccess(nullptr);
    pointer_lock_state_ = POINTERLOCK_UNLOCKED;
    exclusive_access_manager()->UpdateBubble(base::NullCallback());
  }
}

bool PointerLockController::HandleUserPressedEscape() {
  // Don't break the pointer lock here.  Let the ESC key event reach the
  // renderer so the page has a chance to call preventDefault().  If the
  // page does not consume the event, the lock will be broken in the
  // post-ACK HandleKeyboardEvent path.
  return false;
}

void PointerLockController::HandleUserHeldEscape() {
  if (IsPointerLocked()) {
    ExitExclusiveAccessIfNecessary();
  }
}

void PointerLockController::HandleUserReleasedEscapeEarly() {}

bool PointerLockController::RequiresPressAndHoldEscToExit() const {
  return false;
}

void PointerLockController::ExitExclusiveAccessToPreviousState() {
  if (lock_state_callback_for_test_) {
    std::move(lock_state_callback_for_test_).Run();
  }

  pointer_lock_state_ = POINTERLOCK_UNLOCKED;
  SetTabWithExclusiveAccess(nullptr);

  if (!ShouldSuppressBubbleReshowForStateChange()) {
    exclusive_access_manager()->UpdateBubble(base::NullCallback());
  }
}

void PointerLockController::UnlockPointer() {
  WebContents* tab = exclusive_access_tab();

  if (!tab) {
    return;
  }

  content::RenderWidgetHostView* pointer_lock_view = nullptr;
  RenderViewHost* const rvh =
      exclusive_access_tab()->GetPrimaryMainFrame()->GetRenderViewHost();
  if (rvh) {
    pointer_lock_view = rvh->GetWidget()->GetView();
  }

  if (pointer_lock_view) {
    pointer_lock_view->UnlockPointer();
  }
}

void PointerLockController::LockPointer(
    base::WeakPtr<content::WebContents> web_contents,
    bool last_unlocked_by_target) {
  if (!web_contents) {
    if (lock_state_callback_for_test_) {
      std::move(lock_state_callback_for_test_).Run();
    }
    return;
  }
  SetTabWithExclusiveAccess(web_contents.get());
  // Focus may have moved to the modal, so move it back to the WebContents.
  web_contents->Focus();

  // Lock the mouse pointer.
  if (fake_pointer_lock_for_test_ ||
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kSuccess)) {
    if (last_unlocked_by_target &&
        web_contents_granted_silent_pointer_lock_permission_ ==
            web_contents.get()) {
      pointer_lock_state_ = POINTERLOCK_LOCKED_SILENTLY;
    } else {
      pointer_lock_state_ = POINTERLOCK_LOCKED;
    }
  } else {
    SetTabWithExclusiveAccess(nullptr);
    pointer_lock_state_ = POINTERLOCK_UNLOCKED;
  }

  if (!ShouldSuppressBubbleReshowForStateChange()) {
    exclusive_access_manager()->UpdateBubble(
        base::BindOnce(&PointerLockController::OnBubbleHidden,
                       weak_ptr_factory_.GetWeakPtr(), web_contents));
  }
  if (lock_state_callback_for_test_) {
    std::move(lock_state_callback_for_test_).Run();
  }
}

void PointerLockController::OnBubbleHidden(
    base::WeakPtr<content::WebContents> web_contents,
    ExclusiveAccessBubbleHideReason reason) {
  if (bubble_hide_callback_for_test_) {
    bubble_hide_callback_for_test_.Run(reason);
  }

  // Allow silent pointer lock if the bubble has been display for a period of
  // time and dismissed due to timeout.
  if (reason == ExclusiveAccessBubbleHideReason::kTimeout) {
    web_contents_granted_silent_pointer_lock_permission_ = web_contents.get();
  } else {
    web_contents_granted_silent_pointer_lock_permission_ = nullptr;
  }
}

bool PointerLockController::ShouldSuppressBubbleReshowForStateChange() {
  ExclusiveAccessBubbleType bubble_type =
      exclusive_access_manager()->GetExclusiveAccessExitBubbleType();
  return (pointer_lock_state_ == POINTERLOCK_LOCKED &&
          bubble_type ==
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION) ||
         (pointer_lock_state_ == POINTERLOCK_UNLOCKED &&
          bubble_type ==
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION);
}
