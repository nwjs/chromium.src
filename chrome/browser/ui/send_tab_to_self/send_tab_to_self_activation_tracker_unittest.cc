// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

class SendTabToSelfActivationTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SendTabToSelfActivationTrackerTest() = default;
  ~SendTabToSelfActivationTrackerTest() override = default;
};

TEST_F(SendTabToSelfActivationTrackerTest, RecordDesktopTabStrip) {
  base::HistogramTester histogram_tester;

  content::WebContents* contents = web_contents();
  contents->WasHidden();

  // Attach the activation tracker.
  SendTabToSelfActivationTracker::CreateForWebContents(contents);

  // Show the WebContents to trigger activation recording.
  contents->WasShown();

  // Verify that the Desktop Tab Strip entry point is recorded.
  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ActivatedEntryPoint",
      ShareActivatedEntryPoint::kTabStrip, 1);
}

TEST_F(SendTabToSelfActivationTrackerTest, RecordDesktopToast) {
  base::HistogramTester histogram_tester;

  content::WebContents* contents = web_contents();
  contents->WasHidden();

  // Attach the activation tracker.
  SendTabToSelfActivationTracker::CreateForWebContents(contents);

  // Mark the tab as opened via toast.
  SendTabToSelfActivationTracker::SetEntryOpenedViaToast(contents);

  // Show the WebContents to trigger activation recording.
  contents->WasShown();

  // Verify that the Desktop Toast entry point is recorded.
  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ActivatedEntryPoint",
      ShareActivatedEntryPoint::kDesktopToast, 1);
}

TEST_F(SendTabToSelfActivationTrackerTest, RecordOnlyOnce) {
  base::HistogramTester histogram_tester;

  content::WebContents* contents = web_contents();
  contents->WasHidden();

  // Attach the activation tracker.
  SendTabToSelfActivationTracker::CreateForWebContents(contents);

  // Show, hide, and show again to verify it only records once.
  contents->WasShown();
  contents->WasHidden();
  contents->WasShown();

  // Verify that only a single sample is recorded.
  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ActivatedEntryPoint",
      ShareActivatedEntryPoint::kTabStrip, 1);
}

TEST_F(SendTabToSelfActivationTrackerTest, TabDestroyedWithoutShowing) {
  base::HistogramTester histogram_tester;

  // Create a scoped WebContents that starts hidden.
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  contents->WasHidden();
  // Attach the activation tracker.
  SendTabToSelfActivationTracker::CreateForWebContents(contents.get());

  // Destroy the WebContents without ever showing/activating it.
  contents.reset();
  // Verify that the Tab/Browser Closed without activation entry point is
  // recorded.
  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ActivatedEntryPoint",
      ShareActivatedEntryPoint::kTabOrBrowserClosedWithoutActivation, 1);
}

}  // namespace send_tab_to_self
