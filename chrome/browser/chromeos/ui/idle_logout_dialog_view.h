// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "ui/views/window/dialog_delegate.h"

namespace base {
class TimeDelta;
}
namespace views {
class Label;
}

// A class to show the logout on idle dialog if the machine is in retail mode.
class IdleLogoutDialogView : public views::DialogDelegateView {
 public:
  static void ShowDialog();
  static void CloseDialog();

  // views::DialogDelegateView:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

 private:
  IdleLogoutDialogView();
  virtual ~IdleLogoutDialogView();

  // Adds the labels and adds them to the layout.
  void InitAndShow();

  void Show();
  void Close();

  void UpdateCountdownTimer();

  views::Label* restart_label_;
  views::Label* warning_label_;

  // Time at which the countdown is over and we should close the dialog.
  base::Time countdown_end_time_;

  base::RepeatingTimer<IdleLogoutDialogView> timer_;

  base::WeakPtrFactory<IdleLogoutDialogView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdleLogoutDialogView);
};

#endif  // CHROME_BROWSER_CHROMEOS_UI_IDLE_LOGOUT_DIALOG_VIEW_H_
