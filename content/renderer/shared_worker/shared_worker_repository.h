// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_SHARED_WORKER_REPOSITORY_H_
#define CONTENT_RENDERER_SHARED_WORKER_SHARED_WORKER_REPOSITORY_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebAddressSpace.h"
#include "third_party/WebKit/public/web/WebContentSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationContextType.h"
#include "third_party/WebKit/public/web/WebSharedWorkerRepositoryClient.h"

namespace content {

class RenderFrameImpl;

class SharedWorkerRepository final
    : public blink::WebSharedWorkerRepositoryClient {
 public:
  explicit SharedWorkerRepository(RenderFrameImpl* render_frame);
  ~SharedWorkerRepository();

  // WebSharedWorkerRepositoryClient overrides.
  void connect(
      bool isNodeJS,
      const blink::WebURL& url,
      const blink::WebString& name,
      DocumentID document_id,
      const blink::WebString& content_security_policy,
      blink::WebContentSecurityPolicyType,
      blink::WebAddressSpace,
      blink::WebSharedWorkerCreationContextType,
      blink::WebMessagePortChannel* channel,
      std::unique_ptr<blink::WebSharedWorkerConnectListener> listener) override;
  void documentDetached(DocumentID document_id) override;

 private:
  RenderFrameImpl* render_frame_;
  std::set<DocumentID> documents_with_workers_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerRepository);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_SHARED_WORKER_REPOSITORY_H_
