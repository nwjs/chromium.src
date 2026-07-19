// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/fcp_http_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/federated_compute/src/fcp/client/http/in_memory_request_response.h"

namespace private_insights {

class FcpHttpClientTest : public testing::Test {
 public:
  FcpHttpClientTest() = default;
  ~FcpHttpClientTest() override = default;
};

TEST_F(FcpHttpClientTest, PerformRequestsEmpty) {
  FcpHttpClient client(nullptr);
  EXPECT_TRUE(client.PerformRequests({}).ok());
}

TEST_F(FcpHttpClientTest, PerformRequestsCanceledRequest) {
  FcpHttpClient client(nullptr);
  auto request =
      fcp::client::http::InMemoryHttpRequest::Create(
          "https://example.com", fcp::client::http::HttpRequest::Method::kGet,
          /*extra_headers=*/{}, /*body=*/"",
          /*use_compression=*/false)
          .value();
  auto handle = client.EnqueueRequest(std::move(request));
  handle->Cancel();
  fcp::client::http::InMemoryHttpRequestCallback callback;

  std::vector<std::pair<fcp::client::http::HttpRequestHandle*,
                        fcp::client::http::HttpRequestCallback*>>
      requests = {{handle.get(), &callback}};

  absl::Status status = client.PerformRequests(requests);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(),
            "HttpRequestHandle was already performed or canceled");
}

TEST_F(FcpHttpClientTest, TotalSentReceivedBytesInitial) {
  FcpHttpClient client(nullptr);
  auto request =
      fcp::client::http::InMemoryHttpRequest::Create(
          "https://example.com", fcp::client::http::HttpRequest::Method::kGet,
          /*extra_headers=*/{}, /*body=*/"",
          /*use_compression=*/false)
          .value();
  auto handle = client.EnqueueRequest(std::move(request));
  auto bytes = handle->TotalSentReceivedBytes();
  EXPECT_EQ(bytes.sent_bytes, 0);
  EXPECT_EQ(bytes.received_bytes, 0);
}

TEST_F(FcpHttpClientTest, TotalSentReceivedBytesUpdate) {
  FcpHttpClient client(nullptr);
  auto request =
      fcp::client::http::InMemoryHttpRequest::Create(
          "https://example.com", fcp::client::http::HttpRequest::Method::kPost,
          /*extra_headers=*/{}, /*body=*/"test data",
          /*use_compression=*/false)
          .value();
  auto handle = client.EnqueueRequest(std::move(request));
  auto* fcp_handle = static_cast<FcpHttpRequestHandle*>(handle.get());

  fcp_handle->sent_bytes()->store(128);
  fcp_handle->received_bytes()->store(1024);

  auto bytes = handle->TotalSentReceivedBytes();
  EXPECT_EQ(bytes.sent_bytes, 128);
  EXPECT_EQ(bytes.received_bytes, 1024);
}

}  // namespace private_insights
