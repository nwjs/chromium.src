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

#include "public/web/WebKit.h"

#include <memory>

#include "third_party/node-nw/src/node_webkit.h"
#if defined(COMPONENT_BUILD) && defined(WIN32)
#define NW_HOOK_MAP(type, sym, fn) BASE_EXPORT type fn;
#define BLINK_HOOK_MAP(type, sym, fn) CORE_EXPORT type fn;
#else
#define NW_HOOK_MAP(type, sym, fn) extern type fn;
#define BLINK_HOOK_MAP(type, sym, fn) extern type fn;
#endif
#include "content/nw/src/common/node_hooks.h"
#undef NW_HOOK_MAP

#include "modules/gamepad/NavigatorGamepad.h"
#include "public/web/WebFrame.h"
#include "public/web/WebDocument.h"
#include "core/dom/Document.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/layout/LayoutTheme.h"
#include "core/page/Page.h"
#include "core/workers/WorkerBackingThread.h"
#include "gin/public/v8_platform.h"
#include "platform/LayoutTestSupport.h"
#include "platform/heap/Heap.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/WTF.h"
#include "platform/wtf/allocator/Partitions.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/TextEncoding.h"
#include "public/web/WebLocalFrame.h"

namespace blink {

v8::Isolate* MainThreadIsolate() {
  return V8PerIsolateData::MainThreadIsolate();
}

// TODO(tkent): The following functions to wrap LayoutTestSupport should be
// moved to public/platform/.

void SetLayoutTestMode(bool value) {
  LayoutTestSupport::SetIsRunningLayoutTest(value);
}

bool LayoutTestMode() {
  return LayoutTestSupport::IsRunningLayoutTest();
}

void set_web_worker_hooks(void* fn_start) {
  g_web_worker_start_thread_fn = (VoidPtr4Fn)fn_start;
}

void SetMockThemeEnabledForTest(bool value) {
  LayoutTestSupport::SetMockThemeEnabledForTest(value);
  LayoutTheme::GetTheme().DidChangeThemeEngine();
}

void fix_gamepad_nw(WebLocalFrame* frame)
{
  Document* doc = frame->GetDocument();
  NavigatorGamepad* gamepad = NavigatorGamepad::From(*doc);
  ((ContextLifecycleObserver*)gamepad)->SetContext(static_cast<ExecutionContext*>(doc));
  gamepad->Gamepads();
}

void SetFontAntialiasingEnabledForTest(bool value) {
  LayoutTestSupport::SetFontAntialiasingEnabledForTest(value);
}

bool FontAntialiasingEnabledForTest() {
  return LayoutTestSupport::IsFontAntialiasingEnabledForTest();
}

void ResetPluginCache(bool reload_pages) {
  DCHECK(!reload_pages);
  Page::RefreshPlugins();
  Page::ResetPluginData();
}

void DecommitFreeableMemory() {
  WTF::Partitions::DecommitFreeableMemory();
}

void MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level) {
  WorkerBackingThread::MemoryPressureNotificationToWorkerThreadIsolates(level);
}

void SetRAILModeOnWorkerThreadIsolates(v8::RAILMode rail_mode) {
  WorkerBackingThread::SetRAILModeOnWorkerThreadIsolates(rail_mode);
}

void LogRuntimeCallStats() {
  LOG(INFO)
      << "\n"
      << RuntimeCallStats::From(MainThreadIsolate())->ToString().Utf8().data();
}

}  // namespace blink
