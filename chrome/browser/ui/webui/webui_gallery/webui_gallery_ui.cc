// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/webui_gallery_resources.h"
#include "chrome/grit/webui_gallery_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

void CreateAndAddWebuiGalleryUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIWebuiGalleryHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kWebuiGalleryResources, kWebuiGalleryResourcesSize),
      IDR_WEBUI_GALLERY_WEBUI_GALLERY_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc, "frame-src 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameAncestors,
      "frame-ancestors 'self';");

  source->AddString(
      "chromeRefresh2023Attribute",
      features::IsChromeRefresh2023() ? "chrome-refresh-2023" : "");
}

}  // namespace

WebuiGalleryUI::WebuiGalleryUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  CreateAndAddWebuiGalleryUIHtmlSource(Profile::FromWebUI(web_ui));
}

WebuiGalleryUI::~WebuiGalleryUI() = default;
