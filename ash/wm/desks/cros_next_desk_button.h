// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_H_
#define ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/cros_next_desk_button_base.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

class DesksBarView;

// A button in zero state bar showing the name of the desk. Zero state is the
// state of the desks bar when there's only a single desk available, in which
// case the bar is shown in a minimized state. Clicking the button will switch
// to the expanded desks bar and focus on the single desk's name view.
// TODO(conniekxu): Remove `ZeroStateDefaultDeskButton`, replace it with this
// class, and rename this class by removing the prefix CrOSNext.
class CrOSNextDefaultDeskButton : public CrOSNextDeskButtonBase {
 public:
  METADATA_HEADER(CrOSNextDefaultDeskButton);

  explicit CrOSNextDefaultDeskButton(DesksBarView* bar_view);
  CrOSNextDefaultDeskButton(const CrOSNextDefaultDeskButton&) = delete;
  CrOSNextDefaultDeskButton& operator=(const CrOSNextDefaultDeskButton&) =
      delete;
  ~CrOSNextDefaultDeskButton() override = default;

  void UpdateLabelText();

  // CrOSNextDeskButtonBase:
  gfx::Size CalculatePreferredSize() const override;

 private:
  void OnButtonPressed();

  DesksBarView* const bar_view_;
};

// A button view in the desks bar with an icon. The button have three different
// states, and the three states are interchangeable.
// TODO(conniekxu): Remove `ZeroStateIconButton` and `ExpandedDesksBarButton`,
// replace them with this class, and rename this class by removing the prefix
// CrOSNext.
class ASH_EXPORT CrOSNextDeskIconButton : public CrOSNextDeskButtonBase {
 public:
  METADATA_HEADER(CrOSNextDeskIconButton);

  // The enum class defines three states for the button. The button at different
  // states has different sizes. Any state could be transformed into another
  // state under certain conditions.
  enum class State {
    // The state of the button when the DesksBarView is in zero state.
    kZero,
    // The state of the button when the DesksBarView is in expanded state.
    kExpanded,
    // The state of when a window is dragged over the new desk button and held
    // for 500 milliseconds, we can create a new desk. The new desk button state
    // will change to reflect that.
    kDragAndDrop,
  };

  CrOSNextDeskIconButton(DesksBarView* bar_view,
                         const gfx::VectorIcon* button_icon,
                         const std::u16string& text,
                         ui::ColorId icon_color_id,
                         ui::ColorId background_color_id,
                         bool initially_enabled,
                         base::RepeatingClosure callback);
  CrOSNextDeskIconButton(const CrOSNextDeskIconButton&) = delete;
  CrOSNextDeskIconButton& operator=(const CrOSNextDeskIconButton&) = delete;
  ~CrOSNextDeskIconButton() override;

  // Convenient function for returning the desk icon button's corner radius on
  // the given `state`.
  static int GetCornerRadiusOnState(State state);

  State state() const { return state_; }
  void set_paint_as_active(bool paint_as_active) {
    paint_as_active_ = paint_as_active;
  }

  // Called when the button's state (kZero, kExpanded, kDragAndDrop) gets
  // updated. It updates `state_` to store the most updated state and focus ring
  // radius based on `state_`.
  void UpdateState(State state);

  bool IsPointOnButton(const gfx::Point& screen_location) const;

  // CrOSNextDeskButtonBase:
  gfx::Size CalculatePreferredSize() const override;
  // Updates the focus ring based on the dragged item's position and
  // `paint_as_active_`.
  void UpdateFocusState() override;
  void OnThemeChanged() override;
  void StateChanged(ButtonState old_state) override;

 private:
  // Triggered when the button's enable state gets changed, i.e, the button is
  // updated to disabled from enabled, or enabled from disabled. The button's
  // icon and background color will be updated correspondingly to reflect the
  // enable state change. Also this functions will be called after the button's
  // initialization to show the button's correct enable state.
  void UpdateEnabledState();

  DesksBarView* const bar_view_;

  State state_;

  // If `paint_as_active_` is true, then focus ring will be painted with color
  // id `kColorAshCurrentDeskColor` even if it's not already focused.
  bool paint_as_active_ = false;

  const gfx::VectorIcon* const button_icon_;
  const ui::ColorId icon_color_id_;
  const ui::ColorId background_color_id_;

  absl::optional<ui::ColorId> focus_color_id_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_CROS_NEXT_DESK_BUTTON_H_