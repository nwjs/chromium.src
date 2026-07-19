// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ACTIVATION_TRACKER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ACTIVATION_TRACKER_H_

#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace send_tab_to_self {

// Observes a WebContents that was opened in the background by Send Tab to Self.
// When the tab is first shown (made visible/active), it records whether the
// user navigated to it via the desktop toast notification or manually via the
// tab strip.
// TODO(crbug.com/503283050): Look into persisting the activation tracker state
// across restarts to avoid losing metrics.
class SendTabToSelfActivationTracker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SendTabToSelfActivationTracker> {
 public:
  SendTabToSelfActivationTracker(const SendTabToSelfActivationTracker&) =
      delete;
  SendTabToSelfActivationTracker& operator=(
      const SendTabToSelfActivationTracker&) = delete;

  ~SendTabToSelfActivationTracker() override;

  // Static helper to mark that the tab associated with `web_contents` was
  // activated specifically by clicking the desktop toast notification.
  // Safe to call with null.
  static void SetEntryOpenedViaToast(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;

 private:
  friend class content::WebContentsUserData<SendTabToSelfActivationTracker>;
  explicit SendTabToSelfActivationTracker(content::WebContents* web_contents);

  // True if the tab was activated by clicking the desktop toast notification.
  bool opened_via_toast_ = false;

  // True if the metric has been recorded.
  bool metric_recorded_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ACTIVATION_TRACKER_H_
