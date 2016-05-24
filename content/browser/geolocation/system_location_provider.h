// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "content/browser/geolocation/location_provider_base.h"
#include "content/common/content_export.h"
#include "content/public/common/geoposition.h"

#if defined(OS_WIN)
#include "content/browser/geolocation/system_location_provider_win.h"
#elif defined(OS_MACOSX)
#include "content/browser/geolocation/system_location_provider_mac.h"
#endif

namespace content {

// Factory functions for the various types of location provider to abstract
// over the platform-dependent implementations.
CONTENT_EXPORT LocationProvider* NewSystemLocationProvider();
}  // namespace content

#endif  //CONTENT_BROWSER_GEOLOCATION_SYSTEM_LOCATION_PROVIDER_H_
