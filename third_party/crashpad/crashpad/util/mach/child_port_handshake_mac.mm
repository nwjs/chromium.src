// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/mach/child_port_handshake.h"

#include "base/base_paths.h"
#include "base/logging.h"
#import "base/mac/bundle_locations.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsautorelease_pool.h"
#include "base/path_service.h"

namespace crashpad {

// Bundle ID is not set at the time of starting crashpad.
// It has to be manually loaded with this method.
const char* GetOuterBundleId() {
  base::mac::ScopedNSAutoreleasePool pool;

  base::FilePath outer_app_dir;
  PathService::Get(base::FILE_EXE, &outer_app_dir);

  // Contents/Versions/<ChromiumVersion>/nwjs Framework.framework/
  // Versions/A/Helpers/crashpad_handler
  outer_app_dir = outer_app_dir
    .DirName().DirName().DirName().DirName() // nwjs Framework.framework
    .DirName().DirName().DirName(); // Contents
  DCHECK_EQ(outer_app_dir.BaseName().value(), "Contents");

  NSString* outer_app_dir_ns = 
    [NSString stringWithUTF8String:outer_app_dir.DirName().value().c_str()];
  NSBundle* base_bundle = [[NSBundle bundleWithPath:outer_app_dir_ns] retain];
  CHECK(base_bundle);
  return [[base_bundle bundleIdentifier] UTF8String];
}

} // namespace crashpad