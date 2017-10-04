// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_CHROME_ELF_MAIN_H_
#define CHROME_ELF_CHROME_ELF_MAIN_H_

extern "C" void SignalInitializeCrashReporting(void*, void*);
extern "C" void SignalChromeElf();
extern "C" void DumpProcessWithoutCrash();
extern "C" void* ElfGetReporterClient();

#endif  // CHROME_ELF_CHROME_ELF_MAIN_H_
