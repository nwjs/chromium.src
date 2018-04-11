// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_devices_controller.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/session/session_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@test.com";
constexpr char kUser2Email[] = "user2@test.com";

bool GetUserPrefTouchpadEnabled() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs && prefs->GetBoolean(prefs::kTouchpadEnabled);
}

bool GetGlobalTouchpadEnabled() {
  return Shell::Get()->touch_devices_controller()->GetTouchpadEnabled(
      TouchDeviceEnabledSource::GLOBAL);
}

bool GetUserPrefTouchscreenEnabled() {
  return Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::USER_PREF);
}

bool GetGlobalTouchscreenEnabled() {
  return Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL);
}

class TouchDevicesControllerTest : public NoSessionAshTestBase {
 public:
  TouchDevicesControllerTest() = default;
  ~TouchDevicesControllerTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshDebugShortcuts);
    NoSessionAshTestBase::SetUp();
    CreateTestUserSessions();

    // Simulate user 1 login.
    SwitchActiveUser(kUser1Email);

    ASSERT_TRUE(debug::DebugAcceleratorsEnabled());
  }

  void CreateTestUserSessions() {
    GetSessionControllerClient()->Reset();
    GetSessionControllerClient()->AddUserSession(kUser1Email);
    GetSessionControllerClient()->AddUserSession(kUser2Email);
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchDevicesControllerTest);
};

// Tests that touchpad enabled user pref works properly under debug accelerator.
TEST_F(TouchDevicesControllerTest, ToggleTouchpad) {
  ASSERT_TRUE(GetUserPrefTouchpadEnabled());
  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_PAD);
  EXPECT_FALSE(GetUserPrefTouchpadEnabled());

  // Switch to user 2 and switch back.
  SwitchActiveUser(kUser2Email);
  EXPECT_TRUE(GetUserPrefTouchpadEnabled());
  SwitchActiveUser(kUser1Email);
  EXPECT_FALSE(GetUserPrefTouchpadEnabled());

  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_PAD);
  EXPECT_TRUE(GetUserPrefTouchpadEnabled());
}

TEST_F(TouchDevicesControllerSigninTest, SetTouchpadEnabled) {
  ASSERT_TRUE(GetUserPrefTouchpadEnabled());
  ASSERT_TRUE(GetGlobalTouchpadEnabled());

  Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
      false, TouchDeviceEnabledSource::GLOBAL);
  ASSERT_TRUE(GetUserPrefTouchpadEnabled());
  ASSERT_FALSE(GetGlobalTouchpadEnabled());

  Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
      false, TouchDeviceEnabledSource::USER_PREF);
  ASSERT_FALSE(GetUserPrefTouchpadEnabled());
  ASSERT_FALSE(GetGlobalTouchpadEnabled());

  Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
      true, TouchDeviceEnabledSource::GLOBAL);
  ASSERT_FALSE(GetUserPrefTouchpadEnabled());
  ASSERT_TRUE(GetGlobalTouchpadEnabled());
}

// Tests that touchscreen enabled user pref works properly under debug
// accelerator.
TEST_F(TouchDevicesControllerTest, SetTouchscreenEnabled) {
  ASSERT_TRUE(GetGlobalTouchscreenEnabled());
  ASSERT_TRUE(GetUserPrefTouchscreenEnabled());

  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_SCREEN);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
  EXPECT_FALSE(GetUserPrefTouchscreenEnabled());

  // Switch to user 2 and switch back.
  SwitchActiveUser(kUser2Email);
  EXPECT_TRUE(GetUserPrefTouchscreenEnabled());
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());
  EXPECT_FALSE(GetUserPrefTouchscreenEnabled());

  debug::PerformDebugActionIfEnabled(DEBUG_TOGGLE_TOUCH_SCREEN);
  EXPECT_TRUE(GetUserPrefTouchscreenEnabled());
  EXPECT_TRUE(GetGlobalTouchscreenEnabled());

  // The global setting should be preserved when switching users.
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      false, TouchDeviceEnabledSource::GLOBAL);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(GetGlobalTouchscreenEnabled());
}

}  // namespace
}  // namespace ash
