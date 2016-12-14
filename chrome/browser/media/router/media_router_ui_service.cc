// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_ui_service.h"

#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/router/media_router_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace media_router {

MediaRouterUIService::MediaRouterUIService(Profile* profile)
#if defined(NWJS_SDK)
   : action_controller_(profile) {}
#else
  {}
#endif

MediaRouterUIService::~MediaRouterUIService() {}

// static
MediaRouterUIService* MediaRouterUIService::Get(Profile* profile) {
  return MediaRouterUIServiceFactory::GetForBrowserContext(profile);
}

}  // namespace media_router
