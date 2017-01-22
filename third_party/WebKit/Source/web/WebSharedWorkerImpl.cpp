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

#include "web/WebSharedWorkerImpl.h"

#include "third_party/node/src/node_webkit.h"
#if defined(COMPONENT_BUILD) && defined(WIN32)
#define NW_HOOK_MAP(type, sym, fn) BASE_EXPORT type fn;
#define BLINK_HOOK_MAP(type, sym, fn) BASE_EXPORT type fn;
#else
#define NW_HOOK_MAP(type, sym, fn) extern type fn;
#define BLINK_HOOK_MAP(type, sym, fn) extern type fn;
#endif
#include "content/nw/src/common/node_hooks.h"
#undef NW_HOOK_MAP

#include "core/dom/Document.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/events/MessageEvent.h"
#include "core/html/HTMLFormElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/page/Page.h"
#include "core/workers/ParentFrameTaskRunners.h"
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/SharedWorkerThread.h"
#include "core/workers/WorkerClients.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerInspectorProxy.h"
#include "core/workers/WorkerLoaderProxy.h"
#include "core/workers/WorkerScriptLoader.h"
#include "core/workers/WorkerThreadStartupData.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/network/ContentSecurityPolicyParsers.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFileError.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebDevToolsAgent.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSettings.h"
#include "public/web/WebView.h"
#include "public/web/WebWorkerContentSettingsClientProxy.h"
#include "public/web/modules/serviceworker/WebServiceWorkerNetworkProvider.h"
#include "web/IndexedDBClientImpl.h"
#include "web/LocalFileSystemClient.h"
#include "web/WebDataSourceImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WorkerContentSettingsClient.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

// TODO(toyoshim): Share implementation with WebEmbeddedWorkerImpl as much as
// possible.

WebSharedWorkerImpl::WebSharedWorkerImpl(WebSharedWorkerClient* client)
    : m_webView(nullptr),
      m_mainFrame(nullptr),
      m_askedToTerminate(false),
      m_workerInspectorProxy(WorkerInspectorProxy::create()),
      m_client(client),
      m_pauseWorkerContextOnStart(false),
      m_isPausedOnStart(false),
      m_creationAddressSpace(WebAddressSpacePublic) {}

WebSharedWorkerImpl::~WebSharedWorkerImpl() {
  DCHECK(m_webView);
  // Detach the client before closing the view to avoid getting called back.
  m_mainFrame->setClient(0);

  m_webView->close();
  m_mainFrame->close();
  if (m_loaderProxy)
    m_loaderProxy->detachProvider(this);
}

void WebSharedWorkerImpl::terminateWorkerThread() {
  if (m_askedToTerminate)
    return;
  m_askedToTerminate = true;
  if (m_mainScriptLoader) {
    m_mainScriptLoader->cancel();
    m_mainScriptLoader.clear();
    m_client->workerScriptLoadFailed();
    delete this;
    return;
  }
  if (m_workerThread)
    m_workerThread->terminate();
  m_workerInspectorProxy->workerThreadTerminated();
}

void WebSharedWorkerImpl::initializeLoader() {
  // Create 'shadow page'. This page is never displayed, it is used to proxy the
  // loading requests from the worker context to the rest of WebKit and Chromium
  // infrastructure.
  DCHECK(!m_webView);
  m_webView = WebView::create(nullptr, WebPageVisibilityStateVisible);
  // FIXME: http://crbug.com/363843. This needs to find a better way to
  // not create graphics layers.
  m_webView->settings()->setAcceleratedCompositingEnabled(false);
  // FIXME: Settings information should be passed to the Worker process from
  // Browser process when the worker is created (similar to
  // RenderThread::OnCreateNewView).
  m_mainFrame = toWebLocalFrameImpl(
      WebLocalFrame::create(WebTreeScopeType::Document, this));
  m_webView->setMainFrame(m_mainFrame.get());
  m_mainFrame->setDevToolsAgentClient(this);

  // If we were asked to pause worker context on start and wait for debugger
  // then it is the good time to do that.
  m_client->workerReadyForInspection();
  if (m_pauseWorkerContextOnStart) {
    m_isPausedOnStart = true;
    return;
  }
  loadShadowPage();
}

WebApplicationCacheHost* WebSharedWorkerImpl::createApplicationCacheHost(
    WebApplicationCacheHostClient* appcacheHostClient) {
  return m_client->createApplicationCacheHost(appcacheHostClient);
}

void WebSharedWorkerImpl::loadShadowPage() {
  // Construct substitute data source for the 'shadow page'. We only need it
  // to have same origin as the worker so the loading checks work correctly.
  CString content("");
  RefPtr<SharedBuffer> buffer(
      SharedBuffer::create(content.data(), content.length()));
  m_mainFrame->frame()->loader().load(
      FrameLoadRequest(0, ResourceRequest(m_url),
                       SubstituteData(buffer, "text/html", "UTF-8", KURL())));
}

void WebSharedWorkerImpl::willSendRequest(WebLocalFrame* frame,
                                          WebURLRequest& request) {
  if (m_networkProvider)
    m_networkProvider->willSendRequest(frame->dataSource(), request);
}

void WebSharedWorkerImpl::didFinishDocumentLoad(WebLocalFrame* frame) {
  DCHECK(!m_loadingDocument);
  DCHECK(!m_mainScriptLoader);
  m_networkProvider = wrapUnique(
      m_client->createServiceWorkerNetworkProvider(frame->dataSource()));
  m_mainScriptLoader = WorkerScriptLoader::create();
  m_mainScriptLoader->setRequestContext(
      WebURLRequest::RequestContextSharedWorker);
  m_loadingDocument = toWebLocalFrameImpl(frame)->frame()->document();
  m_mainScriptLoader->loadAsynchronously(
      *m_loadingDocument.get(), m_url, DenyCrossOriginRequests,
      m_creationAddressSpace,
      bind(&WebSharedWorkerImpl::didReceiveScriptLoaderResponse,
           WTF::unretained(this)),
      bind(&WebSharedWorkerImpl::onScriptLoaderFinished,
           WTF::unretained(this)));
  // Do nothing here since onScriptLoaderFinished() might have been already
  // invoked and |this| might have been deleted at this point.
}

bool WebSharedWorkerImpl::isControlledByServiceWorker(
    WebDataSource& dataSource) {
  return m_networkProvider &&
         m_networkProvider->isControlledByServiceWorker(dataSource);
}

int64_t WebSharedWorkerImpl::serviceWorkerID(WebDataSource& dataSource) {
  if (!m_networkProvider)
    return -1;
  return m_networkProvider->serviceWorkerID(dataSource);
}

InterfaceProvider* WebSharedWorkerImpl::interfaceProvider() {
  return Platform::current()->interfaceProvider();
}

void WebSharedWorkerImpl::sendProtocolMessage(int sessionId,
                                              int callId,
                                              const WebString& message,
                                              const WebString& state) {
  m_client->sendDevToolsMessage(sessionId, callId, message, state);
}

void WebSharedWorkerImpl::resumeStartup() {
  bool isPausedOnStart = m_isPausedOnStart;
  m_isPausedOnStart = false;
  if (isPausedOnStart)
    loadShadowPage();
}

WebDevToolsAgentClient::WebKitClientMessageLoop*
WebSharedWorkerImpl::createClientMessageLoop() {
  return m_client->createDevToolsMessageLoop();
}

// WorkerReportingProxy --------------------------------------------------------

void WebSharedWorkerImpl::reportException(const String& errorMessage,
                                          std::unique_ptr<SourceLocation>,
                                          int exceptionId) {
  // Not suppported in SharedWorker.
}

void WebSharedWorkerImpl::reportConsoleMessage(MessageSource,
                                               MessageLevel,
                                               const String& message,
                                               SourceLocation*) {
  // Not supported in SharedWorker.
}

void WebSharedWorkerImpl::postMessageToPageInspector(const String& message) {
  m_mainFrame->frame()->document()->postInspectorTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &WebSharedWorkerImpl::postMessageToPageInspectorOnMainThread,
          crossThreadUnretained(this), message));
}

void WebSharedWorkerImpl::postMessageToPageInspectorOnMainThread(
    const String& message) {
  m_workerInspectorProxy->dispatchMessageFromWorker(message);
}

void WebSharedWorkerImpl::didCloseWorkerGlobalScope() {
  Platform::current()->mainThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(
          &WebSharedWorkerImpl::didCloseWorkerGlobalScopeOnMainThread,
          crossThreadUnretained(this)));
}

void WebSharedWorkerImpl::didCloseWorkerGlobalScopeOnMainThread() {
  m_client->workerContextClosed();

  terminateWorkerThread();
}

void WebSharedWorkerImpl::didTerminateWorkerThread() {
  Platform::current()->mainThread()->getWebTaskRunner()->postTask(
      BLINK_FROM_HERE,
      crossThreadBind(
          &WebSharedWorkerImpl::didTerminateWorkerThreadOnMainThread,
          crossThreadUnretained(this)));
}

void WebSharedWorkerImpl::didTerminateWorkerThreadOnMainThread() {
  m_client->workerContextDestroyed();
  // The lifetime of this proxy is controlled by the worker context.
  delete this;
}

// WorkerLoaderProxyProvider -------------------------------------------------

void WebSharedWorkerImpl::postTaskToLoader(
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task) {
  // TODO(hiroshige,yuryu): Make this not use ExecutionContextTask and
  // consider using m_mainThreadTaskRunners->get(TaskType::Networking)
  // instead.
  m_mainFrame->frame()->document()->postTask(location, std::move(task));
}

void WebSharedWorkerImpl::postTaskToWorkerGlobalScope(
    const WebTraceLocation& location,
    std::unique_ptr<ExecutionContextTask> task) {
  m_workerThread->postTask(location, std::move(task));
}

void WebSharedWorkerImpl::connect(WebMessagePortChannel* webChannel) {
  workerThread()->postTask(
      BLINK_FROM_HERE,
      createCrossThreadTask(
          &connectTask, passed(WebMessagePortChannelUniquePtr(webChannel))));
}

void WebSharedWorkerImpl::connectTask(WebMessagePortChannelUniquePtr channel,
                                      ExecutionContext* context) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  MessagePort* port = MessagePort::create(*context);
  port->entangle(std::move(channel));
  WorkerGlobalScope* workerGlobalScope = toWorkerGlobalScope(context);
  SECURITY_DCHECK(workerGlobalScope->isSharedWorkerGlobalScope());
  workerGlobalScope->dispatchEvent(createConnectEvent(port));
}

void WebSharedWorkerImpl::startWorkerContext(bool nodejs,
                                             const base::FilePath& root_path,
    const WebURL& url,
    const WebString& name,
    const WebString& contentSecurityPolicy,
    WebContentSecurityPolicyType policyType,
    WebAddressSpace creationAddressSpace) {
  m_nodejs = nodejs;
  m_root_path = root_path;
  m_url = url;
  m_name = name;
  m_creationAddressSpace = creationAddressSpace;
  initializeLoader();
}

void WebSharedWorkerImpl::didReceiveScriptLoaderResponse() {
  InspectorInstrumentation::didReceiveScriptResponse(
      m_loadingDocument.get(), m_mainScriptLoader->identifier());
  m_client->selectAppCacheID(m_mainScriptLoader->appCacheID());
}

void WebSharedWorkerImpl::onScriptLoaderFinished() {
  DCHECK(m_loadingDocument);
  DCHECK(m_mainScriptLoader);
  if (m_askedToTerminate)
    return;
  if (m_mainScriptLoader->failed()) {
    m_mainScriptLoader->cancel();
    m_client->workerScriptLoadFailed();

    // The SharedWorker was unable to load the initial script, so
    // shut it down right here.
    delete this;
    return;
  }

  Document* document = m_mainFrame->frame()->document();
  // FIXME: this document's origin is pristine and without any extra privileges.
  // (crbug.com/254993)
  SecurityOrigin* starterOrigin = document->getSecurityOrigin();
  std::string main_script = m_root_path.AsUTF8Unsafe();
  if (g_web_worker_start_thread_fn && m_nodejs) {
    (*g_web_worker_start_thread_fn)(nullptr, (void*)m_mainScriptLoader->url().path().utf8().data(), &main_script, &m_nodejs);
  }

  WorkerClients* workerClients = WorkerClients::create();
  provideLocalFileSystemToWorker(workerClients,
                                 LocalFileSystemClient::create());
  WebSecurityOrigin webSecurityOrigin(m_loadingDocument->getSecurityOrigin());
  provideContentSettingsClientToWorker(
      workerClients,
      wrapUnique(
          m_client->createWorkerContentSettingsClientProxy(webSecurityOrigin)));
  provideIndexedDBClientToWorker(workerClients, IndexedDBClientImpl::create());
  ContentSecurityPolicy* contentSecurityPolicy =
      m_mainScriptLoader->releaseContentSecurityPolicy();
  WorkerThreadStartMode startMode =
      m_workerInspectorProxy->workerStartMode(document);
  std::unique_ptr<WorkerSettings> workerSettings =
      wrapUnique(new WorkerSettings(document->settings()));
  std::unique_ptr<WorkerThreadStartupData> startupData =
    WorkerThreadStartupData::create(m_nodejs, main_script,
          m_url, m_loadingDocument->userAgent(), m_mainScriptLoader->script(),
          nullptr, startMode,
          contentSecurityPolicy ? contentSecurityPolicy->headers().get()
                                : nullptr,
          m_mainScriptLoader->referrerPolicy(), starterOrigin, workerClients,
          m_mainScriptLoader->responseAddressSpace(),
          m_mainScriptLoader->originTrialTokens(), std::move(workerSettings));

  // We have a dummy document here for loading but it doesn't really represent
  // the document/frame of associated document(s) for this worker. Here we
  // populate the task runners with null document not to confuse the frame
  // scheduler (which will end up using the thread's default task runner).
  m_mainThreadTaskRunners = ParentFrameTaskRunners::create(nullptr);

  m_loaderProxy = WorkerLoaderProxy::create(this);
  m_workerThread = SharedWorkerThread::create(m_name, m_loaderProxy, *this);
  InspectorInstrumentation::scriptImported(m_loadingDocument.get(),
                                           m_mainScriptLoader->identifier(),
                                           m_mainScriptLoader->script());
  m_mainScriptLoader.clear();

  workerThread()->start(std::move(startupData));
  m_workerInspectorProxy->workerThreadCreated(
      toDocument(m_loadingDocument.get()), workerThread(), m_url);
  m_client->workerScriptLoaded();
}

void WebSharedWorkerImpl::terminateWorkerContext() {
  terminateWorkerThread();
}

void WebSharedWorkerImpl::pauseWorkerContextOnStart() {
  m_pauseWorkerContextOnStart = true;
}

void WebSharedWorkerImpl::attachDevTools(const WebString& hostId,
                                         int sessionId) {
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->attach(hostId, sessionId);
}

void WebSharedWorkerImpl::reattachDevTools(const WebString& hostId,
                                           int sessionId,
                                           const WebString& savedState) {
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->reattach(hostId, sessionId, savedState);
  resumeStartup();
}

void WebSharedWorkerImpl::detachDevTools() {
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->detach();
}

void WebSharedWorkerImpl::dispatchDevToolsMessage(int sessionId,
                                                  int callId,
                                                  const WebString& method,
                                                  const WebString& message) {
  if (m_askedToTerminate)
    return;
  WebDevToolsAgent* devtoolsAgent = m_mainFrame->devToolsAgent();
  if (devtoolsAgent)
    devtoolsAgent->dispatchOnInspectorBackend(sessionId, callId, method,
                                              message);
}

WebSharedWorker* WebSharedWorker::create(WebSharedWorkerClient* client) {
  return new WebSharedWorkerImpl(client);
}

}  // namespace blink
