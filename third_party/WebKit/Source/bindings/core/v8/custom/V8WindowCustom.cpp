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

#include "bindings/core/v8/V8Window.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "bindings/core/v8/SerializedScriptValueFactory.h"
#include "bindings/core/v8/Transferables.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8EventListener.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8Node.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/MessagePort.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameOwner.h"
#include "core/frame/FrameView.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/LayoutTestSupport.h"
#include "wtf/Assertions.h"


#include "bindings/core/v8/V8HTMLFrameElement.h"

namespace blink {

void V8Window::eventAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = toLocalDOMWindow(V8Window::toImpl(info.Holder()));
  ExceptionState exceptionState(
      info.GetIsolate(), ExceptionState::GetterContext, "Window", "event");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()),
                                            impl, exceptionState)) {
    return;
  }

  LocalFrame* frame = impl->frame();
  ASSERT(frame);
  // This is a fast path to retrieve info.Holder()->CreationContext().
  v8::Local<v8::Context> context =
      toV8Context(frame, DOMWrapperWorld::current(info.GetIsolate()));
  if (context.IsEmpty())
    return;

  v8::Local<v8::Value> jsEvent = V8HiddenValue::getHiddenValue(
      ScriptState::current(info.GetIsolate()), context->Global(),
      V8HiddenValue::event(info.GetIsolate()));
  if (jsEvent.IsEmpty())
    return;
  v8SetReturnValue(info, jsEvent);
}

void V8Window::parentAttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
  LocalDOMWindow* imp = toLocalDOMWindow(V8Window::toImpl(info.Holder()));
  LocalFrame* frame = imp->frame();
  if (frame && frame->isNwFakeTop()) {
    v8SetReturnValue(info, ToV8(imp, info.Holder(), info.GetIsolate()));
    return;
  }
  v8SetReturnValue(info, ToV8(imp->parent(), info.Holder(), info.GetIsolate()));
}

void V8Window::topAttributeGetterCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
  LocalDOMWindow* imp = toLocalDOMWindow(V8Window::toImpl(info.Holder()));
  LocalFrame* frame = imp->frame();
  if (frame) {
    for (LocalFrame* f = frame; f; f = toLocalFrame(f->tree().parent())) {
      if (f->isNwFakeTop()) {
        v8SetReturnValue(info, ToV8(f->document()->domWindow(), info.Holder(), info.GetIsolate()));
        return;
      }
    }
  }
  v8SetReturnValue(info, ToV8(imp->top(), info.Holder(), info.GetIsolate()));
}

void V8Window::eventAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = toLocalDOMWindow(V8Window::toImpl(info.Holder()));
  LocalFrame* frame = impl->frame();
  if (frame && frame->isNwFakeTop())
    return;
  ExceptionState exceptionState(
      info.GetIsolate(), ExceptionState::SetterContext, "Window", "event");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()),
                                            impl, exceptionState)) {
    return;
  }

  ASSERT(frame);
  // This is a fast path to retrieve info.Holder()->CreationContext().
  v8::Local<v8::Context> context =
      toV8Context(frame, DOMWrapperWorld::current(info.GetIsolate()));
  if (context.IsEmpty())
    return;

  V8HiddenValue::setHiddenValue(ScriptState::current(info.GetIsolate()),
                                context->Global(),
                                V8HiddenValue::event(info.GetIsolate()), value);
}

void V8Window::frameElementAttributeGetterCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  LocalDOMWindow* impl = toLocalDOMWindow(V8Window::toImpl(info.Holder()));

  LocalFrame* frame = impl->frame();
  if (frame && frame->isNwFakeTop()) {
    v8SetReturnValueNull(info);
    return;
  }

  if (!BindingSecurity::shouldAllowAccessTo(
          currentDOMWindow(info.GetIsolate()), impl->frameElement(),
          BindingSecurity::ErrorReportOption::DoNotReport)) {
    v8SetReturnValueNull(info);
    return;
  }

  // The wrapper for an <iframe> should get its prototype from the context of
  // the frame it's in, rather than its own frame.
  // So, use its containing document as the creation context when wrapping.
  v8::Local<v8::Value> creationContext =
      ToV8(&impl->frameElement()->document(), info.Holder(), info.GetIsolate());
  RELEASE_ASSERT(!creationContext.IsEmpty());
  v8::Local<v8::Value> wrapper =
      ToV8(impl->frameElement(), v8::Local<v8::Object>::Cast(creationContext),
           info.GetIsolate());
  v8SetReturnValue(info, wrapper);
}

void V8Window::openerAttributeSetterCustom(
    v8::Local<v8::Value> value,
    const v8::PropertyCallbackInfo<void>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  DOMWindow* impl = V8Window::toImpl(info.Holder());
  // TODO(dcheng): Investigate removing this, since opener is not really a
  // cross-origin property (so it shouldn't be accessible to begin with)
  ExceptionState exceptionState(isolate, ExceptionState::SetterContext,
                                "Window", "opener");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()),
                                            impl, exceptionState)) {
    return;
  }

  // Opener can be shadowed if it is in the same domain.
  // Have a special handling of null value to behave
  // like Firefox. See bug http://b/1224887 & http://b/791706.
  if (value->IsNull()) {
    // impl->frame() has to be a non-null LocalFrame.  Otherwise, the
    // same-origin check would have failed.
    ASSERT(impl->frame());
    toLocalFrame(impl->frame())->loader().setOpener(0);
  }

  // Delete the accessor from the inner object.
  info.Holder()->Delete(isolate->GetCurrentContext(),
                        v8AtomicString(isolate, "opener"));

  // Put property on the inner object.
  if (info.Holder()->IsObject()) {
    v8::Maybe<bool> unused =
        v8::Local<v8::Object>::Cast(info.Holder())
            ->Set(isolate->GetCurrentContext(),
                  v8AtomicString(isolate, "opener"), value);
    ALLOW_UNUSED_LOCAL(unused);
  }
}

void V8Window::postMessageMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(),
                                ExceptionState::ExecutionContext, "Window",
                                "postMessage");
  if (UNLIKELY(info.Length() < 2)) {
    exceptionState.throwTypeError(
        ExceptionMessages::notEnoughArguments(2, info.Length()));
    return;
  }

  // None of these need to be RefPtr because info and context are guaranteed
  // to hold on to them.
  DOMWindow* window = V8Window::toImpl(info.Holder());
  // TODO(yukishiino): The HTML spec specifies that we should use the
  // Incumbent Realm instead of the Current Realm, but currently we don't have
  // a way to retrieve the Incumbent Realm.  See also:
  // https://html.spec.whatwg.org/multipage/comms.html#dom-window-postmessage
  LocalDOMWindow* source = currentDOMWindow(info.GetIsolate());

  ASSERT(window);
  UseCounter::count(window->frame(), UseCounter::WindowPostMessage);

  // If called directly by WebCore we don't have a calling context.
  if (!source) {
    exceptionState.throwTypeError("No active calling context exists.");
    return;
  }

  // This function has variable arguments and can be:
  //   postMessage(message, targetOrigin)
  //   postMessage(message, targetOrigin, {sequence of transferrables})
  // TODO(foolip): Type checking of the arguments should happen in order, so
  // that e.g. postMessage({}, { toString: () => { throw Error(); } }, 0)
  // throws the Error from toString, not the TypeError for argument 3.
  Transferables transferables;
  const int targetOriginArgIndex = 1;
  if (info.Length() > 2) {
    const int transferablesArgIndex = 2;
    if (!SerializedScriptValue::extractTransferables(
            info.GetIsolate(), info[transferablesArgIndex],
            transferablesArgIndex, transferables, exceptionState)) {
      return;
    }
  }
  // TODO(foolip): targetOrigin should be a USVString in IDL and treated as
  // such here, without TreatNullAndUndefinedAsNullString.
  TOSTRING_VOID(V8StringResource<TreatNullAndUndefinedAsNullString>,
                targetOrigin, info[targetOriginArgIndex]);

  RefPtr<SerializedScriptValue> message = SerializedScriptValue::serialize(
      info.GetIsolate(), info[0], &transferables, nullptr, exceptionState);
  if (exceptionState.hadException())
    return;

  window->postMessage(message.release(), transferables.messagePorts,
                      targetOrigin, source, exceptionState);
}

void V8Window::openMethodCustom(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DOMWindow* impl = V8Window::toImpl(info.Holder());
  ExceptionState exceptionState(
      info.GetIsolate(), ExceptionState::ExecutionContext, "Window", "open");
  if (!BindingSecurity::shouldAllowAccessTo(currentDOMWindow(info.GetIsolate()),
                                            impl, exceptionState)) {
    return;
  }

  TOSTRING_VOID(V8StringResource<TreatNullAndUndefinedAsNullString>, urlString,
                info[0]);
  AtomicString frameName;
  if (info[1]->IsUndefined() || info[1]->IsNull()) {
    frameName = "_blank";
  } else {
    TOSTRING_VOID(V8StringResource<>, frameNameResource, info[1]);
    frameName = frameNameResource;
  }
  TOSTRING_VOID(V8StringResource<TreatNullAndUndefinedAsNullString>,
                windowFeaturesString, info[2]);

  // |impl| has to be a LocalDOMWindow, since RemoteDOMWindows wouldn't have
  // passed the BindingSecurity check above.
  DOMWindow* openedWindow = toLocalDOMWindow(impl)->open(
      urlString, frameName, windowFeaturesString,
      currentDOMWindow(info.GetIsolate()), enteredDOMWindow(info.GetIsolate()));
  if (!openedWindow) {
    v8SetReturnValueNull(info);
    return;
  }

  v8SetReturnValueFast(info, openedWindow, impl);
}

void V8Window::namedPropertyGetterCustom(
    const AtomicString& name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  DOMWindow* window = V8Window::toImpl(info.Holder());
  if (!window)
    return;

  Frame* frame = window->frame();
  // window is detached from a frame.
  if (!frame)
    return;

  // Note that named access on WindowProxy is allowed in the cross-origin case.
  // 7.4.5 [[GetOwnProperty]] (P), step 6.
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getownproperty
  //
  // 7.3.3 Named access on the Window object
  // The document-tree child browsing context name property set
  // https://html.spec.whatwg.org/multipage/browsers.html#document-tree-child-browsing-context-name-property-set
  Frame* child = frame->tree().scopedChild(name);
  if (child) {
    // step 3. Remove each browsing context from childBrowsingContexts whose
    // active document's origin is not same origin with activeDocument's origin
    // and whose browsing context name does not match the name of its browsing
    // context container's name content attribute value.
    if (BindingSecurity::shouldAllowNamedAccessTo(window, child->domWindow()) ||
        name == child->owner()->browsingContextContainerName()) {
      v8SetReturnValueFast(info, child->domWindow(), window);
      return;
    }

    // In addition to the above spec'ed case, we return the child window
    // regardless of step 3 due to crbug.com/701489 for the time being.
    // TODO(yukishiino): Makes iframe.name update the browsing context name
    // appropriately and makes the new name available in the named access on
    // window.  Then, removes the following two lines.
    v8SetReturnValueFast(info, child->domWindow(), window);
    return;
  }

  // This is a cross-origin interceptor. Check that the caller has access to the
  // named results.
  if (!BindingSecurity::shouldAllowAccessTo(
          currentDOMWindow(info.GetIsolate()), window,
          BindingSecurity::ErrorReportOption::DoNotReport)) {
    BindingSecurity::failedAccessCheckFor(info.GetIsolate(), frame);
    return;
  }

  // Search named items in the document.
  Document* doc = toLocalFrame(frame)->document();
  if (!doc || !doc->isHTMLDocument())
    return;

  bool hasNamedItem = toHTMLDocument(doc)->hasNamedItem(name);
  bool hasIdItem = doc->hasElementWithId(name);

  if (!hasNamedItem && !hasIdItem)
    return;

  if (!hasNamedItem && hasIdItem &&
      !doc->containsMultipleElementsWithId(name)) {
    v8SetReturnValueFast(info, doc->getElementById(name), window);
    return;
  }

  HTMLCollection* items = doc->windowNamedItems(name);
  if (!items->isEmpty()) {
    // TODO(esprehn): Firefox doesn't return an HTMLCollection here if there's
    // multiple with the same name, but Chrome and Safari does. What's the
    // right behavior?
    if (items->hasExactlyOneItem()) {
      v8SetReturnValueFast(info, items->item(0), window);
      return;
    }
    v8SetReturnValueFast(info, items, window);
    return;
  }
}

}  // namespace blink
