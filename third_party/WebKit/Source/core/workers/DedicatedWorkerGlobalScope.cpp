/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/workers/DedicatedWorkerGlobalScope.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/DedicatedWorkerThread.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerThreadStartupData.h"
#include <memory>

namespace blink {

DedicatedWorkerGlobalScope* DedicatedWorkerGlobalScope::create(
    DedicatedWorkerThread* thread,
    std::unique_ptr<WorkerThreadStartupData> startupData,
    double timeOrigin) {
  // Note: startupData is finalized on return. After the relevant parts has been
  // passed along to the created 'context'.
  DedicatedWorkerGlobalScope* context = new DedicatedWorkerGlobalScope(
      startupData->m_scriptURL, startupData->m_userAgent, thread, timeOrigin,
      std::move(startupData->m_starterOriginPrivilegeData),
      startupData->m_workerClients);
  context->applyContentSecurityPolicyFromVector(
      *startupData->m_contentSecurityPolicyHeaders);
  context->setWorkerSettings(std::move(startupData->m_workerSettings));
  if (!startupData->m_referrerPolicy.isNull())
    context->parseAndSetReferrerPolicy(startupData->m_referrerPolicy);
  context->setAddressSpace(startupData->m_addressSpace);
  OriginTrialContext::addTokens(context,
                                startupData->m_originTrialTokens.get());
  return context;
}

DedicatedWorkerGlobalScope::DedicatedWorkerGlobalScope(
    const KURL& url,
    const String& userAgent,
    DedicatedWorkerThread* thread,
    double timeOrigin,
    std::unique_ptr<SecurityOrigin::PrivilegeData> starterOriginPrivilegeData,
    WorkerClients* workerClients)
    : WorkerGlobalScope(url,
                        userAgent,
                        thread,
                        timeOrigin,
                        std::move(starterOriginPrivilegeData),
                        workerClients) {}

DedicatedWorkerGlobalScope::~DedicatedWorkerGlobalScope() {}

const AtomicString& DedicatedWorkerGlobalScope::interfaceName() const {
  return EventTargetNames::DedicatedWorkerGlobalScope;
}

void DedicatedWorkerGlobalScope::postMessage(
    ExecutionContext* context,
    PassRefPtr<SerializedScriptValue> message,
    const MessagePortArray& ports,
    ExceptionState& exceptionState) {
  // Disentangle the port in preparation for sending it to the remote context.
  std::unique_ptr<MessagePortChannelArray> channels =
      MessagePort::disentanglePorts(context, ports, exceptionState);
  if (exceptionState.hadException())
    return;
  if (thread())
  workerObjectProxy().postMessageToWorkerObject(std::move(message),
                                                std::move(channels));
}

InProcessWorkerObjectProxy& DedicatedWorkerGlobalScope::workerObjectProxy()
    const {
  return static_cast<DedicatedWorkerThread*>(thread())->workerObjectProxy();
}

DEFINE_TRACE(DedicatedWorkerGlobalScope) {
  WorkerGlobalScope::trace(visitor);
}

}  // namespace blink
