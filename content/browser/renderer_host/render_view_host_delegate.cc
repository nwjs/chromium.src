// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_view_host_delegate.h"

namespace content {

RenderViewHostDelegate::RenderViewHostDelegate()
  :skip_blocking_parser_(true) {
}

RenderViewHostDelegateView* RenderViewHostDelegate::GetDelegateView() {
  return nullptr;
}

bool RenderViewHostDelegate::IsNeverComposited() {
  return false;
}

bool RenderViewHostDelegate::IsJavaScriptDialogShowing() const {
  return false;
}

bool RenderViewHostDelegate::ShouldIgnoreUnresponsiveRenderer() {
  return false;
}

bool RenderViewHostDelegate::IsGuest() {
  return false;
}

absl::optional<SkColor> RenderViewHostDelegate::GetBaseBackgroundColor() {
  return absl::nullopt;
}

bool RenderViewHostDelegate::GetSkipBlockingParser() {
  return skip_blocking_parser_;
}

void RenderViewHostDelegate::SetSkipBlockingParser(bool value) {
  skip_blocking_parser_ = value;
}
}  // namespace content
