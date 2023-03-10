// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PRELOADING_H_
#define CONTENT_BROWSER_PRELOADING_PRELOADING_H_

#include "content/public/browser/preloading.h"

// Defines various //content triggering mechanisms which trigger different
// preloading operations mentioned in content/public/browser/preloading.h.

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.

// If you change this, please follow the process in
// go/preloading-dashboard-updates to update the mapping reflected in
// dashboard, or if you are not a Googler, please file an FYI bug on
// https://crbug.new with component Internals>Preload.
namespace content::content_preloading_predictor {

// Advance numbering by +1 when adding a new element.
//
// Please limit content-internal `PreloadingPredictor` between 50 to 99
// (inclusive) as 0 to 49 are reserved for content-public definitions, and 100
// and beyond are reserved for embedder definitions. Both the value and the name
// should be unique across all the namespaces.

// This API allows an origin to list possible navigation URLs that the user
// might navigate to in order to perform preloading operations.
// For more details please see:
// https://wicg.github.io/nav-speculation/prerendering.html#speculation-rules
static constexpr PreloadingPredictor kSpeculationRules(50, "SpeculationRules");

// TODO(crbug.com/1309934): Add more predictors as we integrate Preloading
// logging.
}  // namespace content::content_preloading_predictor

#endif  // CONTENT_BROWSER_PRELOADING_PRELOADING_H_
