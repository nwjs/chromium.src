// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_USER_AGENT_USER_AGENT_UTIL_H_
#define WEBKIT_USER_AGENT_USER_AGENT_UTIL_H_

#include <string>

#include "base/basictypes.h"
<<<<<<< HEAD:webkit/glue/user_agent.h
#include "webkit/glue/webkit_glue_export.h"
=======
#include "webkit/user_agent/webkit_user_agent_export.h"
>>>>>>> 69bfc514fb662ef58d1b0b0d87f515ff90cb69f5:webkit/user_agent/user_agent_util.h

namespace webkit_glue {

// Builds a User-agent compatible string that describes the OS and CPU type.
<<<<<<< HEAD:webkit/glue/user_agent.h
WEBKIT_GLUE_EXPORT std::string BuildOSCpuInfo();

// Returns the WebKit version, in the form "major.minor (branch@revision)".
WEBKIT_GLUE_EXPORT std::string GetWebKitVersion();

// The following 2 functions return the major and minor webkit versions.
WEBKIT_GLUE_EXPORT int GetWebKitMajorVersion();
WEBKIT_GLUE_EXPORT int GetWebKitMinorVersion();
=======
WEBKIT_USER_AGENT_EXPORT std::string BuildOSCpuInfo();

// Returns the WebKit version, in the form "major.minor (branch@revision)".
WEBKIT_USER_AGENT_EXPORT std::string GetWebKitVersion();

// The following 2 functions return the major and minor webkit versions.
WEBKIT_USER_AGENT_EXPORT int GetWebKitMajorVersion();
WEBKIT_USER_AGENT_EXPORT int GetWebKitMinorVersion();
>>>>>>> 69bfc514fb662ef58d1b0b0d87f515ff90cb69f5:webkit/user_agent/user_agent_util.h

// Helper function to generate a full user agent string from a short
// product name.
WEBKIT_USER_AGENT_EXPORT std::string BuildUserAgentFromProduct(
    const std::string& product);

// Builds a full user agent string given a string describing the OS and a
// product name.
WEBKIT_USER_AGENT_EXPORT std::string BuildUserAgentFromOSAndProduct(
    const std::string& os_info,
    const std::string& product);

}  // namespace webkit_glue

#endif  // WEBKIT_USER_AGENT_USER_AGENT_UTIL_H_
