// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/idle/action.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_idle {

// TODO(crbug.com/1316551): Enable this when Android supports >1 Action.
#if !BUILDFLAG(IS_ANDROID)
TEST(IdleActionTest, Build) {
  auto* factory = ActionFactory::GetInstance();

  auto queue = factory->Build(
      {ActionType::kCloseBrowsers, ActionType::kShowProfilePicker});
  EXPECT_EQ(2u, queue.size());
  EXPECT_EQ(0u, queue.top()->priority());
  queue.pop();
  EXPECT_EQ(1u, queue.top()->priority());

  queue = factory->Build({ActionType::kCloseBrowsers});
  EXPECT_EQ(1u, queue.size());
  EXPECT_EQ(0u, queue.top()->priority());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST(IdleActionTest, ClearBrowsingDataIsSingleAction) {
  auto* factory = ActionFactory::GetInstance();

  auto queue = factory->Build(
      {ActionType::kClearBrowsingHistory, ActionType::kClearDownloadHistory,
       ActionType::kClearCookiesAndOtherSiteData,
       ActionType::kClearCachedImagesAndFiles,
       ActionType::kClearCachedImagesAndFiles, ActionType::kClearPasswordSignin,
       ActionType::kClearAutofill, ActionType::kClearSiteSettings,
       ActionType::kClearHostedAppData});
  EXPECT_EQ(1u, queue.size());
  EXPECT_EQ(2u, queue.top()->priority());
}

}  // namespace enterprise_idle
