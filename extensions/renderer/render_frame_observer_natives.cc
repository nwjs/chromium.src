// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/render_frame_observer_natives.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"

namespace extensions {

namespace {

// Deletes itself when done.
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

// Deletes itself when done.
class LoadWatcher : public content::RenderFrameObserver {
 public:
  LoadWatcher(ScriptContext* context,
              content::RenderFrame* frame,
              v8::Local<v8::Function> cb,
              bool wait_for_next)
      : content::RenderFrameObserver(frame),
        context_(context),
        callback_(context->isolate(), cb),
        posted_(false), wait_for_next_(wait_for_next)
  {
    if (!wait_for_next && ExtensionFrameHelper::Get(frame)->
            did_create_current_document_element()) {
      // If the document element is already created, then we can call the
      // callback immediately (though post it to the message loop so as to not
      // call it re-entrantly).
      // The Unretained is safe because this class manages its own lifetime.
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&LoadWatcher::CallbackAndDie, base::Unretained(this),
                     true, frame->GetRoutingID()));
      posted_ = true;
    }
  }

  void DidCreateDocumentElement() override {
    if (wait_for_next_) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&LoadWatcher::CallbackAndDie, base::Unretained(this),
                     true, routing_id()));
      return;
    }
    
    if (!posted_) CallbackAndDie(true, routing_id());
  }

  void DidFailProvisionalLoad(const blink::WebURLError& error) override {
    CallbackAndDie(false, routing_id());
  }

 private:
  void CallbackAndDie(bool succeeded, int routing_id) {
    v8::Isolate* isolate = context_->isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> args[] = {v8::Boolean::New(isolate, succeeded),
                                   v8::Integer::New(isolate, routing_id)};
    context_->CallFunction(v8::Local<v8::Function>::New(isolate, callback_),
                           arraysize(args), args);
    delete this;
  }

  ScriptContext* context_;
  v8::Global<v8::Function> callback_;
  bool posted_;
  bool wait_for_next_;

  DISALLOW_COPY_AND_ASSIGN(LoadWatcher);
};

}  // namespace

RenderFrameObserverNatives::RenderFrameObserverNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction(
      "OnDocumentElementCreated",
      base::Bind(&RenderFrameObserverNatives::OnDocumentElementCreated,
                 base::Unretained(this)));
  RouteFunction(
      "OnDestruct",
      base::Bind(&RenderFrameObserverNatives::OnDestruct,
                 base::Unretained(this)));
}

void RenderFrameObserverNatives::OnDocumentElementCreated(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  //CHECK(args.Length() == 2);
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());
  bool wait_for_next = false;
  if (args.Length() > 2)
    wait_for_next = args[2]->BooleanValue();

  int frame_id = args[0]->Int32Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register LoadWatcher. " << frame_id;
    return;
  }

  new LoadWatcher(context(), frame, args[1].As<v8::Function>(), wait_for_next);

  args.GetReturnValue().Set(true);
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
