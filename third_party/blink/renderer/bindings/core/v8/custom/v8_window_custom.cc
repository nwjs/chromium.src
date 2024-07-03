/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_html_collection.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/bindings/v8_set_return_value.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"


#include "third_party/blink/renderer/bindings/core/v8/v8_html_frame_element.h"

namespace blink {

template <typename CallbackInfo>
static void ParentAttributeGet(const CallbackInfo& info)
{
  v8::Local<v8::Object> v8_win = info.This();
  DOMWindow* blink_win = V8Window::ToWrappableUnsafe(info.GetIsolate(), v8_win);
  const char* const property_name = "parent";
  blink_win->ReportCoopAccess(property_name);
  DOMWindow* return_value = blink_win->parent();
  if (blink_win->IsLocalDOMWindow()) {
    LocalDOMWindow* imp = To<LocalDOMWindow>(V8Window::ToWrappableUnsafe(info.GetIsolate(), info.This()));
    LocalFrame* frame = imp->GetFrame();
    if (frame && frame->isNwFakeTop()) {
      V8SetReturnValue(info, imp, blink_win, bindings::V8ReturnValue::kMaybeCrossOrigin);
      return;
    }
    V8SetReturnValue(info, imp->parent(), blink_win, bindings::V8ReturnValue::kMaybeCrossOrigin);
  } else {
    V8SetReturnValue(info, return_value, blink_win,
                     bindings::V8ReturnValue::kMaybeCrossOrigin);
  }
}

template <typename CallbackInfo>
static void TopAttributeGet(const CallbackInfo& info)
{
  v8::Local<v8::Object> v8_win = info.This();
  DOMWindow* blink_win = V8Window::ToWrappableUnsafe(info.GetIsolate(), v8_win);
  const char* const property_name = "top";
  blink_win->ReportCoopAccess(property_name);
  DOMWindow* return_value = blink_win->top();
  if (blink_win->IsLocalDOMWindow()) {
    LocalDOMWindow* imp = To<LocalDOMWindow>(V8Window::ToWrappableUnsafe(info.This()));
    LocalFrame* frame = imp->GetFrame();
    if (frame) {
      for (LocalFrame* f = frame; f; ) {
        if (f->isNwFakeTop()) {
          V8SetReturnValue(info, ToV8(f->GetDocument()->domWindow(), info.This(), info.GetIsolate()));
          return;
        }
        Frame* fr = f->Tree().Parent();
        if (!fr || !fr->IsLocalFrame())
          break;
        f = DynamicTo<LocalFrame>(fr);
      }
    }
    V8SetReturnValue(info, ToV8(imp->top(), info.This(), info.GetIsolate()));
  } else {
    V8SetReturnValue(info, return_value, blink_win,
                     bindings::V8ReturnValue::kMaybeCrossOrigin);
  }
}

void V8Window::ParentAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ParentAttributeGet(info);
}

void V8Window::ParentAttributeGetterCustom(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ParentAttributeGet(info);
}

void V8Window::TopAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ParentAttributeGet(info);
}

void V8Window::TopAttributeGetterCustom(
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ParentAttributeGet(info);
}

}  // namespace blink
