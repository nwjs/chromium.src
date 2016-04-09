// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_impl.h"

#include <stddef.h>
#include <utility>

#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/net/port_server.h"

ChromeImpl::~ChromeImpl() {
  if (!quit_)
    port_reservation_->Leak();
}

Status ChromeImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  return Status(kUnknownError, "operation unsupported");
}

const BrowserInfo* ChromeImpl::GetBrowserInfo() const {
  return devtools_http_client_->browser_info();
}

bool ChromeImpl::HasCrashedWebView() {
  for (WebViewList::iterator it = web_views_.begin();
       it != web_views_.end(); ++it) {
    if ((*it)->WasCrashed())
      return true;
  }
  return false;
}

Status ChromeImpl::GetWebViewIds(std::list<std::string>* web_view_ids) {
  WebViewsInfo views_info;
  Status status = devtools_http_client_->GetWebViewsInfo(&views_info);
  if (status.IsError())
    return status;

  // Check if some web views are closed.
  WebViewList::iterator it = web_views_.begin();
  while (it != web_views_.end()) {
    if (!views_info.GetForId((*it)->GetId())) {
      it = web_views_.erase(it);
    } else {
      ++it;
    }
  }

  // Check for newly-opened web views.
  for (size_t i = 0; i < views_info.GetSize(); ++i) {
    const WebViewInfo& view = views_info.Get(i);
    if (devtools_http_client_->IsBrowserWindow(view.type) ||
        (view.type == WebViewInfo::kOther &&
         (view.url.find("chrome-extension://") == 0 ||
          view.url.find("http://") == 0 ||
          view.url.find("https://") == 0 ||
          view.url == "chrome://print/" ||
          view.url == "chrome://media-router/"))) {
      bool found = false;
      for (WebViewList::const_iterator web_view_iter = web_views_.begin();
           web_view_iter != web_views_.end(); ++web_view_iter) {
        if ((*web_view_iter)->GetId() == view.id) {
          found = true;
          break;
        }
      }
      if (!found) {
        scoped_ptr<DevToolsClient> client(
            devtools_http_client_->CreateClient(view.id));
        for (ScopedVector<DevToolsEventListener>::const_iterator listener =
                 devtools_event_listeners_.begin();
             listener != devtools_event_listeners_.end(); ++listener) {
          client->AddListener(*listener);
          // OnConnected will fire when DevToolsClient connects later.
        }
        web_views_.push_back(make_linked_ptr(new WebViewImpl(
            view.id, devtools_http_client_->browser_info(), std::move(client),
            devtools_http_client_->device_metrics())));
      }
    }
  }

  std::list<std::string> web_view_ids_tmp;
  for (WebViewList::const_iterator web_view_iter = web_views_.begin();
       web_view_iter != web_views_.end(); ++web_view_iter) {
    web_view_ids_tmp.push_back((*web_view_iter)->GetId());
  }
  web_view_ids->swap(web_view_ids_tmp);
  return Status(kOk);
}

Status ChromeImpl::GetWebViewById(const std::string& id, WebView** web_view) {
  for (WebViewList::iterator it = web_views_.begin();
       it != web_views_.end(); ++it) {
    if ((*it)->GetId() == id) {
      *web_view = (*it).get();
      return Status(kOk);
    }
  }
  return Status(kUnknownError, "web view not found");
}

Status ChromeImpl::CloseWebView(const std::string& id) {
  Status status = devtools_http_client_->CloseWebView(id);
  if (status.IsError())
    return status;
  for (WebViewList::iterator iter = web_views_.begin();
       iter != web_views_.end(); ++iter) {
    if ((*iter)->GetId() == id) {
      web_views_.erase(iter);
      break;
    }
  }
  return Status(kOk);
}

Status ChromeImpl::ActivateWebView(const std::string& id) {
  return devtools_http_client_->ActivateWebView(id);
}

bool ChromeImpl::IsMobileEmulationEnabled() const {
  return false;
}

bool ChromeImpl::HasTouchScreen() const {
  return false;
}

Status ChromeImpl::Quit() {
  Status status = QuitImpl();
  if (status.IsOk())
    quit_ = true;
  return status;
}

ChromeImpl::ChromeImpl(
    scoped_ptr<DevToolsHttpClient> http_client,
    scoped_ptr<DevToolsClient> websocket_client,
    ScopedVector<DevToolsEventListener>& devtools_event_listeners,
    scoped_ptr<PortReservation> port_reservation)
    : quit_(false),
      devtools_http_client_(std::move(http_client)),
      devtools_websocket_client_(std::move(websocket_client)),
      port_reservation_(std::move(port_reservation)) {
  devtools_event_listeners_.swap(devtools_event_listeners);
}
