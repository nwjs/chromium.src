// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/md_settings_ui.h"

#include <stddef.h>

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/settings/md_settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/settings_resources.h"
#include "grit/settings_resources_map.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#endif  // defined(OS_CHROMEOS)

namespace settings {

SettingsPageUIHandler::SettingsPageUIHandler() {
}

SettingsPageUIHandler::~SettingsPageUIHandler() {
}

MdSettingsUI::MdSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
#if 0
  Profile* profile = Profile::FromWebUI(web_ui);
  AddSettingsPageUIHandler(new AppearanceHandler(web_ui));
  AddSettingsPageUIHandler(new ClearBrowsingDataHandler(web_ui));
  AddSettingsPageUIHandler(new DefaultBrowserHandler(web_ui));
  AddSettingsPageUIHandler(new DownloadsHandler());
  AddSettingsPageUIHandler(new FontHandler(web_ui));
  AddSettingsPageUIHandler(new LanguagesHandler(web_ui));
  AddSettingsPageUIHandler(new PeopleHandler(profile));
  AddSettingsPageUIHandler(new StartupPagesHandler(web_ui));

#if defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(new chromeos::settings::ChangePictureHandler());
#else
  AddSettingsPageUIHandler(new ManageProfileHandler(profile));
#endif

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIMdSettingsHost);

  AddSettingsPageUIHandler(ResetSettingsHandler::Create(html_source, profile));

  // Add all settings resources.
  for (size_t i = 0; i < kSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kSettingsResources[i].name,
                                 kSettingsResources[i].value);
  }

  AddLocalizedStrings(html_source, profile);
  html_source->SetDefaultResource(IDR_SETTINGS_SETTINGS_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
#endif
}

MdSettingsUI::~MdSettingsUI() {
}

void MdSettingsUI::AddSettingsPageUIHandler(
    content::WebUIMessageHandler* handler_raw) {
  scoped_ptr<content::WebUIMessageHandler> handler(handler_raw);
  DCHECK(handler.get());

  web_ui()->AddMessageHandler(handler.release());
}

}  // namespace settings
