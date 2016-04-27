// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/mac/dock.h"

namespace dock {
  AddIconStatus AddIcon(NSString* installed_path, NSString* dmg_app_path) {
    return IconAddFailure;
  }
}  // namespace dock