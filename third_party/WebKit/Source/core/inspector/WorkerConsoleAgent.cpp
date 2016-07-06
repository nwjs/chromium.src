/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/WorkerConsoleAgent.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerReportingProxy.h"
#include "core/workers/WorkerThread.h"

namespace blink {

WorkerConsoleAgent::WorkerConsoleAgent(V8InspectorSession* v8Session, WorkerGlobalScope* workerGlobalScope)
    : InspectorConsoleAgent(v8Session)
    , m_workerGlobalScope(workerGlobalScope)
{
}

WorkerConsoleAgent::~WorkerConsoleAgent()
{
}

DEFINE_TRACE(WorkerConsoleAgent)
{
    visitor->trace(m_workerGlobalScope);
    InspectorConsoleAgent::trace(visitor);
}

void WorkerConsoleAgent::enable(ErrorString* error)
{
    InspectorConsoleAgent::enable(error);
    m_workerGlobalScope->thread()->workerReportingProxy().postWorkerConsoleAgentEnabled();
}

void WorkerConsoleAgent::clearMessages(ErrorString*)
{
    messageStorage()->clear(m_workerGlobalScope.get());
}

ConsoleMessageStorage* WorkerConsoleAgent::messageStorage()
{
    return m_workerGlobalScope->messageStorage();
}

void WorkerConsoleAgent::enableStackCapturingIfNeeded()
{
    ScriptController::setCaptureCallStackForUncaughtExceptions(m_workerGlobalScope->thread()->isolate(), true);
}

void WorkerConsoleAgent::disableStackCapturingIfNeeded()
{
    ScriptController::setCaptureCallStackForUncaughtExceptions(m_workerGlobalScope->thread()->isolate(), false);
}

} // namespace blink
