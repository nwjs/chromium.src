// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt.h"

namespace sandbox {

// static
int Seatbelt::Init(const char* profile, uint64_t flags, char** errorbuf) {
  *errorbuf = NULL;
  return 0;
}

// static
int Seatbelt::InitWithParams(const char* profile,
                             uint64_t flags,
                             const char* const parameters[],
                             char** errorbuf) {
  *errorbuf = NULL;
  return 0;
}

// static
void Seatbelt::FreeError(char* errorbuf) {
}

}  // namespace sandbox
