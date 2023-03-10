// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <memory>

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_finish_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/educational_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/message_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

namespace {
// UI specs.
constexpr int kMenuEntrySize = 56;
constexpr int kMenuEntrySideMargin = 24;
constexpr SkColor kMenuEntryBgColor = SkColorSetA(SK_ColorWHITE, 0x99);
constexpr int kMenuEntryCornerRadius = 8;
constexpr int kNudgeVerticalAlign = 8;
constexpr int kNudgeHeight = 40;

// About focus ring.
// Gap between focus ring outer edge to label.
constexpr float kHaloInset = -4;
// Thickness of focus ring.
constexpr float kHaloThickness = 2;

}  // namespace

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector,
    bool first_launch)
    : touch_injector_(touch_injector) {
  touch_injector_->set_display_overlay_controller(this);

  // There is no instance for unittest.
  if (!ash::Shell::HasInstance())
    return;

  AddOverlay(first_launch ? DisplayMode::kEducation : DisplayMode::kView);
  ash::Shell::Get()->AddPreTargetHandler(this);
  if (auto* dark_light_mode_controller =
          ash::DarkLightModeControllerImpl::Get()) {
    dark_light_mode_controller->AddObserver(this);
  }
}

DisplayOverlayController::~DisplayOverlayController() {
  touch_injector_->set_display_overlay_controller(nullptr);

  // There is no instance for unittest.
  if (!ash::Shell::HasInstance())
    return;

  if (auto* dark_light_mode_controller =
          ash::DarkLightModeControllerImpl::Get()) {
    dark_light_mode_controller->RemoveObserver(this);
  }
  ash::Shell::Get()->RemovePreTargetHandler(this);
  RemoveOverlayIfAny();
}

// For test:
gfx::Rect DisplayOverlayController::GetInputMappingViewBoundsForTesting() {
  return input_mapping_view_ ? input_mapping_view_->bounds() : gfx::Rect();
}

void DisplayOverlayController::AddOverlay(DisplayMode display_mode) {
  RemoveOverlayIfAny();
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  if (!shell_surface_base)
    return;

  auto view = std::make_unique<views::View>();
  exo::ShellSurfaceBase::OverlayParams params(std::move(view));
  params.translucent = true;
  params.overlaps_frame = false;
  params.focusable = true;
  shell_surface_base->AddOverlay(std::move(params));

  auto* overlay_widget = GetOverlayWidget();
  if (overlay_widget)
    overlay_widget->AddObserver(this);

  SetDisplayMode(display_mode);
}

void DisplayOverlayController::RemoveOverlayIfAny() {
  if (display_mode_ == DisplayMode::kEdit)
    OnCustomizeCancel();
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  if (shell_surface_base && shell_surface_base->HasOverlay()) {
    // Call |RemoveInputMenuView| explicitly to make sure UMA stats is updated.
    RemoveInputMenuView();

    auto* overlay_widget = GetOverlayWidget();
    if (overlay_widget)
      overlay_widget->RemoveObserver(this);

    shell_surface_base->RemoveOverlay();
  }
}

void DisplayOverlayController::SetEventTarget(views::Widget* overlay_widget,
                                              bool on_overlay) {
  overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
      on_overlay ? aura::EventTargetingPolicy::kTargetAndDescendants
                 : aura::EventTargetingPolicy::kNone);
}

void DisplayOverlayController::AddNudgeView(views::Widget* overlay_widget) {
  if (nudge_view_)
    return;
  DCHECK(overlay_widget);
  auto nudge_view = std::make_unique<ash::PillButton>(
      base::BindRepeating(&DisplayOverlayController::OnNudgeDismissed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_SETTINGS_NUDGE_ALPHA),
      ash::PillButton::Type::kDefaultWithIconLeading, &kTipIcon);
  nudge_view->SetSize(
      gfx::Size(nudge_view->GetPreferredSize().width(), kNudgeHeight));
  nudge_view->SetButtonTextColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kNudgeLabelColor, IsDarkModeEnabled()));
  nudge_view->SetBackgroundColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kNudgeBackgroundColor, IsDarkModeEnabled()));
  nudge_view->SetIconColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kNudgeIconColor, IsDarkModeEnabled()));
  nudge_view->SetPosition(CalculateNudgePosition(nudge_view->width()));

  auto* parent = overlay_widget->GetContentsView();
  DCHECK(parent);
  nudge_view_ = parent->AddChildView(std::move(nudge_view));
}

void DisplayOverlayController::RemoveNudgeView() {
  if (!nudge_view_)
    return;
  nudge_view_->parent()->RemoveChildViewT(nudge_view_);
  nudge_view_ = nullptr;
}

void DisplayOverlayController::OnNudgeDismissed() {
  RemoveNudgeView();
  DCHECK(touch_injector_);
  touch_injector_->set_show_nudge(false);
}

gfx::Point DisplayOverlayController::CalculateNudgePosition(int nudge_width) {
  gfx::Point nudge_position = CalculateMenuEntryPosition();
  int x = nudge_position.x() - nudge_width - kMenuEntrySideMargin;
  int y = nudge_position.y() + kNudgeVerticalAlign;
  // If the nudge view shows at the outside of the window, move the nudge view
  // down below the menu button and move it to left to make sure it shows inside
  // of the window.
  if (x < 0) {
    x = std::max(0, x + menu_entry_->width() + kMenuEntrySideMargin);
    y += menu_entry_->height();
  }

  return gfx::Point(x, y);
}

void DisplayOverlayController::AddMenuEntryView(views::Widget* overlay_widget) {
  if (menu_entry_) {
    menu_entry_->SetVisible(true);
    return;
  }
  DCHECK(overlay_widget);
  auto game_icon = gfx::CreateVectorIcon(
      vector_icons::kVideogameAssetOutlineIcon, SK_ColorBLACK);

  // Create and position entry point for |InputMenuView|.
  auto menu_entry = std::make_unique<MenuEntryView>(
      base::BindRepeating(&DisplayOverlayController::OnMenuEntryPressed,
                          base::Unretained(this)),
      base::BindRepeating(&DisplayOverlayController::OnMenuEntryPositionChanged,
                          base::Unretained(this)));
  menu_entry->SetImage(views::Button::STATE_NORMAL, game_icon);
  menu_entry->SetBackground(views::CreateRoundedRectBackground(
      kMenuEntryBgColor, kMenuEntryCornerRadius));
  menu_entry->SetSize(gfx::Size(kMenuEntrySize, kMenuEntrySize));
  menu_entry->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  menu_entry->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  menu_entry->SetPosition(CalculateMenuEntryPosition());
  menu_entry->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_GAME_CONTROLS_ALPHA));

  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  menu_entry_ = parent_view->AddChildView(std::move(menu_entry));

  // Set up focus ring for |menu_entry_|.
  views::InstallRoundRectHighlightPathGenerator(menu_entry_, gfx::Insets(),
                                                kMenuEntryCornerRadius);
  ash::StyleUtil::SetUpInkDropForButton(menu_entry_, gfx::Insets(),
                                        /*highlight_on_hover=*/true,
                                        /*highlight_on_focus=*/true);
  auto* focus_ring = views::FocusRing::Get(menu_entry_);
  focus_ring->SetHaloInset(kHaloInset);
  focus_ring->SetHaloThickness(kHaloThickness);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
}

void DisplayOverlayController::RemoveMenuEntryView() {
  if (!menu_entry_)
    return;
  menu_entry_->parent()->RemoveChildViewT(menu_entry_);
  menu_entry_ = nullptr;
}

void DisplayOverlayController::OnMenuEntryPressed() {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);

  SetDisplayMode(DisplayMode::kMenu);

  input_menu_view_ = parent_view->AddChildView(
      InputMenuView::BuildMenuView(this, menu_entry_, parent_view->size()));
  // Hide the menu entry when the menu is displayed.
  menu_entry_->SetVisible(false);
}

void DisplayOverlayController::OnMenuEntryPositionChanged(
    bool leave_focus,
    absl::optional<gfx::Point> location) {
  if (leave_focus)
    SetDisplayMode(DisplayMode::kView);

  if (location)
    touch_injector_->SaveMenuEntryLocation(*location);
}

void DisplayOverlayController::FocusOnMenuEntry() {
  if (!menu_entry_)
    return;
  menu_entry_->RequestFocus();
}

void DisplayOverlayController::ClearFocusOnMenuEntry() {
  if (!menu_entry_)
    return;
  auto* focus_manager = menu_entry_->GetFocusManager();
  if (focus_manager)
    focus_manager->ClearFocus();
}

void DisplayOverlayController::RemoveInputMenuView() {
  if (!input_menu_view_)
    return;
  input_menu_view_->parent()->RemoveChildViewT(input_menu_view_);
  input_menu_view_ = nullptr;
  touch_injector_->OnInputMenuViewRemoved();
}

void DisplayOverlayController::AddInputMappingView(
    views::Widget* overlay_widget) {
  if (input_mapping_view_)
    return;
  DCHECK(overlay_widget);
  auto input_mapping_view = std::make_unique<InputMappingView>(this);
  input_mapping_view->SetPosition(gfx::Point());
  input_mapping_view_ = overlay_widget->GetContentsView()->AddChildView(
      std::move(input_mapping_view));

  // Set input mapping view visibility according to the saved status.
  DCHECK(touch_injector_);
  if (touch_injector_)
    SetInputMappingVisible(touch_injector_->input_mapping_visible());
}

void DisplayOverlayController::RemoveInputMappingView() {
  if (!input_mapping_view_)
    return;
  input_mapping_view_->parent()->RemoveChildViewT(input_mapping_view_);
  input_mapping_view_ = nullptr;
}

void DisplayOverlayController::AddEditFinishView(
    views::Widget* overlay_widget) {
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);

  edit_finish_view_ = parent_view->AddChildView(
      EditFinishView::BuildView(this, parent_view->size()));

  // Since |input_menu_view_| is removed when adding |edit_finish_view_| and
  // |FocusManager| lost the focused view. Set |edit_finish_view_| explicitly
  // as the focused view so Tab traversal key can work as expected.
  auto* focus_manager = edit_finish_view_->GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(edit_finish_view_);
}

void DisplayOverlayController::RemoveEditFinishView() {
  if (!edit_finish_view_)
    return;
  edit_finish_view_->parent()->RemoveChildViewT(edit_finish_view_);
  edit_finish_view_ = nullptr;
}

void DisplayOverlayController::AddEducationalView() {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (educational_view_)
    return;

  educational_view_ = EducationalView::Show(this, GetParentView());
}

void DisplayOverlayController::RemoveEducationalView() {
  if (!educational_view_)
    return;
  educational_view_->parent()->RemoveChildViewT(educational_view_);
  educational_view_ = nullptr;
}

void DisplayOverlayController::OnEducationalViewDismissed() {
  SetDisplayMode(DisplayMode::kView);
  DCHECK(touch_injector_);
  touch_injector_->set_first_launch(false);
}

void DisplayOverlayController::AddButtonForAddActionTap() {
  if (add_action_tap_)
    return;
  auto add_action_tap = std::make_unique<ash::PillButton>(
      base::BindRepeating(
          &DisplayOverlayController::OnAddActionTapButtonPressed,
          base::Unretained(this)),
      u"Add Action Tap", ash::PillButton::Type::kDefaultWithoutIcon,
      /*icon=*/nullptr);
  add_action_tap->SetSize(add_action_tap->GetPreferredSize());

  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  add_action_tap_ = parent_view->AddChildView(std::move(add_action_tap));
  add_action_tap_->SetButtonTextColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kButtonLabelColorPrimary, IsDarkModeEnabled()));
  add_action_tap_->SetBackgroundColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kButtonBackgroundColorPrimary,
      IsDarkModeEnabled()));
  add_action_tap_->SetPosition(
      gfx::Point(parent_view->width() - add_action_tap_->width(), 0));
}

void DisplayOverlayController::RemoveButtonForAddActionTap() {
  if (!add_action_tap_)
    return;
  add_action_tap_->parent()->RemoveChildViewT(add_action_tap_);
  add_action_tap_ = nullptr;
}

void DisplayOverlayController::OnAddActionTapButtonPressed() {
  touch_injector_->AddNewAction(ActionType::TAP);
}

void DisplayOverlayController::AddButtonForAddActionMove() {
  auto add_action_move = std::make_unique<ash::PillButton>(
      base::BindRepeating(
          &DisplayOverlayController::OnAddActionMoveButtonPressed,
          base::Unretained(this)),
      u"Add Action Move", ash::PillButton::Type::kDefaultWithoutIcon,
      /*icon=*/nullptr);
  add_action_move->SetSize(add_action_move->GetPreferredSize());

  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  add_action_move_ = parent_view->AddChildView(std::move(add_action_move));
  add_action_move_->SetButtonTextColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kButtonLabelColorPrimary, IsDarkModeEnabled()));
  add_action_move_->SetBackgroundColor(cros_styles::ResolveColor(
      cros_styles::ColorName::kButtonBackgroundColorPrimary,
      IsDarkModeEnabled()));
  add_action_move_->SetPosition(
      gfx::Point(parent_view->width() - add_action_move_->width(),
                 add_action_tap_->height()));
}

void DisplayOverlayController::RemoveButtonForAddActionMove() {
  if (!add_action_move_)
    return;
  add_action_move_->parent()->RemoveChildViewT(add_action_move_);
  add_action_move_ = nullptr;
}

void DisplayOverlayController::OnAddActionMoveButtonPressed() {
  touch_injector_->AddNewAction(ActionType::MOVE);
}

void DisplayOverlayController::OnActionTrashButtonPressed(Action* action) {
  touch_injector_->RemoveAction(action);
}

views::Widget* DisplayOverlayController::GetOverlayWidget() {
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  DCHECK(shell_surface_base);

  return shell_surface_base ? static_cast<views::Widget*>(
                                  shell_surface_base->GetFocusTraversable())
                            : nullptr;
}

gfx::Point DisplayOverlayController::CalculateMenuEntryPosition() {
  if (touch_injector_->allow_reposition() &&
      touch_injector_->menu_entry_location()) {
    auto normalized_location = touch_injector_->menu_entry_location();
    auto content_bounds = touch_injector_->content_bounds();

    return gfx::Point(static_cast<int>(std::round(normalized_location->x() *
                                                  content_bounds.width())),
                      static_cast<int>(std::round(normalized_location->y() *
                                                  content_bounds.height())));
  } else {
    auto* overlay_widget = GetOverlayWidget();
    if (!overlay_widget)
      return gfx::Point();
    auto* view = overlay_widget->GetContentsView();
    if (!view || view->bounds().IsEmpty())
      return gfx::Point();

    return gfx::Point(
        std::max(0, view->width() - kMenuEntrySize - kMenuEntrySideMargin),
        std::max(0, view->height() / 2 - kMenuEntrySize / 2));
  }
}

views::View* DisplayOverlayController::GetParentView() {
  auto* overlay_widget = GetOverlayWidget();
  if (!overlay_widget)
    return nullptr;
  return overlay_widget->GetContentsView();
}

void DisplayOverlayController::SetDisplayMode(DisplayMode mode) {
  if (display_mode_ == mode)
    return;

  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;

  switch (mode) {
    case DisplayMode::kNone:
      RemoveEditMessage();
      RemoveMenuEntryView();
      RemoveInputMappingView();
      RemoveEducationalView();
      RemoveEditFinishView();
      RemoveNudgeView();
      break;
    case DisplayMode::kEducation:
      // Force recreating educational view as it is responsive to width changes.
      RemoveEducationalView();
      AddEducationalView();
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      break;
    case DisplayMode::kView:
      RemoveEditMessage();
      RemoveInputMenuView();
      RemoveEditFinishView();
      RemoveEducationalView();
      RemoveNudgeView();
      if (ash::features::IsArcInputOverlayBetaEnabled()) {
        RemoveButtonForAddActionTap();
        RemoveButtonForAddActionMove();
      }
      AddInputMappingView(overlay_widget);
      AddMenuEntryView(overlay_widget);
      ClearFocusOnMenuEntry();
      if (touch_injector_->show_nudge())
        AddNudgeView(overlay_widget);
      SetEventTarget(overlay_widget, /*on_overlay=*/false);
      break;
    case DisplayMode::kEdit:
      RemoveInputMenuView();
      RemoveMenuEntryView();
      RemoveEducationalView();
      RemoveNudgeView();
      AddEditFinishView(overlay_widget);
      if (ash::features::IsArcInputOverlayBetaEnabled()) {
        AddButtonForAddActionTap();
        AddButtonForAddActionMove();
      }
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      break;
    case DisplayMode::kPreMenu:
      RemoveNudgeView();
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      FocusOnMenuEntry();
      break;
    case DisplayMode::kMenu:
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (input_mapping_view_)
    input_mapping_view_->SetDisplayMode(mode);

  DCHECK(touch_injector_);
  if (touch_injector_)
    touch_injector_->set_display_mode(mode);

  display_mode_ = mode;
}

absl::optional<gfx::Rect>
DisplayOverlayController::GetOverlayMenuEntryBounds() {
  if (!menu_entry_ || !menu_entry_->GetVisible())
    return absl::nullopt;

  return absl::optional<gfx::Rect>(menu_entry_->GetBoundsInScreen());
}

void DisplayOverlayController::AddActionEditMenu(ActionView* anchor,
                                                 ActionType action_type) {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (!parent_view)
    return;
  auto action_edit_menu =
      ActionEditMenu::BuildActionEditMenu(this, anchor, action_type);
  if (action_edit_menu)
    action_edit_menu_ = parent_view->AddChildView(std::move(action_edit_menu));
}

void DisplayOverlayController::RemoveActionEditMenu() {
  if (!action_edit_menu_)
    return;
  action_edit_menu_->parent()->RemoveChildViewT(action_edit_menu_);
  action_edit_menu_ = nullptr;
}

void DisplayOverlayController::AddEditMessage(const base::StringPiece& message,
                                              MessageType message_type) {
  // There is no instance for unittest.
  if (!ash::Shell::HasInstance())
    return;

  RemoveEditMessage();
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (!parent_view)
    return;
  message_ = MessageView::Show(this, parent_view, message, message_type);
}

void DisplayOverlayController::RemoveEditMessage() {
  if (!message_)
    return;
  message_->parent()->RemoveChildViewT(message_);
  message_ = nullptr;
}

void DisplayOverlayController::OnInputBindingChange(
    Action* action,
    std::unique_ptr<InputElement> input_element) {
  touch_injector_->OnInputBindingChange(action, std::move(input_element));
}

void DisplayOverlayController::OnCustomizeSave() {
  touch_injector_->OnBindingSave();
}

void DisplayOverlayController::OnCustomizeCancel() {
  touch_injector_->OnBindingCancel();
}

void DisplayOverlayController::OnCustomizeRestore() {
  touch_injector_->OnBindingRestore();
}

const std::string& DisplayOverlayController::GetPackageName() const {
  return touch_injector_->package_name();
}

void DisplayOverlayController::OnApplyMenuState() {
  if (display_mode_ != DisplayMode::kView)
    return;

  SetInputMappingVisible(GetTouchInjectorEnable() &&
                         GetInputMappingViewVisible());
}

void DisplayOverlayController::OnActionAdded(Action* action) {
  input_mapping_view_->OnActionAdded(action);
}

void DisplayOverlayController::OnActionRemoved(Action* action) {
  input_mapping_view_->OnActionRemoved(action);
}

void DisplayOverlayController::OnMouseEvent(ui::MouseEvent* event) {
  if ((display_mode_ == DisplayMode::kView && !nudge_view_) ||
      event->type() != ui::ET_MOUSE_PRESSED) {
    return;
  }

  ProcessPressedEvent(*event);
}

void DisplayOverlayController::OnTouchEvent(ui::TouchEvent* event) {
  if ((display_mode_ == DisplayMode::kView && !nudge_view_) ||
      event->type() != ui::ET_TOUCH_PRESSED) {
    return;
  }
  ProcessPressedEvent(*event);
}

void DisplayOverlayController::OnColorModeChanged(bool dark_mode_enabled) {
  // Only make the color mode change responsive when in
  // |DisplayMode::kEducation| because:
  // 1. Other modes like |DisplayMode::kEdit| and |DisplayMode::kView| only have
  // one color mode.
  // 2. When in |DisplayMode::kMenu| and changing the color mode, the menu is
  // closed and it becomes |DisplayMode::kView| so no need to update color mode.
  if (display_mode_ != DisplayMode::kEducation)
    return;
  SetDisplayMode(DisplayMode::kNone);
  SetDisplayMode(DisplayMode::kEducation);
}

void DisplayOverlayController::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  touch_injector_->UpdateForOverlayBoundsChanged(gfx::RectF(new_bounds));

  // Overlay |widget| is null for test.
  if (!widget)
    return;

  auto mode = display_mode_;
  SetDisplayMode(DisplayMode::kNone);
  // Transition to |kView| mode except while on |kEducation| mode since
  // displaying this UI needs to be ensured as the user shouldn't be able to
  // manually access said view.
  if (mode != DisplayMode::kEducation) {
    mode = DisplayMode::kView;
  }

  SetDisplayMode(mode);
}

bool DisplayOverlayController::HasMenuView() const {
  return input_menu_view_ != nullptr;
}

void DisplayOverlayController::SetInputMappingVisible(bool visible) {
  if (!input_mapping_view_)
    return;
  input_mapping_view_->SetVisible(visible);
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return;
  touch_injector_->store_input_mapping_visible(visible);
}

bool DisplayOverlayController::GetInputMappingViewVisible() const {
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return false;
  return touch_injector_->input_mapping_visible();
}

void DisplayOverlayController::SetTouchInjectorEnable(bool enable) {
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return;
  touch_injector_->store_touch_injector_enable(enable);
}

bool DisplayOverlayController::GetTouchInjectorEnable() {
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return false;
  return touch_injector_->touch_injector_enable();
}

void DisplayOverlayController::ProcessPressedEvent(
    const ui::LocatedEvent& event) {
  if (!action_edit_menu_ && !message_ && !input_menu_view_ && !nudge_view_)
    return;

  auto root_location = event.root_location();
  // Convert the LocatedEvent root location to screen location.
  auto origin =
      touch_injector_->window()->GetRootWindow()->GetBoundsInScreen().origin();
  root_location.Offset(origin.x(), origin.y());

  if (action_edit_menu_) {
    auto bounds = action_edit_menu_->GetBoundsInScreen();
    if (!bounds.Contains(root_location))
      RemoveActionEditMenu();
  }

  if (message_) {
    auto bounds = message_->GetBoundsInScreen();
    if (!bounds.Contains(root_location))
      RemoveEditMessage();
  }

  if (input_menu_view_) {
    auto bounds = input_menu_view_->GetBoundsInScreen();
    if (!bounds.Contains(root_location))
      SetDisplayMode(DisplayMode::kView);
  }

  // Dismiss the nudge, regardless where the click was.
  if (nudge_view_)
    OnNudgeDismissed();
}

void DisplayOverlayController::DismissEducationalViewForTesting() {
  OnEducationalViewDismissed();
}

void DisplayOverlayController::TriggerWidgetBoundsChangedForTesting() {
  auto bounds = CalculateWindowContentBounds(touch_injector_->window());
  OnWidgetBoundsChanged(
      /*widget=*/nullptr,
      gfx::Rect(bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

}  // namespace arc::input_overlay
