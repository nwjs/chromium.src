// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/render_frame_observer_natives.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"

namespace extensions {

namespace {

// Deletes itself when done.
class LoadWatcher : public content::RenderFrameObserver {
 public:
  LoadWatcher(content::RenderFrame* frame,
              const base::Callback<void(bool, int)>& callback, bool wait_for_next = false)
    : content::RenderFrameObserver(frame), callback_(callback), wait_for_next_(wait_for_next) {}

  void DidCreateDocumentElement() override {
    if (wait_for_next_) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
                                             base::Bind(&LoadWatcher::DidCreateDocumentElement, base::Unretained(this)));
      wait_for_next_ = false;
      return;
    }
    // The callback must be run as soon as the root element is available.
    // Running the callback may trigger DidCreateDocumentElement or
    // DidFailProvisionalLoad, so delete this before running the callback.
    base::Callback<void(bool, int)> callback = callback_;
    int id = routing_id();
    delete this;
    callback.Run(true, id);
  }

  void DidFailProvisionalLoad(const blink::WebURLError& error) override {
    // Use PostTask to avoid running user scripts while handling this
    // DidFailProvisionalLoad notification.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback_, false, routing_id()));
    delete this;
  }

 private:
  base::Callback<void(bool, int)> callback_;
  bool wait_for_next_;

  DISALLOW_COPY_AND_ASSIGN(LoadWatcher);
};

class CloseWatcher : public content::RenderFrameObserver {
 public:
  CloseWatcher(ScriptContext* context,
               content::RenderFrame* frame,
               v8::Local<v8::Function> cb)
      : content::RenderFrameObserver(frame),
        context_(context->weak_factory_.GetWeakPtr()),
        callback_(context->isolate(), cb)
  {
  }

  void OnDestruct() override {
    base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&CloseWatcher::CallbackAndDie, base::Unretained(this),
                     routing_id()));
  }

 private:
  void CallbackAndDie(int routing_id) {
    if (context_) {
      // context_ was deleted when running
      // issue4007-reload-lost-app-window in test framework
      v8::Isolate* isolate = context_->isolate();
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, routing_id)};
      context_->CallFunction(v8::Local<v8::Function>::New(isolate, callback_),
                             arraysize(args), args);
    }
    delete this;
  }

  base::WeakPtr<ScriptContext> context_;
  v8::Global<v8::Function> callback_;

  DISALLOW_COPY_AND_ASSIGN(CloseWatcher);
};

}  // namespace

RenderFrameObserverNatives::RenderFrameObserverNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context), weak_ptr_factory_(this) {
  RouteFunction(
      "OnDocumentElementCreated",
      base::Bind(&RenderFrameObserverNatives::OnDocumentElementCreated,
                 base::Unretained(this)));
  RouteFunction(
      "OnDestruct",
      base::Bind(&RenderFrameObserverNatives::OnDestruct,
                 base::Unretained(this)));
}

RenderFrameObserverNatives::~RenderFrameObserverNatives() {}

void RenderFrameObserverNatives::Invalidate() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  ObjectBackedNativeHandler::Invalidate();
}

void RenderFrameObserverNatives::OnDocumentElementCreated(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());
  bool wait_for_next = false;
  if (args.Length() > 2)
    wait_for_next = args[2]->BooleanValue();

  int frame_id = args[0]->Int32Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register LoadWatcher.";
    return;
  }

  v8::Global<v8::Function> v8_callback(context()->isolate(),
                                       args[1].As<v8::Function>());
  base::Callback<void(bool, int)> callback(
      base::Bind(&RenderFrameObserverNatives::InvokeCallback,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&v8_callback)));
  if (!wait_for_next && ExtensionFrameHelper::Get(frame)->did_create_current_document_element()) {
    // If the document element is already created, then we can call the callback
    // immediately (though use PostTask to ensure that the callback is called
    // asynchronously).
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, true, frame_id));
  } else {
    new LoadWatcher(frame, callback, wait_for_next);
  }

  args.GetReturnValue().Set(true);
}

void RenderFrameObserverNatives::InvokeCallback(
    v8::Global<v8::Function> callback,
    bool succeeded, int frame_id) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> args[] = {v8::Boolean::New(isolate, succeeded), v8::Integer::New(isolate, frame_id)};
  context()->CallFunction(v8::Local<v8::Function>::New(isolate, callback),
                          arraysize(args), args);
}

void RenderFrameObserverNatives::OnDestruct(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());
  int frame_id = args[0]->Int32Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register CloseWatcher. " << frame_id;
    return;
  }

  v8::Local<v8::Function> func = args[1].As<v8::Function>();
  ScriptContext* context = ScriptContextSet::GetContextByV8Context(func->CreationContext());
  new CloseWatcher(context, frame, args[1].As<v8::Function>());

  args.GetReturnValue().Set(true);
}


}  // namespace extensions
