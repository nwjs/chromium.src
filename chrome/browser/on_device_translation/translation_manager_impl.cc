// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_impl.h"

#include <string_view>

#if !BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/service_controller.h"
#include "chrome/browser/on_device_translation/translator.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "ui/base/l10n/l10n_util.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"

namespace {

#if !BUILDFLAG(IS_ANDROID)
using on_device_translation::SupportedLanguage;

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string& lang) {
  const std::string normalized_lang = l10n_util::GetLanguage(lang);
  return std::find_if(accept_languages.begin(), accept_languages.end(),
                      [&](const std::string_view& lang) {
                        return l10n_util::GetLanguage(lang) == normalized_lang;
                      }) != accept_languages.end();
}

bool IsSupportedPopularLanguage(const std::string& lang) {
  const std::optional<SupportedLanguage> supported_lang =
      on_device_translation::ToSupportedLanguage(lang);
  if (!supported_lang) {
    return false;
  }
  return on_device_translation::IsPopularLanguage(*supported_lang);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(TranslationManagerImpl);

TranslationManagerImpl::TranslationManagerImpl(content::RenderFrameHost* rfh)
    : DocumentUserData<TranslationManagerImpl>(rfh) {
  browser_context_ = rfh->GetBrowserContext()->GetWeakPtr();
}

TranslationManagerImpl::~TranslationManagerImpl() = default;

// static
void TranslationManagerImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::TranslationManager> receiver) {
  TranslationManagerImpl* translation_manager =
      TranslationManagerImpl::GetOrCreateForCurrentDocument(render_frame_host);
  translation_manager->receiver_.Bind(std::move(receiver));
}

void TranslationManagerImpl::CanCreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    CanCreateTranslatorCallback callback) {
  // The API is not supported on Android yet.
#if !BUILDFLAG(IS_ANDROID)
  CHECK(browser_context_);
  if (!PassAcceptLanguagesCheck(
          Profile::FromBrowserContext(browser_context_.get())
              ->GetPrefs()
              ->GetString(language::prefs::kAcceptLanguages),
          source_lang, target_lang)) {
    std::move(callback).Run(
        blink::mojom::CanCreateTranslatorResult::kNoAcceptLanguagesCheckFailed);
    return;
  }
  OnDeviceTranslationServiceController::GetInstance()->CanTranslate(
      source_lang, target_lang, std::move(callback));
#else
  std::move(callback).Run(
      blink::mojom::CanCreateTranslatorResult::kNoNotSupportedLanguage);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void TranslationManagerImpl::CreateTranslator(
    const std::string& source_lang,
    const std::string& target_lang,
    mojo::PendingReceiver<blink::mojom::Translator> receiver,
    CreateTranslatorCallback callback) {
  // The API is not supported on Android yet.
#if !BUILDFLAG(IS_ANDROID)
  CHECK(browser_context_);
  if (!PassAcceptLanguagesCheck(
          Profile::FromBrowserContext(browser_context_.get())
              ->GetPrefs()
              ->GetString(language::prefs::kAcceptLanguages),
          source_lang, target_lang)) {
    std::move(callback).Run(false);
    return;
  }
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<Translator>(source_lang, target_lang,
                                   std::move(callback)),
      std::move(receiver));
#else
  std::move(callback).Run(false);
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
// static
bool TranslationManagerImpl::PassAcceptLanguagesCheck(
    const std::string& accept_languages_str,
    const std::string& source_lang,
    const std::string& target_lang) {
  if (!on_device_translation::kTranslationAPIAcceptLanguagesCheck.Get()) {
    return true;
  }
  // When the TranslationAPIAcceptLanguagesCheck feature is enabled, the
  // Translation API will fail if neither the source nor destination language is
  // in the AcceptLanguages. This is intended to mitigate privacy concerns.
  const std::vector<std::string_view> accept_languages =
      base::SplitStringPiece(accept_languages_str, ",", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  // TODO(crbug.com/371899260): Implement better language code handling.

  // One of the source or the destination language must be in the user's accept
  // language.
  const bool source_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, source_lang);
  const bool target_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, target_lang);
  if (!(source_lang_is_in_accept_langs || target_lang_is_in_accept_langs)) {
    return false;
  }

  // The other language must be a popular language.
  if (!source_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(source_lang)) {
    return false;
  }
  if (!target_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(target_lang)) {
    return false;
  }
  return true;
}
#endif  // !BUILDFLAG(IS_ANDROID)
