// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_

#include <memory>
#include <vector>

#include "base/thread_annotations.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/attribution_reporting.h"

namespace content {

struct AttributionConfig;
class CommonSourceInfo;

// Implementation of the storage delegate. This class handles assigning
// report times to newly created reports. It
// also controls constants for AttributionStorage. This is owned by
// AttributionStorageSql, and should only be accessed on the attribution storage
// task runner.
class CONTENT_EXPORT AttributionStorageDelegateImpl
    : public AttributionStorageDelegate {
 public:
  static std::unique_ptr<AttributionStorageDelegate> CreateForTesting(
      AttributionNoiseMode noise_mode,
      AttributionDelayMode delay_mode,
      const AttributionConfig& config);

  explicit AttributionStorageDelegateImpl(
      AttributionNoiseMode noise_mode = AttributionNoiseMode::kDefault,
      AttributionDelayMode delay_mode = AttributionDelayMode::kDefault);
  AttributionStorageDelegateImpl(const AttributionStorageDelegateImpl&) =
      delete;
  AttributionStorageDelegateImpl& operator=(
      const AttributionStorageDelegateImpl&) = delete;
  AttributionStorageDelegateImpl(AttributionStorageDelegateImpl&&) = delete;
  AttributionStorageDelegateImpl& operator=(AttributionStorageDelegateImpl&&) =
      delete;
  ~AttributionStorageDelegateImpl() override;

  // AttributionStorageDelegate:
  base::Time GetEventLevelReportTime(const CommonSourceInfo& source,
                                     base::Time trigger_time) const override;
  base::Time GetAggregatableReportTime(base::Time trigger_time) const override;
  base::TimeDelta GetDeleteExpiredSourcesFrequency() const override;
  base::TimeDelta GetDeleteExpiredRateLimitsFrequency() const override;
  base::GUID NewReportID() const override;
  absl::optional<OfflineReportDelayConfig> GetOfflineReportDelayConfig()
      const override;
  void ShuffleReports(std::vector<AttributionReport>& reports) override;
  RandomizedResponse GetRandomizedResponse(
      const CommonSourceInfo& source) override;

  // Generates fake reports using a random "stars and bars" sequence index of a
  // possible output of the API.
  //
  // Exposed for testing.
  std::vector<FakeReport> GetRandomFakeReports(const CommonSourceInfo& source);

  // Generates fake reports from the "stars and bars" sequence index of a
  // possible output of the API. This output is determined by the following
  // algorithm:
  // 1. Find all stars before the first bar. These stars represent suppressed
  //    reports.
  // 2. For all other stars, count the number of bars that precede them. Each
  //    star represents a report where the reporting window and trigger data is
  //    uniquely determined by that number.
  //
  // Exposed for testing.
  std::vector<FakeReport> GetFakeReportsForSequenceIndex(
      const CommonSourceInfo& source,
      int random_stars_and_bars_sequence_index) const;

 protected:
  AttributionStorageDelegateImpl(AttributionNoiseMode noise_mode,
                                 AttributionDelayMode delay_mode,
                                 const AttributionConfig& config);

  const AttributionNoiseMode noise_mode_ GUARDED_BY_CONTEXT(sequence_checker_);
  const AttributionDelayMode delay_mode_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_STORAGE_DELEGATE_IMPL_H_
