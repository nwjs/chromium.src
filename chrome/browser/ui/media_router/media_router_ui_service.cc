// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/media_router/media_router_ui_service.h"

#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/media_router/media_router_ui_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_prefs/user_prefs.h"

namespace media_router {

MediaRouterUIService::MediaRouterUIService(Profile* profile)
    : MediaRouterUIService(profile, nullptr) {}

MediaRouterUIService::MediaRouterUIService(
    Profile* profile,
    std::unique_ptr<MediaRouterActionController> action_controller)
    : profile_(profile),
#if defined(NWJS_SDK)
      action_controller_(std::move(action_controller)),
#endif
      profile_pref_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  profile_pref_registrar_->Init(profile->GetPrefs());
  profile_pref_registrar_->Add(
      ::prefs::kEnableMediaRouter,
      base::BindRepeating(&MediaRouterUIService::ConfigureService,
                          base::Unretained(this)));
  ConfigureService();
}

MediaRouterUIService::~MediaRouterUIService() {}

void MediaRouterUIService::Shutdown() {
  DisableService();
}

// static
MediaRouterUIService* MediaRouterUIService::Get(Profile* profile) {
  return MediaRouterUIServiceFactory::GetForBrowserContext(profile);
}

MediaRouterActionController* MediaRouterUIService::action_controller() {
#if defined(NWJS_SDK)
  return action_controller_.get();
#else
  return nullptr;
#endif
}

void MediaRouterUIService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MediaRouterUIService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MediaRouterUIService::ConfigureService() {
  if (!MediaRouterEnabled(profile_)) {
    DisableService();
  }
#if defined(NWJS_SDK)
  else if (!action_controller_ && MediaRouterEnabled(profile_)) {
    action_controller_ =
        std::make_unique<MediaRouterActionController>(profile_);
  }
#endif
}

void MediaRouterUIService::DisableService() {
  for (auto& observer : observers_)
    observer.OnServiceDisabled();
#if defined(NWJS_SDK)
  action_controller_.reset();
#endif
}

}  // namespace media_router
