// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_mode.h"

// Various constants used throughout the Read Anything feature.
namespace {

// |ui::AXMode::kHTML| is needed for URL information.
// |ui::AXMode::kScreenReader| is needed for heading level information.
const ui::AXMode kReadAnythingAXMode =
    ui::AXMode::kWebContents | ui::AXMode::kHTML | ui::AXMode::kScreenReader;

// Visual constants for Read Anything feature.
const int kInternalInsets = 8;
const int kSeparatorTopBottomPadding = 4;
const int kMinimumComboboxWidth = 110;

const int kButtonPadding = 4;
const int kIconSize = 16;
const int kColorsIconSize = 24;

const char kReadAnythingDefaultFontName[] = "Standard font";

// Font size in em
const double kReadAnythingDefaultFontScale = 1;
const double kReadAnythingMinimumFontScale = 0.5;
const double kReadAnythingMaximumFontScale = 4.5;
const double kReadAnythingFontScaleIncrement = 0.25;

}  // namespace

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_CONSTANTS_H_
