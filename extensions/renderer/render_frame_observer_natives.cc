// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/render_frame_observer_natives.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/script_context.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-primitive.h"

#include "extensions/renderer/script_context_set.h"

namespace extensions {

namespace {

// Deletes itself when done.
class LoadWatcher : public content::RenderFrameObserver {
 public:
  LoadWatcher(content::RenderFrame* frame,
              base::OnceCallback<void(bool, int)> callback, bool wait_for_next = false)
    : content::RenderFrameObserver(frame), callback_(std::move(callback)), wait_for_next_(wait_for_next) {}

  LoadWatcher(const LoadWatcher&) = delete;
  LoadWatcher& operator=(const LoadWatcher&) = delete;

  void DidCreateDocumentElement() override {
    if (wait_for_next_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                             base::BindOnce(&LoadWatcher::DidCreateDocumentElement, base::Unretained(this)));
      wait_for_next_ = false;
      return;
    }
    // Defer the callback instead of running it now to avoid re-entrancy caused
    // by the JavaScript callback.
    int id = routing_id();
    ExtensionFrameHelper::Get(render_frame())
      ->ScheduleAtDocumentStart(base::BindOnce(std::move(callback_), true, id));
    delete this;
  }

  void DidFailProvisionalLoad() override {
    // Use PostTask to avoid running user scripts while handling this
    // DidFailProvisionalLoad notification.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
                                                  FROM_HERE, base::BindOnce(std::move(callback_), false, routing_id()));
    delete this;
  }

  void OnDestruct() override { delete this; }

 private:
  base::OnceCallback<void(bool, int)> callback_;
  bool wait_for_next_;
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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&CloseWatcher::CallbackAndDie, base::Unretained(this),
                     routing_id()));
  }

 private:
  void CallbackAndDie(int routing_id) {
    if (context_ && context_->is_valid()) {
      // context_ was deleted when running
      // issue4007-reload-lost-app-window in test framework
      v8::Isolate* isolate = context_->isolate();
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Value> args[] = {v8::Integer::New(isolate, routing_id)};
      context_->SafeCallFunction(v8::Local<v8::Function>::New(isolate, callback_),
                                 std::size(args), args);
    }
    delete this;
  }

  base::WeakPtr<ScriptContext> context_;
  v8::Global<v8::Function> callback_;

};

}  // namespace

RenderFrameObserverNatives::RenderFrameObserverNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

RenderFrameObserverNatives::~RenderFrameObserverNatives() {}

void RenderFrameObserverNatives::AddRoutes() {
  RouteHandlerFunction(
      "OnDocumentElementCreated", "app.window",
      base::BindRepeating(&RenderFrameObserverNatives::OnDocumentElementCreated,
                          base::Unretained(this)));
  RouteHandlerFunction(
      "OnDestruct",
      base::BindRepeating(&RenderFrameObserverNatives::OnDestruct,
                 base::Unretained(this)));
}

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
    wait_for_next = args[2].As<v8::Boolean>()->Value();

  int frame_id = args[0].As<v8::Int32>()->Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register LoadWatcher.";
    return;
  }

  v8::Global<v8::Function> v8_callback(context()->isolate(),
                                       args[1].As<v8::Function>());
  auto callback(base::BindOnce(&RenderFrameObserverNatives::InvokeCallback,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(v8_callback)));
  if (!wait_for_next && ExtensionFrameHelper::Get(frame)->did_create_current_document_element()) {
    // If the document element is already created, then we can call the callback
    // immediately (though use PostTask to ensure that the callback is called
    // asynchronously).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
                                                  FROM_HERE, base::BindOnce(std::move(callback), true, frame_id));
  } else {
    new LoadWatcher(frame, std::move(callback), wait_for_next);
  }

  args.GetReturnValue().Set(true);
}

void RenderFrameObserverNatives::InvokeCallback(
    v8::Global<v8::Function> callback,
    bool succeeded, int frame_id) {
  v8::Isolate* isolate = context()->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> args[] = {v8::Boolean::New(isolate, succeeded), v8::Integer::New(isolate, frame_id)};
  context()->SafeCallFunction(v8::Local<v8::Function>::New(isolate, callback),
                              std::size(args), args);
}

void RenderFrameObserverNatives::OnDestruct(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsFunction());
  int frame_id = args[0].As<v8::Int32>()->Value();

  content::RenderFrame* frame = content::RenderFrame::FromRoutingID(frame_id);
  if (!frame) {
    LOG(WARNING) << "No render frame found to register CloseWatcher. " << frame_id;
    return;
  }

  v8::Local<v8::Function> func = args[1].As<v8::Function>();
  v8::Local<v8::Context> v8_context;
  if (!func->GetCreationContext().ToLocal(&v8_context)) {
    args.GetReturnValue().Set(false);
    return;
  }
  ScriptContext* context = ScriptContextSet::GetContextByV8Context(v8_context);
  new CloseWatcher(context, frame, args[1].As<v8::Function>());

  args.GetReturnValue().Set(true);
}


}  // namespace extensions
