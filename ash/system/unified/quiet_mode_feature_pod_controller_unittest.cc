// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quiet_mode_feature_pod_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

// Tests manually control their session state.
class QuietModeFeaturePodControllerTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  QuietModeFeaturePodControllerTest() = default;

  QuietModeFeaturePodControllerTest(const QuietModeFeaturePodControllerTest&) =
      delete;
  QuietModeFeaturePodControllerTest& operator=(
      const QuietModeFeaturePodControllerTest&) = delete;

  ~QuietModeFeaturePodControllerTest() override = default;

  void SetUp() override {
    auto enabled_features = std::vector<base::test::FeatureRef>();
    if (IsOsSettingsAppBadgingToggleEnabled()) {
      enabled_features.push_back(features::kOsSettingsAppBadgingToggle);
    }
    if (IsQsRevampEnabled()) {
      enabled_features.push_back(features::kQsRevamp);
    }
    feature_list_.InitWithFeatures(enabled_features, /*disabled_features*/ {});
    NoSessionAshTestBase::SetUp();

    GetPrimaryUnifiedSystemTray()->ShowBubble();
  }

  void TearDown() override {
    if (IsQsRevampEnabled()) {
      tile_.reset();
    } else {
      button_.reset();
    }
    controller_.reset();
    NoSessionAshTestBase::TearDown();
  }

  bool IsOsSettingsAppBadgingToggleEnabled() { return std::get<0>(GetParam()); }

  bool IsQsRevampEnabled() { return std::get<1>(GetParam()); }

  void SetUpButton() {
    controller_ =
        std::make_unique<QuietModeFeaturePodController>(tray_controller());
    if (IsQsRevampEnabled()) {
      tile_ = controller_->CreateTile();
    } else {
      button_.reset(controller_->CreateButton());
    }
  }

  UnifiedSystemTrayController* tray_controller() {
    return GetPrimaryUnifiedSystemTray()
        ->bubble()
        ->unified_system_tray_controller();
  }

  void PressIcon() { controller_->OnIconPressed(); }

  void PressLabel() { controller_->OnLabelPressed(); }

  bool IsButtonVisible() {
    return IsQsRevampEnabled() ? tile_->GetVisible() : button_->GetVisible();
  }

  FeaturePodButton* button() { return button_.get(); }

 private:
  std::unique_ptr<QuietModeFeaturePodController> controller_;
  std::unique_ptr<FeaturePodButton> button_;
  std::unique_ptr<FeatureTile> tile_;

  base::test::ScopedFeatureList feature_list_;
};
INSTANTIATE_TEST_SUITE_P(
    All,
    QuietModeFeaturePodControllerTest,
    testing::Combine(
        testing::Bool() /* IsOsSettingsAppBadgingToggleEnabled() */,
        testing::Bool() /* IsQsRevampEnabled */));

TEST_P(QuietModeFeaturePodControllerTest, ButtonVisibilityNotLoggedIn) {
  SetUpButton();
  // If not logged in, it should not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(QuietModeFeaturePodControllerTest, ButtonVisibilityLoggedIn) {
  CreateUserSessions(1);
  SetUpButton();
  // If logged in, it should be visible.
  EXPECT_TRUE(IsButtonVisible());
}

TEST_P(QuietModeFeaturePodControllerTest, ButtonVisibilityLocked) {
  CreateUserSessions(1);
  BlockUserSession(UserSessionBlockReason::BLOCKED_BY_LOCK_SCREEN);
  SetUpButton();
  // If locked, it should not be visible.
  EXPECT_FALSE(IsButtonVisible());
}

TEST_P(QuietModeFeaturePodControllerTest, IconUMATracking) {
  // TODO(b/263505103): Implement Feature Tile metrics.
  if (IsQsRevampEnabled()) {
    return;
  }

  CreateUserSessions(1);
  SetUpButton();
  message_center::MessageCenter::Get()->SetQuietMode(false);

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Turn on quiet mode when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      QsFeatureCatalogName::kQuietMode,
      /*expected_count=*/1);

  // Turn off quiet mode when pressing on the icon.
  PressIcon();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/1);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/1);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);
  histogram_tester->ExpectBucketCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      QsFeatureCatalogName::kQuietMode,
      /*expected_count=*/1);
}

TEST_P(QuietModeFeaturePodControllerTest, LabelUMATracking) {
  // Qs Revamp Feature Tile does not have a detailed view.
  if (IsQsRevampEnabled()) {
    return;
  }

  CreateUserSessions(1);
  SetUpButton();

  // No metrics logged before clicking on any views.
  auto histogram_tester = std::make_unique<base::HistogramTester>();
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
      /*count=*/0);
  histogram_tester->ExpectTotalCount(
      "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
      /*count=*/0);
  histogram_tester->ExpectTotalCount("Ash.UnifiedSystemView.FeaturePod.DiveIn",
                                     /*count=*/0);

  // Show quiet mode detailed view when pressing on the label.
  PressLabel();
  if (IsOsSettingsAppBadgingToggleEnabled()) {
    // A press on the label should toggle the feature. The detailed view has
    // been removed, and the settings were moved to OSSettings.
    histogram_tester->ExpectTotalCount(
        "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
        /*count=*/1);
    histogram_tester->ExpectTotalCount(
        "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
        /*count=*/0);
    histogram_tester->ExpectTotalCount(
        "Ash.UnifiedSystemView.FeaturePod.DiveIn",
        /*count=*/0);
  } else {
    histogram_tester->ExpectTotalCount(
        "Ash.UnifiedSystemView.FeaturePod.ToggledOn",
        /*count=*/0);
    histogram_tester->ExpectTotalCount(
        "Ash.UnifiedSystemView.FeaturePod.ToggledOff",
        /*count=*/0);
    histogram_tester->ExpectTotalCount(
        "Ash.UnifiedSystemView.FeaturePod.DiveIn",
        /*count=*/1);
    histogram_tester->ExpectBucketCount(
        "Ash.UnifiedSystemView.FeaturePod.DiveIn",
        QsFeatureCatalogName::kQuietMode,
        /*expected_count=*/1);
  }
}

}  // namespace ash
