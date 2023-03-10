// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/password_manager_resources.h"
#include "chrome/grit/password_manager_resources_map.h"
#include "components/grit/components_scaled_resources.h"
#include "components/password_manager/content/common/web_ui_constants.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/chrome_unscaled_resources.h"
#endif

namespace {

content::WebUIDataSource* CreateAndAddPasswordsUIHTMLSource(
    Profile* profile,
    content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, password_manager::kChromeUIPasswordManagerHost);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kPasswordManagerResources, kPasswordManagerResourcesSize),
      IDR_PASSWORD_MANAGER_PASSWORD_MANAGER_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"addPassword", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD_BUTTON},
      {"addPasswordFooter", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD_FOOTNOTE},
      {"addPasswordTitle", IDS_PASSWORD_MANAGER_UI_ADD_PASSWORD},
      {"addShortcut", IDS_PASSWORD_MANAGER_UI_ADD_SHORTCUT_TITLE},
      {"addShortcutDescription",
       IDS_PASSWORD_MANAGER_UI_ADD_SHORTCUT_DESCRIPTION},
      {"autosigninDescription", IDS_PASSWORD_MANAGER_UI_AUTOSIGNIN_TOGGLE_DESC},
      {"autosigninLabel", IDS_PASSWORD_MANAGER_UI_AUTOSIGNIN_TOGGLE_LABEL},
      {"blockedSitesDescription",
       IDS_PASSWORD_MANAGER_UI_BLOCKED_SITES_DESCRIPTION},
      {"blockedSitesEmptyDescription",
       IDS_PASSWORD_MANAGER_UI_NO_BLOCKED_SITES_DESCRIPTION},
      {"blockedSitesTitle", IDS_PASSWORD_MANAGER_UI_BLOCKED_SITES_TITLE},
      {"cancel", IDS_CANCEL},
      {"changePassword", IDS_PASSWORD_MANAGER_UI_CHANGE_PASSWORD_BUTTON},
      {"checkup", IDS_PASSWORD_MANAGER_UI_CHECKUP},
      {"checkupCanceled", IDS_PASSWORD_MANAGER_UI_CHECKUP_CANCELED},
      {"checkupErrorGeneric", IDS_PASSWORD_MANAGER_UI_CHECKUP_OTHER_ERROR},
      {"checkupErrorNoPasswords", IDS_PASSWORD_MANAGER_UI_CHECKUP_NO_PASSWORDS},
      {"checkupErrorOffline", IDS_PASSWORD_MANAGER_UI_CHECKUP_OFFLINE},
      {"checkupErrorQuota", IDS_PASSWORD_MANAGER_UI_CHECKUP_QUOTA_LIMIT},
      {"checkupErrorSignedOut", IDS_PASSWORD_MANAGER_UI_CHECKUP_SIGNED_OUT},
      {"compromisedRowWithError",
       IDS_PASSWORD_MANAGER_UI_CHECKUP_COMPROMISED_SECTION},
      {"checkupProgress", IDS_PASSWORD_MANAGER_UI_CHECKUP_PROGRESS},
      {"checkupTitle", IDS_PASSWORD_MANAGER_UI_CHECKUP_TITLE},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"close", IDS_CLOSE},
      {"compromisedPasswordsDescription",
       IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_DESCRIPTION},
      {"compromisedPasswordsEmpty",
       IDS_PASSWORD_MANAGER_UI_NO_COMPROMISED_PASSWORDS},
      {"compromisedPasswordsTitle",
       IDS_PASSWORD_MANAGER_UI_HAS_COMPROMISED_PASSWORDS},
      {"copyPassword", IDS_PASSWORD_MANAGER_UI_COPY_PASSWORD},
      {"copyUsername", IDS_PASSWORD_MANAGER_UI_COPY_USERNAME},
      {"deletePassword", IDS_DELETE},
      {"downloadFile", IDS_PASSWORD_MANAGER_UI_DOWNLOAD_FILE},
      {"editPassword", IDS_EDIT},
      {"emptyNote", IDS_PASSWORD_MANAGER_UI_NO_NOTE_SAVED},
      {"exportPasswords", IDS_PASSWORD_MANAGER_UI_EXPORT_TITLE},
      {"exportPasswordsDescription",
       IDS_PASSWORD_MANAGER_UI_EXPORT_BANNER_DESCRIPTION},
      {"exportPasswordsFailTips",
       IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TIPS},
      {"exportPasswordsFailTipsAnotherFolder",
       IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TIP_ANOTHER_FOLDER},
      {"exportPasswordsFailTipsEnoughSpace",
       IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TIP_ENOUGH_SPACE},
      {"exportPasswordsFailTitle",
       IDS_PASSWORD_MANAGER_UI_EXPORTING_FAILURE_TITLE},
      {"exportPasswordsTryAgain", IDS_PASSWORD_MANAGER_UI_EXPORT_TRY_AGAIN},
      {"exportingPasswordsTitle", IDS_PASSWORD_MANAGER_UI_EXPORTING_TITLE},
      {"federationLabel", IDS_PASSWORD_MANAGER_UI_FEDERATION_LABEL},
      {"hidePassword", IDS_PASSWORD_MANAGER_UI_HIDE_PASSWORD},
      {"importPasswords", IDS_PASSWORD_MANAGER_UI_IMPORT_BANNER_TITLE},
      {"importPasswordsDescription",
       IDS_PASSWORD_MANAGER_UI_IMPORT_BANNER_DESCRIPTION},
      {"justNow", IDS_PASSWORD_MANAGER_UI_JUST_NOW},
      {"leakedPassword", IDS_PASSWORD_MANAGER_UI_PASSWORD_LEAKED},
      {"localPasswordManager",
       IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SAVING_ON_DEVICE},
      {"menu", IDS_MENU},
      {"moreActions", IDS_PASSWORD_MANAGER_UI_MORE_ACTIONS},
      {"muteCompromisedPassword", IDS_PASSWORD_MANAGER_UI_MUTE_ISSUE},
      {"mutedCompromisedCredentials",
       IDS_PASSWORD_MANAGER_UI_MUTED_COMPROMISED_PASSWORDS},
      {"notesLabel", IDS_PASSWORD_MANAGER_UI_NOTES_LABEL},
      {"passwordCopiedToClipboard",
       IDS_PASSWORD_MANAGER_UI_PASSWORD_COPIED_TO_CLIPBOARD},
      {"passwordLabel", IDS_PASSWORD_MANAGER_UI_PASSWORD_LABEL},
      {"passwordManager",
       IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT},
      {"passwords", IDS_PASSWORD_MANAGER_UI_PASSWORDS},
      {"phishedAndLeakedPassword",
       IDS_PASSWORD_MANAGER_UI_PASSWORD_PHISHED_AND_LEAKED},
      {"phishedPassword", IDS_PASSWORD_MANAGER_UI_PASSWORD_PHISHED},
      {"reusedPasswordsDescription",
       IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_DESCRIPTION},
      {"reusedPasswordsEmpty", IDS_PASSWORD_MANAGER_UI_NO_REUSED_PASSWORDS},
      {"reusedPasswordsTitle", IDS_PASSWORD_MANAGER_UI_HAS_REUSED_PASSWORDS},
      {"save", IDS_SAVE},
      {"savePasswordsLabel",
       IDS_PASSWORD_MANAGER_UI_SAVE_PASSWORDS_TOGGLE_LABEL},
      {"searchPrompt", IDS_PASSWORD_MANAGER_UI_SEARCH_PROMPT},
      {"settings", IDS_PASSWORD_MANAGER_UI_SETTINGS},
      {"showPassword", IDS_PASSWORD_MANAGER_UI_SHOW_PASSWORD},
      {"sitesLabel", IDS_PASSWORD_MANAGER_UI_SITES_LABEL},
      {"title", IDS_PASSWORD_MANAGER_UI_TITLE},
      {"trustedVaultBannerLabelOfferOptIn",
       IDS_PASSWORD_MANAGER_UI_TRUSTED_VAULT_OPT_IN_TITLE},
      {"trustedVaultBannerSubLabelOfferOptIn",
       IDS_PASSWORD_MANAGER_UI_RUSTED_VAULT_OPT_IN_DESCRIPTION},
      {"tryAgain", IDS_PASSWORD_MANAGER_UI_CHECK_PASSWORDS_AFTER_ERROR},
      {"unmuteCompromisedPassword", IDS_PASSWORD_MANAGER_UI_UNMUTE_ISSUE},
      {"usernameCopiedToClipboard",
       IDS_PASSWORD_MANAGER_UI_USERNAME_COPIED_TO_CLIPBOARD},
      {"usernameLabel", IDS_PASSWORD_MANAGER_UI_USERNAME_LABEL},
      {"weakPasswordsDescription",
       IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_DESCRIPTION},
      {"weakPasswordsEmpty", IDS_PASSWORD_MANAGER_UI_NO_WEAK_PASSWORDS},
      {"weakPasswordsTitle", IDS_PASSWORD_MANAGER_UI_HAS_WEAK_PASSWORDS},
      {"websiteLabel", IDS_PASSWORD_MANAGER_UI_WEBSITE_LABEL},
  };
  for (const auto& str : kStrings)
    webui::AddLocalizedString(source, str.name, str.id);

  source->AddString(
      "passwordsSectionDescription",
      l10n_util::GetStringFUTF16(
          IDS_PASSWORD_MANAGER_UI_PASSWORDS_DESCRIPTION,
          base::ASCIIToUTF16(chrome::kPasswordManagerLearnMoreURL)));

  source->AddBoolean("isPasswordManagerShortcutInstalled",
                     web_app::FindInstalledAppWithUrlInScope(
                         profile, web_ui->GetWebContents()->GetURL())
                         .has_value());

  source->AddString(
      "checkupUrl",
      base::UTF8ToUTF16(
          password_manager::GetPasswordCheckupURL(
              password_manager::PasswordCheckupReferrer::kPasswordCheck)
              .spec()));

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Overwrite ubranded logo for Chrome-branded builds.
  source->AddResourcePath("images/password_manager_logo.svg",
                          IDR_CHROME_PASSWORD_MANAGER_LOGO);

  // This path is used in the manifest of the PasswordManager web app
  // (chrome/browser/resources/password_manager/
  // chrome_branded_manifest.webmanifest).
  source->AddResourcePath("images/password_manager_pwa_icon.svg",
                          IDR_CHROME_PASSWORD_MANAGER_PWA_ICON);
#endif

  return source;
}

void AddPluralStrings(content::WebUI* web_ui) {
  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString(
      "checkedPasswords", IDS_PASSWORD_MANAGER_UI_CHECKUP_RESULT);
  plural_string_handler->AddLocalizedString(
      "checkingPasswords", IDS_PASSWORD_MANAGER_UI_CHECKUP_RUNNING_LABEL);
  plural_string_handler->AddLocalizedString(
      "compromisedPasswords",
      IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString(
      "numberOfAccounts", IDS_PASSWORD_MANAGER_UI_NUMBER_OF_ACCOUNTS);
  plural_string_handler->AddLocalizedString(
      "reusedPasswords", IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_COUNT);
  plural_string_handler->AddLocalizedString(
      "weakPasswords", IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_COUNT);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

}  // namespace

PasswordManagerUI::PasswordManagerUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://password-manager/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  auto* source = CreateAndAddPasswordsUIHTMLSource(profile, web_ui);
  AddPluralStrings(web_ui);
  ManagedUIHandler::Initialize(web_ui, source);
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
}

// static
base::RefCountedMemory* PasswordManagerUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_PASSWORD_MANAGER_FAVICON, scale_factor));
}
