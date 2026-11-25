// Copyright 2026 The NW.js Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NW.js workaround for Windows on Arm (arm64) C++ builds.
//
// Since LLVM PR #227722 (clang 24), <arm_vector_types.h> declares the Arm FPM
// builtins __arm_set_fpm_lscale/__arm_set_fpm_nscale/__arm_set_fpm_lscale2 as
// bare prototypes of compiler builtins (previously they were static inline
// definitions). Those prototypes are not wrapped in extern "C", so a C++
// translation unit sees them with C++ language linkage while the builtins have
// C linkage, producing a hard error:
//
//   error: declaration of '__arm_set_fpm_lscale' has a different language
//          linkage
//   note: '__arm_set_fpm_lscale' is a builtin with type 'void ()'
//
// That header is now reachable from almost every C++ TU on Windows/arm64,
// because the Windows SDK's UCRT <wchar.h> (>= 10.0.26100.1882) includes
// <arm_neon.h> unconditionally for its vectorized wmemchr/wmemcmp (the same
// change that makes UCRT pull in <intrin.h> on x86). libc++ <cwchar>, reached
// through <algorithm>, is enough to trigger it.
//
// Including <arm_neon.h> once here, inside an extern "C" block and before any
// other header, gives the declarations C linkage (matching the builtins). Its
// include guard then makes the later UCRT include a no-op, so the conflicting
// C++-linkage declarations are never parsed.
//
// This header is force-included only for C++ compilations with
// is_win && target_cpu == "arm64"; see //build/config/compiler/BUILD.gn.

#ifndef BUILD_WIN_NW_ARM64_NEON_C_LINKAGE_H_
#define BUILD_WIN_NW_ARM64_NEON_C_LINKAGE_H_

#if defined(_M_ARM64) || defined(__aarch64__)
extern "C" {
#include <arm_neon.h>
}  // extern "C"
#endif

#endif  // BUILD_WIN_NW_ARM64_NEON_C_LINKAGE_H_
