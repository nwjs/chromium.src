// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides forward declarations for XPC symbols that are not
// present in the 10.6 SDK. It uses generate_stubs to produce code to
// dynamically load the libxpc.dylib library and set up a stub table, with
// the same names as the real XPC functions.

#ifndef SANDBOX_MAC_XPC_H_
#define SANDBOX_MAC_XPC_H_

#include <AvailabilityMacros.h>
#include <mach/mach.h>

#include "sandbox/sandbox_export.h"

// Declares XPC object types. This includes <xpc/xpc.h> if available.
#include "sandbox/mac/xpc_stubs_header.fragment"

#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

// C++ library loader.
#include "sandbox/mac/xpc_stubs.h"

extern "C" {
// Signatures for XPC public functions that are loaded by xpc_stubs.h.
#include "sandbox/mac/xpc_stubs.sig"
#if !defined(NWJS_MAS)
// Signatures for private XPC functions.
#include "sandbox/mac/xpc_private_stubs.sig"
#endif
}  // extern "C"

#else

// Signatures for private XPC functions.
extern "C" {
#if !defined(NWJS_MAS)
#include "sandbox/mac/xpc_private_stubs.sig"
#endif
}  // extern "C"

#endif

namespace sandbox {

// Dynamically loads the XPC library.
bool SANDBOX_EXPORT InitializeXPC();

}  // namespace sandbox

#endif  // SANDBOX_MAC_XPC_H_
