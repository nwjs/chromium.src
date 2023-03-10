// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
#define CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "content/browser/aggregation_service/aggregatable_report.h"
#include "content/browser/private_aggregation/private_aggregation_budget_key.h"
#include "content/browser/private_aggregation/private_aggregation_budgeter.h"
#include "content/browser/private_aggregation/private_aggregation_host.h"
#include "content/browser/private_aggregation/private_aggregation_manager.h"
#include "content/common/aggregatable_report.mojom-forward.h"
#include "content/common/private_aggregation_host.mojom-forward.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class Time;
}

namespace url {
class Origin;
}

namespace content {

class MockPrivateAggregationBudgeter : public PrivateAggregationBudgeter {
 public:
  MockPrivateAggregationBudgeter();
  ~MockPrivateAggregationBudgeter() override;

  MOCK_METHOD(void,
              ConsumeBudget,
              (int,
               const PrivateAggregationBudgetKey&,
               base::OnceCallback<void(RequestResult)>),
              (override));

  MOCK_METHOD(void,
              ClearData,
              (base::Time,
               base::Time,
               StoragePartition::StorageKeyMatcherFunction,
               base::OnceClosure),
              (override));
};

// Note: the `TestBrowserContext` may require a `BrowserTaskEnvironment` to be
// set up.
class MockPrivateAggregationHost : public PrivateAggregationHost {
 public:
  MockPrivateAggregationHost();
  ~MockPrivateAggregationHost() override;

  MOCK_METHOD(bool,
              BindNewReceiver,
              (url::Origin,
               url::Origin,
               PrivateAggregationBudgetKey::Api,
               mojo::PendingReceiver<mojom::PrivateAggregationHost>),
              (override));

  MOCK_METHOD(void,
              SendHistogramReport,
              (std::vector<mojom::AggregatableReportHistogramContributionPtr>,
               mojom::AggregationServiceMode,
               mojom::DebugModeDetailsPtr),
              (override));

 private:
  TestBrowserContext test_browser_context_;
};

class MockPrivateAggregationManager : public PrivateAggregationManager {
 public:
  MockPrivateAggregationManager();
  ~MockPrivateAggregationManager() override;

  MOCK_METHOD(bool,
              BindNewReceiver,
              (url::Origin,
               url::Origin,
               PrivateAggregationBudgetKey::Api,
               mojo::PendingReceiver<mojom::PrivateAggregationHost>),
              (override));

  MOCK_METHOD(void,
              ClearBudgetData,
              (base::Time,
               base::Time,
               StoragePartition::StorageKeyMatcherFunction,
               base::OnceClosure),
              (override));
};

class MockPrivateAggregationContentBrowserClient
    : public TestContentBrowserClient {
 public:
  MockPrivateAggregationContentBrowserClient();
  ~MockPrivateAggregationContentBrowserClient() override;

  // ContentBrowserClient:
  MOCK_METHOD(bool,
              IsPrivateAggregationAllowed,
              (content::BrowserContext * browser_context,
               const url::Origin& top_frame_origin,
               const url::Origin& reporting_origin),
              (override));
  MOCK_METHOD(void,
              LogWebFeatureForCurrentPage,
              (content::RenderFrameHost*, blink::mojom::WebFeature),
              (override));
};

bool operator==(const PrivateAggregationBudgetKey::TimeWindow&,
                const PrivateAggregationBudgetKey::TimeWindow&);

bool operator==(const PrivateAggregationBudgetKey&,
                const PrivateAggregationBudgetKey&);

}  // namespace content

#endif  // CONTENT_BROWSER_PRIVATE_AGGREGATION_PRIVATE_AGGREGATION_TEST_UTILS_H_
