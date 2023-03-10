// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"

#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_turn_sync_on_delegate.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"

ProfilePickerSignedInFlowController::ProfilePickerSignedInFlowController(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    std::unique_ptr<content::WebContents> contents,
    signin_metrics::AccessPoint signin_access_point,
    absl::optional<SkColor> profile_color)
    : host_(host),
      profile_(profile),
      contents_(std::move(contents)),
      signin_access_point_(signin_access_point),
      profile_color_(profile_color) {
  DCHECK(profile_);
  DCHECK(contents_);
  // TODO(crbug.com/1300109): Consider renaming the enum entry -- this does not
  // have to be profile creation flow, it can be profile onboarding.
  profile_keep_alive_ = std::make_unique<ScopedProfileKeepAlive>(
      profile_, ProfileKeepAliveOrigin::kProfileCreationFlow);
}

ProfilePickerSignedInFlowController::~ProfilePickerSignedInFlowController() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  LOG(WARNING) << "crbug.com/1340791 | Flow controller destruction.";
#endif
  if (contents())
    contents()->SetDelegate(nullptr);
}

void ProfilePickerSignedInFlowController::Init() {
  DCHECK(!IsInitialized());

  contents()->SetDelegate(this);

  const CoreAccountInfo& account_info =
      IdentityManagerFactory::GetForProfile(profile_)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  DCHECK(!account_info.IsEmpty()) << "A profile with valid (unconsented) "
                                     "primary account must be passed in.";
  email_ = account_info.email;

  base::OnceClosure sync_consent_completed_closure =
      base::BindOnce(&ProfilePickerSignedInFlowController::FinishAndOpenBrowser,
                     weak_ptr_factory_.GetWeakPtr(), PostHostClearedCallback());

  // TurnSyncOnHelper deletes itself once done.
  new TurnSyncOnHelper(
      profile_, signin_access_point_,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO,
      signin_metrics::Reason::kSigninPrimaryAccount, account_info.account_id,
      TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
      std::make_unique<ProfilePickerTurnSyncOnDelegate>(
          weak_ptr_factory_.GetWeakPtr(), profile_),
      std::move(sync_consent_completed_closure));
}

void ProfilePickerSignedInFlowController::Cancel() {}

void ProfilePickerSignedInFlowController::SwitchToSyncConfirmation() {
  DCHECK(IsInitialized());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  PreShowScreenForDebug();
#endif
  host_->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/false),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignedInFlowController::
                                       SwitchToSyncConfirmationFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this)));
}

void ProfilePickerSignedInFlowController::SwitchToEnterpriseProfileWelcome(
    EnterpriseProfileWelcomeUI::ScreenType type,
    signin::SigninChoiceCallback proceed_callback) {
  DCHECK(IsInitialized());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  PreShowScreenForDebug();
#endif
  host_->ShowScreen(contents(),
                    GURL(chrome::kChromeUIEnterpriseProfileWelcomeURL),
                    /*navigation_finished_closure=*/
                    base::BindOnce(&ProfilePickerSignedInFlowController::
                                       SwitchToEnterpriseProfileWelcomeFinished,
                                   // Unretained is enough as the callback is
                                   // called by the owner of this instance.
                                   base::Unretained(this), type,
                                   std::move(proceed_callback)));
}

void ProfilePickerSignedInFlowController::SwitchToProfileSwitch(
    const base::FilePath& profile_path) {
  DCHECK(IsInitialized());
  // The sign-in flow is finished, no profile window should be shown in the end.
  Cancel();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  PreShowScreenForDebug();
#endif
  switch_profile_path_ = profile_path;
  host_->ShowScreenInPickerContents(
      GURL(chrome::kChromeUIProfilePickerUrl).Resolve("profile-switch"));
}

absl::optional<SkColor> ProfilePickerSignedInFlowController::GetProfileColor()
    const {
  // The new profile theme may be overridden by an existing policy theme. This
  // check ensures the correct theme is applied to the sync confirmation window.
  auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  if (theme_service->UsingPolicyTheme())
    return theme_service->GetPolicyThemeColor();
  return profile_color_;
}

GURL ProfilePickerSignedInFlowController::GetSyncConfirmationURL(bool loading) {
  GURL url = GURL(chrome::kChromeUISyncConfirmationURL);
  return AppendSyncConfirmationQueryParams(
      loading ? url.Resolve(chrome::kChromeUISyncConfirmationLoadingPath) : url,
      SyncConfirmationStyle::kWindow);
}

std::unique_ptr<content::WebContents>
ProfilePickerSignedInFlowController::ReleaseContents() {
  return std::move(contents_);
}

bool ProfilePickerSignedInFlowController::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // Ignores context menu.
  return true;
}

void ProfilePickerSignedInFlowController::SwitchToSyncConfirmationFinished() {
#if 0
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  SyncConfirmationUI* sync_confirmation_ui =
      static_cast<SyncConfirmationUI*>(contents()->GetWebUI()->GetController());

  sync_confirmation_ui->InitializeMessageHandlerWithBrowser(nullptr);
#endif
}

void ProfilePickerSignedInFlowController::
    SwitchToEnterpriseProfileWelcomeFinished(
        EnterpriseProfileWelcomeUI::ScreenType type,
        signin::SigninChoiceCallback proceed_callback) {
  DCHECK(IsInitialized());
  // Initialize the WebUI page once we know it's committed.
  EnterpriseProfileWelcomeUI* enterprise_profile_welcome_ui =
      contents()
          ->GetWebUI()
          ->GetController()
          ->GetAs<EnterpriseProfileWelcomeUI>();

  enterprise_profile_welcome_ui->Initialize(
      /*browser=*/nullptr, type,
      IdentityManagerFactory::GetForProfile(profile_)
          ->FindExtendedAccountInfoByEmailAddress(email_),
      /*profile_creation_required_by_policy=*/false,
      /*show_link_data_option=*/false, GetProfileColor(),
      std::move(proceed_callback));
}

bool ProfilePickerSignedInFlowController::IsInitialized() const {
  // `email_` is set in Init(), use it as the proxy here.
  return !email_.empty();
}
