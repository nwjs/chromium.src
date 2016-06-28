// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/send_request_natives.h"

#include <stdint.h>

#include "base/json/json_reader.h"
#include "content/public/child/v8_value_converter.h"
#include "extensions/renderer/request_sender.h"
#include "extensions/renderer/script_context.h"

using content::V8ValueConverter;

namespace extensions {

SendRequestNatives::SendRequestNatives(RequestSender* request_sender,
                                       ScriptContext* context)
    : ObjectBackedNativeHandler(context), request_sender_(request_sender) {
  RouteFunction(
      "StartRequest",
      base::Bind(&SendRequestNatives::StartRequest, base::Unretained(this)));
  RouteFunction(
      "StartRequestSync",
      base::Bind(&SendRequestNatives::StartRequestSync, base::Unretained(this)));
  RouteFunction(
      "GetGlobal",
      base::Bind(&SendRequestNatives::GetGlobal, base::Unretained(this)));
}

void SendRequestNatives::StartRequestSync(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(5, args.Length());
  std::string name = *v8::String::Utf8Value(args[0]);
  bool has_callback = args[2]->BooleanValue();
  bool for_io_thread = args[3]->BooleanValue();
  bool preserve_null_in_objects = args[4]->BooleanValue();

  int request_id = request_sender_->GetNextRequestId();
  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  // See http://crbug.com/149880. The context menus APIs relies on this, but
  // we shouldn't really be doing it (e.g. for the sake of the storage API).
  converter->SetFunctionAllowed(true);

  if (!preserve_null_in_objects)
    converter->SetStripNullFromObjects(true);

  std::unique_ptr<base::Value> value_args(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!value_args.get() || !value_args->IsType(base::Value::TYPE_LIST)) {
    NOTREACHED() << "Unable to convert args passed to StartRequestSync";
    return;
  }

  std::string error;
  bool success;
  base::ListValue response;
  request_sender_->StartRequestSync(
      context(),
      name,
      request_id,
      has_callback,
      for_io_thread,
      static_cast<base::ListValue*>(value_args.get()),
      &success,
      &response, &error
      );
  if (!success) {
    args.GetIsolate()->ThrowException(
                                      v8::String::NewFromUtf8(args.GetIsolate(), error.c_str()));
    return;
  }
  args.GetReturnValue().Set(converter->ToV8Value(&response,
                                                 context()->v8_context()));
}

// Starts an API request to the browser, with an optional callback.  The
// callback will be dispatched to EventBindings::HandleResponse.
void SendRequestNatives::StartRequest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(5, args.Length());
  std::string name = *v8::String::Utf8Value(args[0]);
  bool has_callback = args[2]->BooleanValue();
  bool for_io_thread = args[3]->BooleanValue();
  bool preserve_null_in_objects = args[4]->BooleanValue();

  int request_id = request_sender_->GetNextRequestId();
  args.GetReturnValue().Set(static_cast<int32_t>(request_id));

  std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());

  // See http://crbug.com/149880. The context menus APIs relies on this, but
  // we shouldn't really be doing it (e.g. for the sake of the storage API).
  converter->SetFunctionAllowed(true);

  if (!preserve_null_in_objects)
    converter->SetStripNullFromObjects(true);

  std::unique_ptr<base::Value> value_args(
      converter->FromV8Value(args[1], context()->v8_context()));
  if (!value_args.get() || !value_args->IsType(base::Value::TYPE_LIST)) {
    NOTREACHED() << "Unable to convert args passed to StartRequest";
    return;
  }

  request_sender_->StartRequest(
      context(),
      name,
      request_id,
      has_callback,
      for_io_thread,
      static_cast<base::ListValue*>(value_args.get()));
}

void SendRequestNatives::GetGlobal(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(1, args.Length());
  CHECK(args[0]->IsObject());
  v8::Local<v8::Context> v8_context =
      v8::Local<v8::Object>::Cast(args[0])->CreationContext();
  if (ContextCanAccessObject(context()->v8_context(), v8_context->Global(),
                             false)) {
    args.GetReturnValue().Set(v8_context->Global());
  }
}

}  // namespace extensions
