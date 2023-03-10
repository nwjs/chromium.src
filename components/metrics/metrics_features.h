// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_FEATURES_H_
#define COMPONENTS_METRICS_METRICS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace metrics::features {
// Determines whether histograms that that are expected to be set on every log
// should be emitted in OnDidCreateMetricsLog() instead of
// ProvideCurrentSessionData().
BASE_DECLARE_FEATURE(kEmitHistogramsEarlier);

// If set, histograms that are expected to be set on every log will be emitted
// in DisableRecording().
extern const base::FeatureParam<bool> kEmitHistogramsForIndependentLogs;

// Determines whether the metrics service should create periodic logs
// asynchronously.
BASE_DECLARE_FEATURE(kMetricsServiceAsyncCollection);

// Determines at what point the metrics service is allowed to close a log when
// Chrome is closed (and backgrounded/foregrounded for mobile platforms). When
// this feature is disabled, the metrics service can only close a log if it has
// already started sending logs. When this feature is enabled, the metrics
// service can close a log starting from when the first log is opened.
BASE_DECLARE_FEATURE(kMetricsServiceAllowEarlyLogClose);

// Determines whether logs stored in Local State are cleared when the Chrome
// install is detected as cloned.
BASE_DECLARE_FEATURE(kMetricsClearLogsOnClonedInstall);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// Determines whether we immediately flush Local State after uploading a log
// while Chrome is in the background. Only applicable for mobile platforms.
BASE_DECLARE_FEATURE(kReportingServiceFlushPrefsOnUploadInBackground);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}  // namespace metrics::features

#endif  // COMPONENTS_METRICS_METRICS_FEATURES_H_