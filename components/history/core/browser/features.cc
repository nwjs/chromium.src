// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/features.h"

#include "build/build_config.h"
#include "components/history/core/browser/top_sites_impl.h"

namespace history {
namespace {
constexpr auto kOrganicRepeatableQueriesDefaultValue =
    BUILDFLAG(IS_ANDROID) ? base::FEATURE_ENABLED_BY_DEFAULT
                          : base::FEATURE_DISABLED_BY_DEFAULT;

// Specifies the scaling behavior, i.e. whether the relevance scales of the
// top sites and repeatable queries should be first aligned.
// The default behavior is to mix the two lists as is.
constexpr bool kScaleRepeatableQueriesScoresDefaultValue =
    BUILDFLAG(IS_ANDROID) ? true : false;

// Defines the maximum number of repeatable queries that can be shown.
// The default behavior is having no limit, i.e., the number of the tiles.
constexpr int kMaxNumRepeatableQueriesDefaultValue =
    BUILDFLAG(IS_ANDROID) ? 4 : kTopSitesNumber;
}  // namespace

// If enabled, the most repeated queries from the user browsing history are
// shown in the Most Visited tiles.
BASE_FEATURE(kOrganicRepeatableQueries,
             "OrganicRepeatableQueries",
             kOrganicRepeatableQueriesDefaultValue);

// The maximum number of repeatable queries to show in the Most Visited tiles.
const base::FeatureParam<int> kMaxNumRepeatableQueries(
    &kOrganicRepeatableQueries,
    "MaxNumRepeatableQueries",
    kMaxNumRepeatableQueriesDefaultValue);

// Whether the scores for the repeatable queries and the most visited sites
// should first be scaled to an equivalent range before mixing.
const base::FeatureParam<bool> kScaleRepeatableQueriesScores(
    &kOrganicRepeatableQueries,
    "ScaleRepeatableQueriesScores",
    kScaleRepeatableQueriesScoresDefaultValue);

// Whether a repeatable query should precede a most visited site with equal
// score. The default behavior is for the sites to precede the queries.
// Used for tie-breaking, especially when kScaleRepeatableQueriesScores is true.
const base::FeatureParam<bool> kPrivilegeRepeatableQueries(
    &kOrganicRepeatableQueries,
    "PrivilegeRepeatableQueries",
    false);

}  // namespace history
