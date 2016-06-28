// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/request_sender.h"

#include "base/values.h"
#include "content/public/renderer/render_frame.h"
#include "extensions/common/extension_messages.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebUserGestureToken.h"

namespace extensions {

// Contains info relevant to a pending API request.
struct PendingRequest {
 public:
  PendingRequest(const std::string& name,
                 RequestSender::Source* source,
                 blink::WebUserGestureToken token)
      : name(name), source(source), token(token) {}

  std::string name;
  RequestSender::Source* source;
  blink::WebUserGestureToken token;
};

RequestSender::ScopedTabID::ScopedTabID(RequestSender* request_sender,
                                        int tab_id)
    : request_sender_(request_sender),
      tab_id_(tab_id),
      previous_tab_id_(request_sender->source_tab_id_) {
  request_sender_->source_tab_id_ = tab_id;
}

RequestSender::ScopedTabID::~ScopedTabID() {
  DCHECK_EQ(tab_id_, request_sender_->source_tab_id_);
  request_sender_->source_tab_id_ = previous_tab_id_;
}

RequestSender::RequestSender() : source_tab_id_(-1) {}

RequestSender::~RequestSender() {}

void RequestSender::InsertRequest(int request_id,
                                  PendingRequest* pending_request) {
  DCHECK_EQ(0u, pending_requests_.count(request_id));
  pending_requests_[request_id].reset(pending_request);
}

linked_ptr<PendingRequest> RequestSender::RemoveRequest(int request_id) {
  PendingRequestMap::iterator i = pending_requests_.find(request_id);
  if (i == pending_requests_.end())
    return linked_ptr<PendingRequest>();
  linked_ptr<PendingRequest> result = i->second;
  pending_requests_.erase(i);
  return result;
}

int RequestSender::GetNextRequestId() const {
  static int next_request_id = 0;
  return next_request_id++;
}

void RequestSender::StartRequestSync(Source* source,
                                     const std::string& name,
                                     int request_id,
                                     bool has_callback,
                                     bool for_io_thread,
                                     base::ListValue* value_args,
                                     bool* success,
                                     base::ListValue* response,
                                     std::string* error) {
  ScriptContext* context = source->GetContext();
  if (!context)
    return;

  // Get the current RenderView so that we can send a routed IPC message from
  // the correct source.
  content::RenderFrame* renderframe = context->GetRenderFrame();
  if (!renderframe)
    return;

  // TODO(koz): See if we can make this a CHECK.
  if (!context->HasAccessOrThrowError(name))
    return;

  GURL source_url;
  if (blink::WebLocalFrame* webframe = context->web_frame())
    source_url = webframe->document().url();

  // InsertRequest(request_id, new PendingRequest(name, source,
  //     blink::WebUserGestureIndicator::currentUserGestureToken()));

  ExtensionHostMsg_Request_Params params;
  params.name = name;
  params.arguments.Swap(value_args);
  params.extension_id = context->GetExtensionID();
  params.source_url = source_url;
  params.source_tab_id = source_tab_id_;
  params.request_id = request_id;
  params.has_callback = has_callback;
  params.user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGesture();
  if (for_io_thread) {
    NOTREACHED() << "not implemented";
  } else {
    renderframe->Send(
                     new ExtensionHostMsg_RequestSync(renderframe->GetRoutingID(), params,
                                                      success, response, error));
  }
}

void RequestSender::StartRequest(Source* source,
                                 const std::string& name,
                                 int request_id,
                                 bool has_callback,
                                 bool for_io_thread,
                                 base::ListValue* value_args) {
  ScriptContext* context = source->GetContext();
  if (!context)
    return;

  // Get the current RenderFrame so that we can send a routed IPC message from
  // the correct source.
  content::RenderFrame* render_frame = context->GetRenderFrame();
  if (!render_frame)
    return;

  // TODO(koz): See if we can make this a CHECK.
  if (!context->HasAccessOrThrowError(name))
    return;

  GURL source_url;
  if (blink::WebLocalFrame* webframe = context->web_frame())
    source_url = webframe->document().url();

  InsertRequest(request_id, new PendingRequest(name, source,
      blink::WebUserGestureIndicator::currentUserGestureToken()));

  ExtensionHostMsg_Request_Params params;
  params.name = name;
  params.arguments.Swap(value_args);
  params.extension_id = context->GetExtensionID();
  params.source_url = source_url;
  params.source_tab_id = source_tab_id_;
  params.request_id = request_id;
  params.has_callback = has_callback;
  params.user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGesture();
  if (for_io_thread) {
    render_frame->Send(new ExtensionHostMsg_RequestForIOThread(
        render_frame->GetRoutingID(), params));
  } else {
    render_frame->Send(
        new ExtensionHostMsg_Request(render_frame->GetRoutingID(), params));
  }
}

void RequestSender::HandleResponse(int request_id,
                                   bool success,
                                   const base::ListValue& response,
                                   const std::string& error) {
  linked_ptr<PendingRequest> request = RemoveRequest(request_id);

  if (!request.get()) {
    // This can happen if a context is destroyed while a request is in flight.
    return;
  }

  blink::WebScopedUserGesture gesture(request->token);
  request->source->OnResponseReceived(
      request->name, request_id, success, response, error);
}

void RequestSender::InvalidateSource(Source* source) {
  for (PendingRequestMap::iterator it = pending_requests_.begin();
       it != pending_requests_.end();) {
    if (it->second->source == source)
      pending_requests_.erase(it++);
    else
      ++it;
  }
}

}  // namespace extensions
