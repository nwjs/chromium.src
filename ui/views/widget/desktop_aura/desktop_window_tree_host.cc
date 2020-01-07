// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#endif

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"

namespace views {

void DesktopWindowTreeHost::SetBoundsInDIP(const gfx::Rect& bounds) {
  aura::Window* root = AsWindowTreeHost()->window();
  const gfx::Rect bounds_in_pixels =
      display::Screen::GetScreen()->DIPToScreenRectInWindow(root, bounds);
  AsWindowTreeHost()->SetBoundsInPixels(bounds_in_pixels);
}

#if defined(OS_WIN)
void DesktopWindowTreeHost::SetPositionInDIP(const gfx::Point& pos) {
  const gfx::Point pos_in_pixels =
    display::win::ScreenWin::DIPToScreenPoint(pos);
  AsWindowTreeHost()->SetPositionInPixels(pos_in_pixels);
}
#endif

std::unique_ptr<aura::client::ScreenPositionClient>
DesktopWindowTreeHost::CreateScreenPositionClient() {
  return std::make_unique<DesktopScreenPositionClient>(
      AsWindowTreeHost()->window());
}

}  // namespace views
