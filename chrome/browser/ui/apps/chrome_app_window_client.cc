// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_client.h"

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/apps/scoped_keep_alive.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/devtools_agent_host.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"

#include "content/nw/src/nw_content.h"

// TODO(jamescook): We probably shouldn't compile this class at all on Android.
// See http://crbug.com/343612
#if !defined(OS_ANDROID)
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#endif

ChromeAppWindowClient::ChromeAppWindowClient() {
}

ChromeAppWindowClient::~ChromeAppWindowClient() {
}

// static
ChromeAppWindowClient* ChromeAppWindowClient::GetInstance() {
  return base::Singleton<
      ChromeAppWindowClient,
      base::LeakySingletonTraits<ChromeAppWindowClient>>::get();
}

extensions::AppWindow* ChromeAppWindowClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
#if defined(OS_ANDROID)
  return NULL;
#else
  extensions::AppWindow* ret = new extensions::AppWindow(
      context,
      new ChromeAppDelegate(make_scoped_ptr(new ScopedKeepAlive)),
      extension);
  nw::CreateAppWindowHook(ret);
  return ret;
#endif
}

extensions::NativeAppWindow* ChromeAppWindowClient::CreateNativeAppWindow(
    extensions::AppWindow* window,
    extensions::AppWindow::CreateParams* params) {
#if defined(OS_ANDROID)
  return NULL;
#else
  return CreateNativeAppWindowImpl(window, *params);
#endif
}

void ChromeAppWindowClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    const base::Closure& callback) {
  scoped_refptr<content::DevToolsAgentHost> agent(
      content::DevToolsAgentHost::GetOrCreateFor(web_contents));
  DevToolsWindow::OpenDevToolsWindow(web_contents);

  DevToolsWindow* devtools_window =
      DevToolsWindow::FindDevToolsWindow(agent.get());
  if (devtools_window)
    devtools_window->SetLoadCompletedCallback(callback);
  else
    callback.Run();
}

bool ChromeAppWindowClient::IsCurrentChannelOlderThanDev() {
  return extensions::GetCurrentChannel() > version_info::Channel::DEV;
}
