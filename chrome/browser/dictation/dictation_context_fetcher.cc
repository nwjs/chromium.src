// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context_fetcher.h"

#include "base/functional/bind.h"
#include "chrome/browser/dictation/target.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"

namespace dictation {

DictationContextFetcher::DictationContextFetcher() = default;
DictationContextFetcher::~DictationContextFetcher() = default;

void DictationContextFetcher::Fetch(const Target& target,
                                    GetContextCallback callback) {
  content::RenderFrameHost* rfh = target.GetRenderFrameHost();
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    DictationContext context;
    context.editable_content = target.GetSelectedText();
    std::move(callback).Run(std::move(context));
    return;
  }

  auto options = optimization_guide::DefaultAIPageContentOptions(
      /*on_critical_path=*/true);
  options->include_same_site_only = true;

  optimization_guide::GetAIPageContent(
      web_contents, std::move(options),
      base::BindOnce(&DictationContextFetcher::OnPageContentCaptured,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     target.GetSelectedText()));
}

void DictationContextFetcher::OnPageContentCaptured(
    GetContextCallback callback,
    const std::string& editable_content,
    base::expected<optimization_guide::AIPageContentResult, std::string>
        result) {
  DictationContext context;
  context.editable_content = editable_content;
  if (result.has_value()) {
    context.annotated_page_content = std::move(result->proto);
  }
  // TODO(b/525847081): Implement innerText
  std::move(callback).Run(std::move(context));
}

}  // namespace dictation
