// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_tray.h"

#include "ash/constants/ash_features.h"
#include "ash/shelf/shelf.h"
#include "ash/style/icon_button.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_media_state.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class VideoConferenceTrayTest : public AshTestBase {
 public:
  VideoConferenceTrayTest() = default;
  VideoConferenceTrayTest(const VideoConferenceTrayTest&) = delete;
  VideoConferenceTrayTest& operator=(const VideoConferenceTrayTest&) = delete;
  ~VideoConferenceTrayTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kVideoConference);

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not do it in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    set_create_global_cras_audio_handler(false);
    AshTestBase::SetUp();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  VideoConferenceTray* video_conference_tray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  IconButton* toggle_bubble_button() {
    return video_conference_tray()->toggle_bubble_button_;
  }

  VideoConferenceTrayButton* camera_icon() {
    return video_conference_tray()->camera_icon();
  }

  VideoConferenceTrayButton* audio_icon() {
    return video_conference_tray()->audio_icon();
  }

  // The video conference tray and its buttons are, by default, not visible.
  // However, we would want to make it visible for testing.
  void SetTrayAndButtonsVisible() {
    video_conference_tray()->SetVisiblePreferred(true);
    camera_icon()->SetVisible(true);
    audio_icon()->SetVisible(true);
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
};

TEST_F(VideoConferenceTrayTest, ClickTrayButton) {
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(video_conference_tray()->GetBubbleView());

  // Clicking the toggle button should construct and open up the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());
  EXPECT_TRUE(toggle_bubble_button()->toggled());

  // Clicking it again should reset the bubble.
  LeftClickOn(toggle_bubble_button());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());

  LeftClickOn(toggle_bubble_button());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView());
  EXPECT_TRUE(video_conference_tray()->GetBubbleView()->GetVisible());
  EXPECT_TRUE(toggle_bubble_button()->toggled());

  // Click anywhere else outside the bubble (i.e. the status area button) should
  // close the bubble.
  LeftClickOn(
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray());
  EXPECT_FALSE(video_conference_tray()->GetBubbleView());
  EXPECT_FALSE(toggle_bubble_button()->toggled());
}

TEST_F(VideoConferenceTrayTest, ToggleBubbleButtonRotation) {
  SetTrayAndButtonsVisible();

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);

  // When the bubble is not open in horizontal shelf, the indicator should point
  // up (not rotated).
  EXPECT_EQ(0,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in horizontal shelf, the indicator should point
  // down.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(180,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  // When the bubble is not open in left shelf, the indicator should point to
  // the right.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(90,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in left shelf, the indicator should point to the
  // left.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(270,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  // When the bubble is not open in right shelf, the indicator should point to
  // the left.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(270,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());

  // When the bubble is open in right shelf, the indicator should point to the
  // right.
  LeftClickOn(toggle_bubble_button());
  EXPECT_EQ(90,
            video_conference_tray()->GetRotationValueForToggleBubbleButton());
}

TEST_F(VideoConferenceTrayTest, TrayVisibility) {
  // We only show the tray when there is any running media app(s).
  VideoConferenceMediaState state;
  state.has_media_app = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(video_conference_tray()->GetVisible());

  state.has_media_app = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(video_conference_tray()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, CameraButtonVisibility) {
  // Camera icon should only be visible when permission has been granted.
  VideoConferenceMediaState state;
  state.has_camera_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->GetVisible());

  state.has_camera_permission = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, MicrophoneButtonVisibility) {
  // Microphone icon should only be visible when permission has been granted.
  VideoConferenceMediaState state;
  state.has_microphone_permission = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->GetVisible());

  state.has_microphone_permission = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(audio_icon()->GetVisible());
}

TEST_F(VideoConferenceTrayTest, ScreenshareButtonVisibility) {
  auto* screen_share_icon = video_conference_tray()->screen_share_icon();

  VideoConferenceMediaState state;
  state.is_capturing_screen = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(screen_share_icon->GetVisible());
  EXPECT_TRUE(screen_share_icon->show_privacy_indicator());

  state.is_capturing_screen = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(screen_share_icon->GetVisible());
  EXPECT_FALSE(screen_share_icon->show_privacy_indicator());
}

TEST_F(VideoConferenceTrayTest, ToggleCameraButton) {
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(camera_icon()->toggled());

  // Click the button should mute the camera.
  LeftClickOn(camera_icon());
  EXPECT_TRUE(controller()->camera_muted());
  EXPECT_TRUE(camera_icon()->toggled());

  // Toggle again, should be unmuted.
  LeftClickOn(camera_icon());
  EXPECT_FALSE(controller()->camera_muted());
  EXPECT_FALSE(camera_icon()->toggled());
}

TEST_F(VideoConferenceTrayTest, ToggleMicrophoneButton) {
  SetTrayAndButtonsVisible();

  EXPECT_FALSE(audio_icon()->toggled());

  // Click the button should mute the microphone.
  LeftClickOn(audio_icon());
  EXPECT_TRUE(controller()->microphone_muted());
  EXPECT_TRUE(audio_icon()->toggled());

  // Toggle again, should be unmuted.
  LeftClickOn(audio_icon());
  EXPECT_FALSE(controller()->microphone_muted());
  EXPECT_FALSE(audio_icon()->toggled());
}

TEST_F(VideoConferenceTrayTest, PrivacyIndicator) {
  SetTrayAndButtonsVisible();

  // Privacy indicator should be shown when camera is actively capturing video.
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
  VideoConferenceMediaState state;
  state.is_capturing_camera = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(camera_icon()->show_privacy_indicator());

  // Privacy indicator should be shown when microphone is actively capturing
  // audio.
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
  state.is_capturing_microphone = true;
  controller()->UpdateWithMediaState(state);
  EXPECT_TRUE(audio_icon()->show_privacy_indicator());

  // Should not show indicator when not capture.
  state.is_capturing_camera = false;
  state.is_capturing_microphone = false;
  controller()->UpdateWithMediaState(state);
  EXPECT_FALSE(camera_icon()->show_privacy_indicator());
  EXPECT_FALSE(audio_icon()->show_privacy_indicator());
}

}  // namespace ash