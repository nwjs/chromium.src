/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/workers/InProcessWorkerMessagingProxy.h"

#include "third_party/node/src/node_webkit.h"
#define BLINK_HOOK_MAP(type, sym, fn) BLINK_EXPORT type fn = nullptr;
#if defined(COMPONENT_BUILD) && defined(WIN32)
#define NW_HOOK_MAP(type, sym, fn) BASE_EXPORT type fn;
#else
#define NW_HOOK_MAP(type, sym, fn) extern type fn;
#endif
#include "content/nw/src/common/node_hooks.h"
#undef NW_HOOK_MAP

#include "core/dom/Document.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/dom/SecurityContext.h"
#include "core/events/ErrorEvent.h"
#include "core/events/MessageEvent.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoadTiming.h"
#include "core/loader/DocumentLoader.h"
#include "core/origin_trials/OriginTrialContext.h"
#include "core/workers/InProcessWorkerBase.h"
#include "core/workers/InProcessWorkerObjectProxy.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "public/platform/WebTaskRunner.h"
#include "wtf/WTF.h"
#include <memory>

namespace blink {

namespace {

void processUnhandledExceptionOnWorkerGlobalScope(int exceptionId, ExecutionContext* scriptContext)
{
    WorkerGlobalScope* globalScope = toWorkerGlobalScope(scriptContext);
    globalScope->exceptionUnhandled(exceptionId);
}

void processMessageOnWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels, InProcessWorkerObjectProxy* workerObjectProxy, ExecutionContext* scriptContext)
{
    WorkerGlobalScope* globalScope = toWorkerGlobalScope(scriptContext);
    MessagePortArray* ports = MessagePort::entanglePorts(*scriptContext, std::move(channels));
    globalScope->dispatchEvent(MessageEvent::create(ports, message));
    workerObjectProxy->confirmMessageFromWorkerObject();
    workerObjectProxy->startPendingActivityTimer();
}

static int s_liveMessagingProxyCount = 0;

} // namespace

InProcessWorkerMessagingProxy::InProcessWorkerMessagingProxy(InProcessWorkerBase* workerObject, WorkerClients* workerClients)
    : InProcessWorkerMessagingProxy(workerObject->getExecutionContext(), workerObject, workerClients)
{
    DCHECK(m_workerObject);
}

InProcessWorkerMessagingProxy::~InProcessWorkerMessagingProxy()
{
    DCHECK(isParentContextThread());
    DCHECK(!m_workerObject);
    if (m_loaderProxy)
        m_loaderProxy->detachProvider(this);
    s_liveMessagingProxyCount--;
}

int InProcessWorkerMessagingProxy::proxyCount()
{
    DCHECK(isMainThread());
    return s_liveMessagingProxyCount;
}

void InProcessWorkerMessagingProxy::startWorkerGlobalScope(const KURL& scriptURL, const String& userAgent, const String& sourceCode)
{
    DCHECK(isParentContextThread());
    if (m_askedToTerminate) {
        // Worker.terminate() could be called from JS before the thread was
        // created.
        return;
    }

    Document* document = toDocument(getExecutionContext());
    SecurityOrigin* starterOrigin = document->getSecurityOrigin();

    bool isNodeJS = document->frame() && document->frame()->isNodeJS();
    std::string main_script;
    if (g_web_worker_start_thread_fn) {
      (*g_web_worker_start_thread_fn)(document->frame(), (void*)scriptURL.path().utf8().data(), &main_script, &isNodeJS);
    }

    ContentSecurityPolicy* csp = m_workerObject->contentSecurityPolicy() ? m_workerObject->contentSecurityPolicy() : document->contentSecurityPolicy();
    DCHECK(csp);

    WorkerThreadStartMode startMode = m_workerInspectorProxy->workerStartMode(document);
    std::unique_ptr<WorkerSettings> workerSettings = wrapUnique(new WorkerSettings(document->settings()));
    std::unique_ptr<WorkerThreadStartupData> startupData = WorkerThreadStartupData::create(isNodeJS, main_script, scriptURL, userAgent, sourceCode, nullptr, startMode, csp->headers().get(), m_workerObject->referrerPolicy(), starterOrigin, m_workerClients.release(), document->addressSpace(), OriginTrialContext::getTokens(document).get(), std::move(workerSettings));
    double originTime = document->loader() ? document->loader()->timing().referenceMonotonicTime() : monotonicallyIncreasingTime();

    m_loaderProxy = WorkerLoaderProxy::create(this);
    m_workerThread = createWorkerThread(originTime);
    m_workerThread->start(std::move(startupData));
    workerThreadCreated();
    m_workerInspectorProxy->workerThreadCreated(document, m_workerThread.get(), scriptURL);
}

void InProcessWorkerMessagingProxy::postMessageToWorkerObject(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels)
{
    DCHECK(isParentContextThread());
    if (!m_workerObject || m_askedToTerminate)
        return;

    MessagePortArray* ports = MessagePort::entanglePorts(*getExecutionContext(), std::move(channels));
    m_workerObject->dispatchEvent(MessageEvent::create(ports, message));
}

void InProcessWorkerMessagingProxy::postMessageToWorkerGlobalScope(PassRefPtr<SerializedScriptValue> message, std::unique_ptr<MessagePortChannelArray> channels)
{
    DCHECK(isParentContextThread());
    if (m_askedToTerminate)
        return;

    std::unique_ptr<ExecutionContextTask> task = createCrossThreadTask(&processMessageOnWorkerGlobalScope, message, passed(std::move(channels)), crossThreadUnretained(&workerObjectProxy()));
    if (m_workerThread) {
        // A message event is an activity and may initiate another activity.
        m_workerGlobalScopeMayHavePendingActivity = true;
        ++m_unconfirmedMessageCount;
        m_workerThread->postTask(BLINK_FROM_HERE, std::move(task));
    } else {
        m_queuedEarlyTasks.append(std::move(task));
    }
}

void InProcessWorkerMessagingProxy::postTaskToWorkerGlobalScope(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task)
{
    if (m_askedToTerminate)
        return;

    DCHECK(m_workerThread);
    m_workerThread->postTask(location, std::move(task));
}

void InProcessWorkerMessagingProxy::postTaskToLoader(const WebTraceLocation& location, std::unique_ptr<ExecutionContextTask> task)
{
    DCHECK(getExecutionContext()->isDocument());
    // TODO(hiroshige,yuryu): Make this not use ExecutionContextTask and use
    // m_parentFrameTaskRunners->get(TaskType::Networking) instead.
    getExecutionContext()->postTask(location, std::move(task));
}

void InProcessWorkerMessagingProxy::dispatchErrorEvent(const String& errorMessage, std::unique_ptr<SourceLocation> location, int exceptionId)
{
    DCHECK(isParentContextThread());
    if (!m_workerObject)
        return;

    // We don't bother checking the askedToTerminate() flag here, because
    // exceptions should *always* be reported even if the thread is terminated.
    // This is intentionally different than the behavior in MessageWorkerTask,
    // because terminated workers no longer deliver messages (section 4.6 of the
    // WebWorker spec), but they do report exceptions.

    ErrorEvent* event = ErrorEvent::create(errorMessage, location->clone(), nullptr);
    if (m_workerObject->dispatchEvent(event) == DispatchEventResult::NotCanceled)
        postTaskToWorkerGlobalScope(BLINK_FROM_HERE, createCrossThreadTask(&processUnhandledExceptionOnWorkerGlobalScope, exceptionId));
}

void InProcessWorkerMessagingProxy::reportConsoleMessage(MessageSource source, MessageLevel level, const String& message, std::unique_ptr<SourceLocation> location)
{
    DCHECK(isParentContextThread());
    if (m_askedToTerminate)
        return;
    if (m_workerInspectorProxy)
        m_workerInspectorProxy->addConsoleMessageFromWorker(level, message, std::move(location));
}

void InProcessWorkerMessagingProxy::workerThreadCreated()
{
    DCHECK(isParentContextThread());
    DCHECK(!m_askedToTerminate);
    DCHECK(m_workerThread);

    DCHECK(!m_unconfirmedMessageCount);
    m_unconfirmedMessageCount = m_queuedEarlyTasks.size();

    // Worker initialization means a pending activity.
    m_workerGlobalScopeMayHavePendingActivity = true;

    for (auto& earlyTasks : m_queuedEarlyTasks)
        m_workerThread->postTask(BLINK_FROM_HERE, std::move(earlyTasks));
    m_queuedEarlyTasks.clear();
}

InProcessWorkerMessagingProxy::InProcessWorkerMessagingProxy(ExecutionContext* executionContext, InProcessWorkerBase* workerObject, WorkerClients* workerClients)
    : m_executionContext(executionContext)
    , m_workerObjectProxy(InProcessWorkerObjectProxy::create(this))
    , m_workerObject(workerObject)
    , m_mayBeDestroyed(false)
    , m_unconfirmedMessageCount(0)
    , m_workerGlobalScopeMayHavePendingActivity(false)
    , m_askedToTerminate(false)
    , m_workerInspectorProxy(WorkerInspectorProxy::create())
    , m_workerClients(workerClients)
    , m_parentFrameTaskRunners(ParentFrameTaskRunners::create(toDocument(m_executionContext.get())->frame()))
{
    DCHECK(isParentContextThread());
    s_liveMessagingProxyCount++;
}

void InProcessWorkerMessagingProxy::workerObjectDestroyed()
{
    DCHECK(isParentContextThread());

    // workerObjectDestroyed() is called in InProcessWorkerBase's destructor.
    // Thus it should be guaranteed that a weak pointer m_workerObject has been
    // cleared before this method gets called.
    DCHECK(!m_workerObject);

    m_parentFrameTaskRunners->get(TaskType::Internal)->postTask(BLINK_FROM_HERE, WTF::bind(&InProcessWorkerMessagingProxy::workerObjectDestroyedInternal, unretained(this)));
}

void InProcessWorkerMessagingProxy::workerObjectDestroyedInternal()
{
    DCHECK(isParentContextThread());
    m_mayBeDestroyed = true;
    if (m_workerThread)
        terminateWorkerGlobalScope();
    else
        workerThreadTerminated();
}

void InProcessWorkerMessagingProxy::workerThreadTerminated()
{
    DCHECK(isParentContextThread());

    // This method is always the last to be performed, so the proxy is not
    // needed for communication in either side any more. However, the Worker
    // object may still exist, and it assumes that the proxy exists, too.
    m_askedToTerminate = true;
    m_workerThread = nullptr;
    m_workerInspectorProxy->workerThreadTerminated();
    if (m_mayBeDestroyed)
        delete this;
}

void InProcessWorkerMessagingProxy::terminateWorkerGlobalScope()
{
    DCHECK(isParentContextThread());

    if (m_askedToTerminate)
        return;
    m_askedToTerminate = true;

    if (m_workerThread)
        m_workerThread->terminate();

    m_workerInspectorProxy->workerThreadTerminated();
}

void InProcessWorkerMessagingProxy::postMessageToPageInspector(const String& message)
{
    DCHECK(isParentContextThread());
    if (m_workerInspectorProxy)
        m_workerInspectorProxy->dispatchMessageFromWorker(message);
}

void InProcessWorkerMessagingProxy::confirmMessageFromWorkerObject()
{
    DCHECK(isParentContextThread());
    if (m_askedToTerminate)
        return;
    DCHECK(m_unconfirmedMessageCount);
    --m_unconfirmedMessageCount;
}

void InProcessWorkerMessagingProxy::pendingActivityFinished()
{
    DCHECK(isParentContextThread());
    DCHECK(m_workerGlobalScopeMayHavePendingActivity);
    if (m_unconfirmedMessageCount > 0) {
        // Ignore the report because an inflight message event may initiate a
        // new activity.
        return;
    }
    m_workerGlobalScopeMayHavePendingActivity = false;
}

bool InProcessWorkerMessagingProxy::hasPendingActivity() const
{
    DCHECK(isParentContextThread());
    if (m_askedToTerminate)
        return false;
    return m_unconfirmedMessageCount || m_workerGlobalScopeMayHavePendingActivity;
}

bool InProcessWorkerMessagingProxy::isParentContextThread() const
{
    // TODO(nhiroki): Nested worker is not supported yet, so the parent context
    // thread should be equal to the main thread (http://crbug.com/31666).
    DCHECK(getExecutionContext()->isDocument());
    return isMainThread();
}

} // namespace blink
