// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/attribution_reporting/source_registration_error.mojom-forward.h"
#include "content/browser/attribution_reporting/attribution_internals.mojom.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace attribution_reporting {
class SuitableOrigin;
}  // namespace attribution_reporting

namespace base {
class Time;
}  // namespace base

namespace content {

class WebUI;

// Implements the mojo endpoint for the attribution internals WebUI which
// proxies calls to the `AttributionManager` to get information about stored
// attribution data. Also observes the manager in order to push events, e.g.
// reports being sent or dropped, to the internals WebUI. Owned by
// `AttributionInternalsUI`.
class AttributionInternalsHandlerImpl
    : public attribution_internals::mojom::Handler,
      public AttributionObserver {
 public:
  AttributionInternalsHandlerImpl(
      WebUI* web_ui,
      mojo::PendingReceiver<attribution_internals::mojom::Handler> receiver);
  AttributionInternalsHandlerImpl(const AttributionInternalsHandlerImpl&) =
      delete;
  AttributionInternalsHandlerImpl& operator=(
      const AttributionInternalsHandlerImpl&) = delete;
  AttributionInternalsHandlerImpl(AttributionInternalsHandlerImpl&&) = delete;
  AttributionInternalsHandlerImpl& operator=(
      AttributionInternalsHandlerImpl&&) = delete;
  ~AttributionInternalsHandlerImpl() override;

  // mojom::AttributionInternalsHandler:
  void IsAttributionReportingEnabled(
      attribution_internals::mojom::Handler::
          IsAttributionReportingEnabledCallback callback) override;
  void GetActiveSources(
      attribution_internals::mojom::Handler::GetActiveSourcesCallback callback)
      override;
  void GetReports(AttributionReport::Type report_type,
                  attribution_internals::mojom::Handler::GetReportsCallback
                      callback) override;
  void SendReports(const std::vector<AttributionReport::Id>& ids,
                   attribution_internals::mojom::Handler::SendReportsCallback
                       callback) override;
  void ClearStorage(attribution_internals::mojom::Handler::ClearStorageCallback
                        callback) override;
  void AddObserver(
      mojo::PendingRemote<attribution_internals::mojom::Observer> observer,
      attribution_internals::mojom::Handler::AddObserverCallback callback)
      override;

 private:
  // AttributionObserver:
  void OnSourcesChanged() override;
  void OnReportsChanged(AttributionReport::Type report_type) override;
  void OnSourceHandled(const StorableSource& source,
                       absl::optional<uint64_t> cleared_debug_key,
                       StorableSource::Result result) override;
  void OnReportSent(const AttributionReport& report,
                    bool is_debug_report,
                    const SendResult& info) override;
  void OnDebugReportSent(const AttributionDebugReport&,
                         int status,
                         base::Time) override;
  void OnTriggerHandled(const AttributionTrigger& trigger,
                        absl::optional<uint64_t> cleared_debug_key,
                        const CreateReportResult& result) override;
  void OnFailedSourceRegistration(
      const std::string& header_value,
      base::Time source_time,
      const attribution_reporting::SuitableOrigin& source_origin,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      attribution_reporting::mojom::SourceRegistrationError) override;

  void OnObserverDisconnected(mojo::RemoteSetElementId);

  raw_ptr<WebUI> web_ui_;

  mojo::Receiver<attribution_internals::mojom::Handler> receiver_;

  mojo::RemoteSet<attribution_internals::mojom::Observer> observers_;

  base::ScopedObservation<AttributionManager, AttributionObserver>
      manager_observation_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_INTERNALS_HANDLER_IMPL_H_
