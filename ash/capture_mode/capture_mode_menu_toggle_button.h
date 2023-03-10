// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_TOGGLE_BUTTON_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_TOGGLE_BUTTON_H_

#include "ash/capture_mode/capture_mode_session_focus_cycler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// A view section in the capture mode settings menu that consists of a menu item
// with a toggle button.
class CaptureModeMenuToggleButton
    : public views::View,
      public CaptureModeSessionFocusCycler::HighlightableView {
 public:
  METADATA_HEADER(CaptureModeMenuToggleButton);

  CaptureModeMenuToggleButton(const gfx::VectorIcon& icon,
                              const std::u16string& label_text,
                              bool enabled,
                              views::ToggleButton::PressedCallback callback);
  CaptureModeMenuToggleButton(const CaptureModeMenuToggleButton&) = delete;
  CaptureModeMenuToggleButton& operator=(const CaptureModeMenuToggleButton&) =
      delete;
  ~CaptureModeMenuToggleButton() override;

  // views::View
  void OnThemeChanged() override;

  // CaptureModeSessionFocusCycler::HighlightableView:
  views::View* GetView() override;

  views::ToggleButton* toggle_button_for_testing() { return toggle_button_; }

 private:
  views::ImageView* icon_view_;
  views::Label* label_view_;

  // Toggles between enabling and disabling the capture mode demo tools feature.
  views::ToggleButton* toggle_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_CAMERA_PREVIEW_VIEW_H_
