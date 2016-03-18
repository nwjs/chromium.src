// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_icons/status_icon_linux_wrapper.h"

#include "ui/message_center/notifier_settings.h"
#include "ui/views/linux_ui/linux_ui.h"

StatusIconLinuxWrapper::StatusIconLinuxWrapper(
    views::StatusIconLinux* status_icon) {
  status_icon_.reset(status_icon);
  status_icon_->set_delegate(this);
}

StatusIconLinuxWrapper::~StatusIconLinuxWrapper() {
}

void StatusIconLinuxWrapper::SetImage(const gfx::ImageSkia& image) {
  status_icon_->SetImage(image);
}

void StatusIconLinuxWrapper::SetToolTip(const base::string16& tool_tip) {
  status_icon_->SetToolTip(tool_tip);
}

void StatusIconLinuxWrapper::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const base::string16& title,
    const base::string16& contents,
    const message_center::NotifierId& notifier_id) {
  notification_.DisplayBalloon(icon, title, contents, notifier_id);
}

void StatusIconLinuxWrapper::OnClick() {
  DispatchClickEvent();
}

bool StatusIconLinuxWrapper::HasClickAction() {
  return HasObservers();
}

void StatusIconLinuxWrapper::OnMenuStateChanged() {
  status_icon_->RefreshPlatformContextMenu();
}

StatusIconLinuxWrapper* StatusIconLinuxWrapper::CreateWrappedStatusIcon(
    const gfx::ImageSkia& image,
    const base::string16& tool_tip) {
  const views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui) {
    scoped_ptr<views::StatusIconLinux> status_icon =
        linux_ui->CreateLinuxStatusIcon(image, tool_tip);
    if (status_icon.get())
      return new StatusIconLinuxWrapper(status_icon.release());
  }
  return NULL;
}

void StatusIconLinuxWrapper::UpdatePlatformContextMenu(
    ui::MenuModel* model) {
  status_icon_->UpdatePlatformContextMenu(model);
}
