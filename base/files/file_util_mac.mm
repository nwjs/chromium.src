// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include <copyfile.h>
#include <sys/un.h>
#import <Foundation/Foundation.h>

#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace base {

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  ThreadRestrictions::AssertIOAllowed();
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;
  return (copyfile(from_path.value().c_str(),
                   to_path.value().c_str(), NULL, COPYFILE_DATA) == 0);
}

bool GetTempDir(base::FilePath* path) {
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil)
    return false;
  *path = base::mac::NSStringToFilePath(tmp);
  return true;
}

FilePath GetHomeDir() {
  NSString* tmp = NSHomeDirectory();
  if (tmp != nil) {
    FilePath mac_home_dir = base::mac::NSStringToFilePath(tmp);
    if (!mac_home_dir.empty())
      return mac_home_dir;
  }

  // Fall back on temp dir if no home directory is defined.
  FilePath rv;
  if (GetTempDir(&rv))
    return rv;

  // Last resort.
  return FilePath("/tmp");
}

#if defined(OS_MACOSX)

SanitizedSocketPath::SanitizedSocketPath (const base::FilePath& socket_path)
    : socket_path_(socket_path) {
  if (socket_path.value().length() >= arraysize(sockaddr_un::sun_path)) {
    bool found_current_dir = GetCurrentDirectory(&old_path_);
    CHECK(found_current_dir) << "Failed to determine the current directory.";
    changed_directory_ = SetCurrentDirectory(socket_path.DirName());
    CHECK(changed_directory_) << "Failed to change directory: " <<
        socket_path.DirName().value();
  }
}

SanitizedSocketPath::~SanitizedSocketPath() {
  if (changed_directory_)
    SetCurrentDirectory(old_path_);
}

base::FilePath SanitizedSocketPath::SocketPath() const {
  return changed_directory_ ? socket_path_.BaseName() : socket_path_;
}

#endif // NWJS_MAS

}  // namespace base
