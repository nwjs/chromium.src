// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/tablet_state.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// The default delay between the user pressing the size button and the buttons
// adjacent to the size button morphing into buttons for snapping left and
// right.
const int kSetButtonsToSnapModeDelayMs = 150;

// The amount that a user can overshoot one of the caption buttons while in
// "snap mode" and keep the button hovered/pressed.
const int kMaxOvershootX = 200;
const int kMaxOvershootY = 50;

constexpr base::TimeDelta kPieAnimationPressDuration = base::Milliseconds(150);
constexpr base::TimeDelta kPieAnimationHoverDuration = base::Milliseconds(500);

// Returns true if a mouse drag while in "snap mode" at |location_in_screen|
// would hover/press |button| or keep it hovered/pressed.
bool HitTestButton(const views::FrameCaptionButton* button,
                   const gfx::Point& location_in_screen) {
  gfx::Rect expanded_bounds_in_screen = button->GetBoundsInScreen();
  if (button->GetState() == views::Button::STATE_HOVERED ||
      button->GetState() == views::Button::STATE_PRESSED) {
    expanded_bounds_in_screen.Inset(
        gfx::Insets::VH(-kMaxOvershootY, -kMaxOvershootX));
  }
  return expanded_bounds_in_screen.Contains(location_in_screen);
}

SnapDirection GetSnapDirection(const views::FrameCaptionButton* to_hover) {
  if (!to_hover)
    return SnapDirection::kNone;

  aura::Window* window = to_hover->GetWidget()->GetNativeWindow();
  switch (to_hover->GetIcon()) {
    case views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED:
      return GetSnapDirectionForWindow(window, /*left_top=*/true);
    case views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED:
      return GetSnapDirectionForWindow(window, /*left_top=*/false);
    case views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
    case views::CAPTION_BUTTON_ICON_MINIMIZE:
    case views::CAPTION_BUTTON_ICON_CLOSE:
    case views::CAPTION_BUTTON_ICON_BACK:
    case views::CAPTION_BUTTON_ICON_LOCATION:
    case views::CAPTION_BUTTON_ICON_MENU:
    case views::CAPTION_BUTTON_ICON_ZOOM:
    case views::CAPTION_BUTTON_ICON_CENTER:
    case views::CAPTION_BUTTON_ICON_CUSTOM:
    case views::CAPTION_BUTTON_ICON_COUNT:
      NOTREACHED();
      return SnapDirection::kNone;
  }
}

}  // namespace

// This view controls animating a pie on a parent button which indicates when
// long press or long hover will end.
class FrameSizeButton::PieAnimationView : public views::View,
                                          public views::AnimationDelegateViews {
 public:
  explicit PieAnimationView(FrameSizeButton* button)
      : views::AnimationDelegateViews(this), button_(button) {
    SetCanProcessEventsWithinSubtree(false);
    animation_.SetTweenType(gfx::Tween::LINEAR);
  }
  PieAnimationView(const PieAnimationView&) = delete;
  PieAnimationView& operator=(const PieAnimationView&) = delete;
  ~PieAnimationView() override = default;

  void Start(base::TimeDelta duration, MultitaskMenuEntryType entry_type) {
    entry_type_ = entry_type;

    animation_.Reset(0.0);
    // `SlideAnimation` is unaffected by debug tools such as
    // "--ui-slow-animations" flag, so manually multiply the duration here.
    animation_.SetSlideDuration(
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() * duration);
    animation_.Show();
  }

  void Stop() {
    animation_.Reset(0.0);
    SchedulePaint();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    const double animation_value = animation_.GetCurrentValue();
    if (animation_value == 0.0) {
      return;
    }

    // The pie is a filled arc which starts at the top and sweeps around
    // clockwise.
    const SkScalar start_angle = -90.f;
    const SkScalar sweep_angle = 360.f * animation_value;

    SkPath path;
    const gfx::Rect bounds = GetLocalBounds();
    path.moveTo(bounds.CenterPoint().x(), bounds.CenterPoint().y());
    path.arcTo(gfx::RectToSkRect(bounds), start_angle, sweep_angle,
               /*forceMoveTo=*/false);
    path.close();

    cc::PaintFlags flags;
    flags.setColor(
        GetWidget()->GetColorProvider()->GetColor(ui::kColorSysStateHover));
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(path, flags);
  }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    SchedulePaint();
    button_->ShowMultitaskMenu(entry_type_);
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    SchedulePaint();
  }

 private:
  gfx::SlideAnimation animation_{this};

  // Tracks the entry type that triggered the latests pie animation. Used for
  // recording metrics once the menu is shown.
  MultitaskMenuEntryType entry_type_ =
      MultitaskMenuEntryType::kFrameSizeButtonHover;

  // The button `this` is associated with. Unowned.
  raw_ptr<FrameSizeButton> button_;
};

// The class to observe the to-be-snapped window during the waiting-for-snap
// mode. If the window's window state is changed or the window is put in
// overview during the waiting mode, cancel the snap.
class FrameSizeButton::SnappingWindowObserver : public aura::WindowObserver {
 public:
  SnappingWindowObserver(aura::Window* window, FrameSizeButton* size_button)
      : window_(window), size_button_(size_button) {
    window_->AddObserver(this);
  }

  SnappingWindowObserver(const SnappingWindowObserver&) = delete;
  SnappingWindowObserver& operator=(const SnappingWindowObserver&) = delete;

  ~SnappingWindowObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window_, window);
    if ((key == chromeos::kIsShowingInOverviewKey &&
         window_->GetProperty(chromeos::kIsShowingInOverviewKey)) ||
        key == chromeos::kWindowStateTypeKey) {
      // If the window is put in overview while we're in waiting-for-snapping
      // mode, or the window's window state has changed, cancel the snap.
      size_button_->CancelSnap();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
    size_button_->CancelSnap();
  }

 private:
  raw_ptr<aura::Window> window_;
  raw_ptr<FrameSizeButton> size_button_;
};

FrameSizeButton::FrameSizeButton(PressedCallback callback,
                                 FrameSizeButtonDelegate* delegate)
    : views::FrameCaptionButton(std::move(callback),
                                views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
                                HTMAXBUTTON),
      delegate_(delegate),
      set_buttons_to_snap_mode_delay_ms_(kSetButtonsToSnapModeDelayMs) {
  display_observer_.emplace(this);

  if (chromeos::wm::features::IsFloatWindowEnabled()) {
    pie_animation_view_ =
        AddChildView(std::make_unique<PieAnimationView>(this));
  }
}

FrameSizeButton::~FrameSizeButton() = default;

bool FrameSizeButton::IsMultitaskMenuShown() const {
  return multitask_menu_ && multitask_menu_->IsBubbleShown();
}

void FrameSizeButton::ShowMultitaskMenu(MultitaskMenuEntryType entry_type) {
  // Show Multitask Menu if float is enabled. Note here float flag is also used
  // to represent other relatable UI/UX changes.
  if (chromeos::wm::features::IsFloatWindowEnabled()) {
    DCHECK(!chromeos::TabletState::Get()->InTabletMode());
    RecordMultitaskMenuEntryType(entry_type);
    // Owned by the bubble which contains this view. If there is an existing
    // bubble, it will be deactivated and then close and destroy itself.
    multitask_menu_ = new MultitaskMenu(
        /*anchor=*/this, GetWidget(),
        base::BindOnce(&FrameSizeButton::OnMultitaskMenuClosed,
                       weak_factory_.GetWeakPtr()));
    multitask_menu_->ShowBubble();
  }
}

void FrameSizeButton::ToggleMultitaskMenu() {
  DCHECK(chromeos::wm::features::IsFloatWindowEnabled());
  DCHECK(!chromeos::TabletState::Get()->InTabletMode());
  if (!multitask_menu_) {
    RecordMultitaskMenuEntryType(MultitaskMenuEntryType::kAccel);
    multitask_menu_ = new MultitaskMenu(
        /*anchor=*/this, GetWidget(),
        base::BindOnce(&FrameSizeButton::OnMultitaskMenuClosed,
                       weak_factory_.GetWeakPtr()));
  }
  multitask_menu_->ToggleBubble();
}

void FrameSizeButton::OnMultitaskMenuClosed() {
  multitask_menu_ = nullptr;
}

bool FrameSizeButton::OnMousePressed(const ui::MouseEvent& event) {
  // Note that this triggers `StateChanged()`, and we want the changes to
  // `pie_animation_view_` below to come after `StateChanged()`.
  views::FrameCaptionButton::OnMousePressed(event);

  if (IsTriggerableEvent(event)) {
    // Add a visual indicator of when snap mode will get triggered.
    StartPieAnimation(kPieAnimationPressDuration,
                      MultitaskMenuEntryType::kFrameSizeButtonLongPress);

    // The minimize and close buttons are set to snap left and right when
    // snapping is enabled. Do not enable snapping if the minimize button is not
    // visible. The close button is always visible.
    if (!in_snap_mode_ && delegate_->CanSnap() &&
        delegate_->IsMinimizeButtonVisible()) {
      StartSetButtonsToSnapModeTimer(event);
    }
  }

  return true;
}

bool FrameSizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateSnapPreview(event);
  // By default a FrameCaptionButton reverts to STATE_NORMAL once the mouse
  // leaves its bounds. Skip FrameCaptionButton's handling when
  // |in_snap_mode_| == true because we want different behavior.
  if (!in_snap_mode_)
    views::FrameCaptionButton::OnMouseDragged(event);
  return true;
}

void FrameSizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event))
    CommitSnap(event);

  views::FrameCaptionButton::OnMouseReleased(event);
}

void FrameSizeButton::OnMouseCaptureLost() {
  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
  views::FrameCaptionButton::OnMouseCaptureLost();
}

void FrameSizeButton::OnMouseMoved(const ui::MouseEvent& event) {
  // Ignore any synthetic mouse moves during a drag.
  if (!in_snap_mode_)
    views::FrameCaptionButton::OnMouseMoved(event);
}

void FrameSizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().touch_points() > 1) {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
    return;
  }
  if (event->type() == ui::ET_GESTURE_TAP_DOWN && delegate_->CanSnap()) {
    StartSetButtonsToSnapModeTimer(*event);

    // Go through FrameCaptionButton's handling so that the button gets pressed.
    views::FrameCaptionButton::OnGestureEvent(event);

    // Add a visual indicator of when snap mode will get triggered. Note that
    // order matters as the subclasses will call `StateChanged()` and we want
    // the changes there to run first.
    StartPieAnimation(kPieAnimationPressDuration,
                      MultitaskMenuEntryType::kFrameSizeButtonLongTouch);
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateSnapPreview(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_GESTURE_END) {
    if (CommitSnap(*event)) {
      event->SetHandled();
      return;
    }
  }

  views::FrameCaptionButton::OnGestureEvent(event);
}

void FrameSizeButton::StateChanged(views::Button::ButtonState old_state) {
  views::FrameCaptionButton::StateChanged(old_state);

  if (!chromeos::wm::features::IsFloatWindowEnabled())
    return;

  // Pie animation will start on both active/inactive window.
  if (GetState() == views::Button::STATE_HOVERED) {
    // On animation end we should show the multitask menu.
    // Note that if the window is not active, after the pie animation this will
    // activate the window.
    StartPieAnimation(kPieAnimationHoverDuration,
                      MultitaskMenuEntryType::kFrameSizeButtonHover);
  } else if (old_state == views::Button::STATE_HOVERED) {
    DCHECK(pie_animation_view_);
    pie_animation_view_->Stop();
  }
}

void FrameSizeButton::Layout() {
  if (pie_animation_view_) {
    // Use the bounds of the inkdrop.
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(GetInkdropInsets(bounds.size()));
    pie_animation_view_->SetBoundsRect(bounds);
  }

  views::FrameCaptionButton::Layout();
}

void FrameSizeButton::OnDisplayTabletStateChanged(display::TabletState state) {
  if (state == display::TabletState::kEnteringTabletMode) {
    if (pie_animation_view_) {
      pie_animation_view_->Stop();
    }
    set_buttons_to_snap_mode_timer_.Stop();
  }
}

void FrameSizeButton::StartSetButtonsToSnapModeTimer(
    const ui::LocatedEvent& event) {
  set_buttons_to_snap_mode_timer_event_location_ = event.location();
  if (set_buttons_to_snap_mode_delay_ms_ == 0) {
    AnimateButtonsToSnapMode();
  } else {
    set_buttons_to_snap_mode_timer_.Start(
        FROM_HERE, base::Milliseconds(set_buttons_to_snap_mode_delay_ms_), this,
        &FrameSizeButton::AnimateButtonsToSnapMode);
  }
}

void FrameSizeButton::StartPieAnimation(base::TimeDelta duration,
                                        MultitaskMenuEntryType entry_type) {
  if (!chromeos::wm::features::IsFloatWindowEnabled() ||
      chromeos::TabletState::Get()->InTabletMode()) {
    return;
  }

  DCHECK(pie_animation_view_);
  pie_animation_view_->Start(duration, entry_type);
}

void FrameSizeButton::AnimateButtonsToSnapMode() {
  SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate::kYes);

  // Start observing the to-be-snapped window.
  snapping_window_observer_ = std::make_unique<SnappingWindowObserver>(
      GetWidget()->GetNativeWindow(), this);
}

void FrameSizeButton::SetButtonsToSnapMode(
    FrameSizeButtonDelegate::Animate animate) {
  DCHECK(!chromeos::TabletState::Get()->InTabletMode());
  in_snap_mode_ = true;

  // When using a right-to-left layout the close button is left of the size
  // button and the minimize button is right of the size button.
  if (base::i18n::IsRTL()) {
    delegate_->SetButtonIcons(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                              views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                              animate);
  } else {
    delegate_->SetButtonIcons(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                              views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                              animate);
  }
}

void FrameSizeButton::UpdateSnapPreview(const ui::LocatedEvent& event) {
  if (!in_snap_mode_) {
    // Set the buttons adjacent to the size button to snap left and right early
    // if the user drags past the drag threshold.
    // |set_buttons_to_snap_mode_timer_| is checked to avoid entering the snap
    // mode as a result of an unsupported drag type (e.g. only the right mouse
    // button is pressed).
    gfx::Vector2d delta(event.location() -
                        set_buttons_to_snap_mode_timer_event_location_);
    if (!set_buttons_to_snap_mode_timer_.IsRunning() ||
        !views::View::ExceededDragThreshold(delta)) {
      return;
    }
    AnimateButtonsToSnapMode();
  }

  const views::FrameCaptionButton* to_hover = GetButtonToHover(event);
  SnapDirection snap = GetSnapDirection(to_hover);

  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  bool press_size_button =
      to_hover || HitTestButton(this, event_location_in_screen);

  if (to_hover) {
    // Progress the minimize and close icon morph animations to the end if they
    // are in progress.
    SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate::kNo);
  }

  delegate_->SetHoveredAndPressedButtons(to_hover,
                                         press_size_button ? this : nullptr);
  delegate_->ShowSnapPreview(snap,
                             /*allow_haptic_feedback=*/event.IsMouseEvent());
}

const views::FrameCaptionButton* FrameSizeButton::GetButtonToHover(
    const ui::LocatedEvent& event) const {
  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  const views::FrameCaptionButton* closest_button =
      delegate_->GetButtonClosestTo(event_location_in_screen);
  if ((closest_button->GetIcon() ==
           views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED ||
       closest_button->GetIcon() ==
           views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED) &&
      HitTestButton(closest_button, event_location_in_screen)) {
    return closest_button;
  }
  return nullptr;
}

bool FrameSizeButton::CommitSnap(const ui::LocatedEvent& event) {
  snapping_window_observer_.reset();
  SnapDirection snap = GetSnapDirection(GetButtonToHover(event));
  delegate_->CommitSnap(snap);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);

  if (snap == SnapDirection::kPrimary) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxLeft"));
  } else if (snap == SnapDirection::kSecondary) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxRight"));
  } else {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
    return false;
  }

  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kNo);
  return true;
}

void FrameSizeButton::CancelSnap() {
  snapping_window_observer_.reset();
  delegate_->CommitSnap(SnapDirection::kNone);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);
  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
}

void FrameSizeButton::SetButtonsToNormalMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = false;
  if (pie_animation_view_) {
    pie_animation_view_->Stop();
  }
  set_buttons_to_snap_mode_timer_.Stop();
  delegate_->SetButtonsToNormal(animate);
}

BEGIN_METADATA(FrameSizeButton, views::FrameCaptionButton)
END_METADATA

}  // namespace chromeos
