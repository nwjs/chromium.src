// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_SET_BID_BINDINGS_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_SET_BID_BINDINGS_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

// Class to manage bindings for setting a bidding result. Expected to be used
// for a context managed by ContextRecycler.
class CONTENT_EXPORT SetBidBindings : public Bindings {
 public:
  explicit SetBidBindings(AuctionV8Helper* v8_helper);
  SetBidBindings(const SetBidBindings&) = delete;
  SetBidBindings& operator=(const SetBidBindings&) = delete;
  ~SetBidBindings() override;

  // This must be called before every time this is used.
  // bidder_worklet_non_shared_params->ads.has_value() must be true.
  void ReInitialize(base::TimeTicks start,
                    bool has_top_level_seller_origin,
                    const mojom::BidderWorkletNonSharedParams*
                        bidder_worklet_non_shared_params,
                    bool restrict_to_kanon_ads);

  void FillInGlobalTemplate(
      v8::Local<v8::ObjectTemplate> global_template) override;
  void Reset() override;

  bool has_bid() const { return !bid_.is_null(); }
  mojom::BidderWorkletBidPtr TakeBid();

  // Returns true if there was no error, and false on error. Note that a valid
  // value that results in no bid is not considered an error.
  bool SetBid(v8::Local<v8::Value> generate_bid_result,
              std::string error_prefix,
              std::vector<std::string>& errors_out);

 private:
  static void SetBid(const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;

  base::TimeTicks start_;
  bool has_top_level_seller_origin_ = false;

  raw_ptr<const mojom::BidderWorkletNonSharedParams>
      bidder_worklet_non_shared_params_ = nullptr;
  bool restrict_to_kanon_ads_ = false;

  mojom::BidderWorkletBidPtr bid_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_SET_BID_BINDINGS_H_
