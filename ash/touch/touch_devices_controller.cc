// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_devices_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {

namespace {

void OnSetTouchpadEnabledDone(bool enabled, bool succeeded) {
  // Don't log here, |succeeded| is only true if there is a touchpad *and* the
  // value changed. In other words |succeeded| is false when not on device or
  // the value was already at the value specified. Neither of these are
  // interesting failures.
  if (!succeeded)
    return;

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  if (!cursor_manager)
    return;

  if (enabled)
    cursor_manager->ShowCursor();
  else
    cursor_manager->HideCursor();
}

ui::InputDeviceControllerClient* GetInputDeviceControllerClient() {
  return Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
}

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

// static
void TouchDevicesController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kTouchpadEnabled, PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(prefs::kTouchscreenEnabled,
                                PrefRegistry::PUBLIC);
}

TouchDevicesController::TouchDevicesController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

TouchDevicesController::~TouchDevicesController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void TouchDevicesController::ToggleTouchpad() {
  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  const bool touchpad_enabled = prefs->GetBoolean(prefs::kTouchpadEnabled);
  prefs->SetBoolean(prefs::kTouchpadEnabled, !touchpad_enabled);
}

bool TouchDevicesController::GetTouchpadEnabled(
    TouchDeviceEnabledSource source) const {
  if (source == TouchDeviceEnabledSource::GLOBAL)
    return global_touchpad_enabled_;

  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchpadEnabled);
}

void TouchDevicesController::SetTouchpadEnabled(
    bool enabled,
    TouchDeviceEnabledSource source) {
  if (source == TouchDeviceEnabledSource::GLOBAL) {
    global_touchpad_enabled_ = enabled;
    UpdateTouchpadEnabled();
    return;
  }

  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kTouchpadEnabled, enabled);
}

bool TouchDevicesController::GetTouchscreenEnabled(
    TouchDeviceEnabledSource source) const {
  if (source == TouchDeviceEnabledSource::GLOBAL)
    return global_touchscreen_enabled_;

  PrefService* prefs = GetActivePrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchscreenEnabled);
}

void TouchDevicesController::SetTouchscreenEnabled(
    bool enabled,
    TouchDeviceEnabledSource source) {
  if (source == TouchDeviceEnabledSource::GLOBAL) {
    global_touchscreen_enabled_ = enabled;
    // Explicitly call |UpdateTouchscreenEnabled()| to update the actual
    // touchscreen state from multiple sources.
    UpdateTouchscreenEnabled();
    return;
  }

  PrefService* prefs = GetActivePrefService();
  if (!prefs)
    return;
  prefs->SetBoolean(prefs::kTouchscreenEnabled, enabled);
}

void TouchDevicesController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void TouchDevicesController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  ObservePrefs(prefs);
}

void TouchDevicesController::ObservePrefs(PrefService* prefs) {
  // Watch for pref updates.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      prefs::kTouchpadEnabled,
      base::Bind(&TouchDevicesController::UpdateTouchpadEnabled,
                 base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kTouchscreenEnabled,
      base::Bind(&TouchDevicesController::UpdateTouchscreenEnabled,
                 base::Unretained(this)));
  // Load current state.
  UpdateTouchpadEnabled();
  UpdateTouchscreenEnabled();
}

void TouchDevicesController::UpdateTouchpadEnabled() {
  if (!GetInputDeviceControllerClient())
    return;  // Happens in tests.

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs)
    return;

  bool enabled = global_touchpad_enabled_ &&
                 prefs->GetBoolean(prefs::kTouchpadEnabled);

  GetInputDeviceControllerClient()->SetInternalTouchpadEnabled(
      enabled, base::BindRepeating(&OnSetTouchpadEnabledDone, enabled));
}

void TouchDevicesController::UpdateTouchscreenEnabled() {
  if (!GetInputDeviceControllerClient())
    return;  // Happens in tests.

  GetInputDeviceControllerClient()->SetTouchscreensEnabled(
      GetTouchscreenEnabled(TouchDeviceEnabledSource::GLOBAL) &&
      GetTouchscreenEnabled(TouchDeviceEnabledSource::USER_PREF));
}

}  // namespace ash
