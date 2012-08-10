// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/layout.h"

#include <Cocoa/Cocoa.h>

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
@end

@interface NSWindow (LionAPI)
- (CGFloat)backingScaleFactor;
@end

namespace {

std::vector<ui::ScaleFactor>& GetSupportedScaleFactorsInternal() {
  static std::vector<ui::ScaleFactor>* supported_scale_factors =
      new std::vector<ui::ScaleFactor>();
  if (supported_scale_factors->empty()) {
    supported_scale_factors->push_back(ui::SCALE_FACTOR_100P);
    supported_scale_factors->push_back(ui::SCALE_FACTOR_200P);
  }
  return *supported_scale_factors;
}

float GetScaleFactorScaleForNativeView(gfx::NativeView view) {
  if (NSWindow* window = [view window]) {
    if ([window respondsToSelector:@selector(backingScaleFactor)])
      return [window backingScaleFactor];
    return [window userSpaceScaleFactor];
  }

  NSArray* screens = [NSScreen screens];
  if (![screens count])
    return 1.0f;

  NSScreen* screen = [screens objectAtIndex:0];
  if ([screen respondsToSelector:@selector(backingScaleFactor)])
    return [screen backingScaleFactor];
  return [screen userSpaceScaleFactor];
}

}  // namespace

namespace ui {

ScaleFactor GetScaleFactorForNativeView(gfx::NativeView view) {
  return GetScaleFactorFromScale(GetScaleFactorScaleForNativeView(view));
}

std::vector<ScaleFactor> GetSupportedScaleFactors() {
  return GetSupportedScaleFactorsInternal();
}

namespace test {

void SetSupportedScaleFactors(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  std::vector<ui::ScaleFactor>& supported_scale_factors =
      GetSupportedScaleFactorsInternal();
  supported_scale_factors = scale_factors;
}

}  // namespace test

}  // namespace ui
