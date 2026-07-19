// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"

#include "base/check.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "content/public/browser/web_contents.h"

namespace send_tab_to_self {

SendTabToSelfActivationTracker::SendTabToSelfActivationTracker(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SendTabToSelfActivationTracker>(
          *web_contents) {
  // Verify that the tab is not already visible during creation.
  CHECK_NE(web_contents->GetVisibility(), content::Visibility::VISIBLE);
}

SendTabToSelfActivationTracker::~SendTabToSelfActivationTracker() {
  if (!metric_recorded_) {
    RecordActivatedEntryPoint(
        ShareActivatedEntryPoint::kTabOrBrowserClosedWithoutActivation);
  }
}

// static
void SendTabToSelfActivationTracker::SetEntryOpenedViaToast(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
  if (auto* tracker = FromWebContents(web_contents)) {
    tracker->opened_via_toast_ = true;
  }
}

void SendTabToSelfActivationTracker::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    // Only record when the tab becomes visible to the user.
    return;
  }
  metric_recorded_ = true;
  RecordActivatedEntryPoint(opened_via_toast_
                                ? ShareActivatedEntryPoint::kDesktopToast
                                : ShareActivatedEntryPoint::kTabStrip);

  // Self-destruct now that the metric has been recorded.
  // This deletes `this`, so the method must return immediately.
  GetWebContents().RemoveUserData(UserDataKey());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SendTabToSelfActivationTracker);

}  // namespace send_tab_to_self
