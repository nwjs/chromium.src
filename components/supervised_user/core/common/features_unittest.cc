// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/features.h"

#include "base/dcheck_is_on.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_users {

// Tests supervised user features configurations.
class FeaturesTest : public testing::Test {
 protected:
  FeaturesTest() = default;
  FeaturesTest(const FeaturesTest&) = delete;
  FeaturesTest& operator=(const FeaturesTest&) = delete;
  ~FeaturesTest() override = default;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests `kWebFilterInterstitialRefresh` and `kLocalWebApproval`features
// configuration.
using LocalWebApprovalsFeatureTest = FeaturesTest;

TEST_F(LocalWebApprovalsFeatureTest,
       InterstitialRefreshDisabledAndLocalApprovalsDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {},
      /* disabled_features */ {kWebFilterInterstitialRefresh,
                               kLocalWebApprovals});
  EXPECT_FALSE(IsWebFilterInterstitialRefreshEnabled());
  EXPECT_FALSE(IsLocalWebApprovalsEnabled());
}

TEST_F(LocalWebApprovalsFeatureTest,
       InterstitialRefreshEnabledAndLocalApprovalsEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {kWebFilterInterstitialRefresh,
                              kLocalWebApprovals},
      /* disabled_features */ {});
  EXPECT_TRUE(IsWebFilterInterstitialRefreshEnabled());
  EXPECT_TRUE(IsLocalWebApprovalsEnabled());
}

TEST_F(LocalWebApprovalsFeatureTest,
       InterstitialRefreshEnabledAndLocalApprovalsDisabled) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {kWebFilterInterstitialRefresh},
      /* disabled_features */ {kLocalWebApprovals});
  EXPECT_TRUE(IsWebFilterInterstitialRefreshEnabled());
  EXPECT_FALSE(IsLocalWebApprovalsEnabled());
}

// Tests that DCHECK is triggered when local web approval feature is enabled
// without the refreshed web filter interstitial layout feature.
#if DCHECK_IS_ON()
TEST_F(LocalWebApprovalsFeatureTest,
       InterstitialRefreshDisableAndLocalApprovalsEnabled) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {kLocalWebApprovals},
      /* disabled_features */ {kWebFilterInterstitialRefresh});
  EXPECT_DEATH_IF_SUPPORTED(IsWebFilterInterstitialRefreshEnabled(), "");
  EXPECT_DEATH_IF_SUPPORTED(IsLocalWebApprovalsEnabled(), "");
}
#endif

}  // namespace supervised_users
