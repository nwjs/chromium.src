// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_observer.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"

void BlacklistObserver::ContentBlocked(const URLRequest* /*request*/) {
  // TODO

  /* This code will probably be useful when this function is implemented.
  const URLRequest::UserData* d =
      request->GetUserData(&Blacklist::kRequestDataKey);
  const Blacklist::Match* match = static_cast<const Blacklist::Match*>(d);
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  const GURL& gurl = request->url();
  */
}
