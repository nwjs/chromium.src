// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEBUG_REPORT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEBUG_REPORT_H_

#include "base/time/time.h"
#include "base/values.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace content {

class AttributionTrigger;
class CreateReportResult;
class StorableSource;

// Class that contains all the data needed to serialize and send an attribution
// debug report.
class CONTENT_EXPORT AttributionDebugReport {
 public:
  static absl::optional<AttributionDebugReport> Create(
      const StorableSource& source,
      bool is_debug_cookie_set,
      const AttributionStorage::StoreSourceResult& result);

  static absl::optional<AttributionDebugReport> Create(
      const AttributionTrigger& trigger,
      bool is_debug_cookie_set,
      const CreateReportResult& result);

  ~AttributionDebugReport();

  AttributionDebugReport(const AttributionDebugReport&) = delete;
  AttributionDebugReport& operator=(const AttributionDebugReport&) = delete;

  AttributionDebugReport(AttributionDebugReport&&);
  AttributionDebugReport& operator=(AttributionDebugReport&&);

  const base::Value::List& ReportBody() const { return report_body_; }

  GURL ReportURL() const;

  // TODO(apaseltiner): This is a workaround to allow the simulator to adjust
  // times while accounting for sub-second precision. Investigate removing it.
  base::Time GetOriginalReportTimeForTesting() const {
    return original_report_time_;
  }

 private:
  AttributionDebugReport(base::Value::List report_body,
                         attribution_reporting::SuitableOrigin reporting_origin,
                         base::Time original_report_time);

  base::Value::List report_body_;
  attribution_reporting::SuitableOrigin reporting_origin_;

  // Only set for report bodies that would include an event-level
  // scheduled_report_time field.
  base::Time original_report_time_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DEBUG_REPORT_H_
