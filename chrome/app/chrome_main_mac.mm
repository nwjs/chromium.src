// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_main_mac.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#import "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/chrome_paths_internal.h"

void SetUpBundleOverrides() {
  base::mac::ScopedNSAutoreleasePool pool;

  base::mac::SetOverrideFrameworkBundlePath(chrome::GetFrameworkBundlePath());

  NSBundle* base_bundle = chrome::OuterAppBundle();

  NSString* base_bundle_id = [base_bundle bundleIdentifier];

  // MAS: bundle id is used as the name of mach IPC.
  // The original implementation requires the app to be entitled with 
  // [Global Mach Service Temporary Exception][1], which might be rejected by the
  // reviewer.
  // So we made use of an custom `NWTeamID` property in Info.plist and
  // `com.apple.security.application-groups` entitlement to workaround.
  // See [IPC and POSIX Semaphores and Shared Memory][2] for details.
  // 
  // [1]: https://developer.apple.com/library/content/documentation/Miscellaneous/Reference/EntitlementKeyReference/Chapters/AppSandboxTemporaryExceptionEntitlements.html#//apple_ref/doc/uid/TP40011195-CH5-SW6
  // [2]: https://developer.apple.com/library/content/documentation/Security/Conceptual/AppSandboxDesignGuide/AppSandboxInDepth/AppSandboxInDepth.html#//apple_ref/doc/uid/TP40011183-CH3-SW24
  NSString* team_id = [base_bundle objectForInfoDictionaryKey:@"NWTeamID"];
  if (team_id) {
    base_bundle_id = [NSString stringWithFormat:@"%@.%@", team_id, base_bundle_id];
  }

  base::mac::SetBaseBundleID([base_bundle_id UTF8String]);
}
