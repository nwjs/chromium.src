// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_main.h"

#include "components/crash/content/app/crash_reporter_client.h"
#include <assert.h>
#include <windows.h>

#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome/install_static/user_data_dir.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/crash/crash_helper.h"

extern std::wstring g_nwjs_prod_name, g_nwjs_prod_version;
// This function is a temporary workaround for https://crbug.com/655788. We
// need to come up with a better way to initialize crash reporting that can
// happen inside DllMain().
void SignalInitializeCrashReporting(void* prod_name, void* prod_version) {
  if (prod_name) g_nwjs_prod_name = *(std::wstring*)prod_name;
  if (prod_version) g_nwjs_prod_version = *(std::wstring*)prod_version;
  if (prod_name || prod_version) {
	  //install_static::InitializeProductDetailsForPrimaryModule();
#if 0
	  std::wstring user_data_dir, invalid_user_data_dir;
	  install_static::PrimaryInstallDetails* details = (install_static::PrimaryInstallDetails*)&install_static::InstallDetails::Get();
	  const install_static::InstallDetails::Payload* payload = details->GetPayload();
	  install_static::DeriveUserDataDirectory(*payload->mode, &user_data_dir, &invalid_user_data_dir);
	  details->set_user_data_dir(user_data_dir);
	  details->set_invalid_user_data_dir(invalid_user_data_dir);
#endif
  }
  if (!elf_crash::InitializeCrashReporting()) {
#ifdef _DEBUG
    assert(false);
#endif  // _DEBUG
  }
}

void SignalChromeElf() {
  blacklist::ResetBeacon();
}

bool GetUserDataDirectoryThunk(wchar_t* user_data_dir,
                               size_t user_data_dir_length,
                               wchar_t* invalid_user_data_dir,
                               size_t invalid_user_data_dir_length) {
  std::wstring user_data_dir_str, invalid_user_data_dir_str;
  bool ret = install_static::GetUserDataDirectory(&user_data_dir_str,
                                                  &invalid_user_data_dir_str);
  assert(ret);
  install_static::IgnoreUnused(ret);
  wcsncpy_s(user_data_dir, user_data_dir_length, user_data_dir_str.c_str(),
            _TRUNCATE);
  wcsncpy_s(invalid_user_data_dir, invalid_user_data_dir_length,
            invalid_user_data_dir_str.c_str(), _TRUNCATE);

  return true;
}

void* ElfGetReporterClient() {
  return crash_reporter::GetCrashReporterClient();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    install_static::InitializeProductDetailsForPrimaryModule();

    // CRT on initialization installs an exception filter which calls
    // TerminateProcess. We need to hook CRT's attempt to set an exception.
#if 0 ////disable this or NW will fail with Enigma VB
    elf_crash::DisableSetUnhandledExceptionFilter();
#endif

    install_static::InitializeProcessType();

    __try {
      blacklist::Initialize(false);  // Don't force, abort if beacon is present.
    } __except (elf_crash::GenerateCrashDump(GetExceptionInformation())) {
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    elf_crash::ShutdownCrashReporting();
  }
  return TRUE;
}

void DumpProcessWithoutCrash() {
  elf_crash::DumpWithoutCrashing();
}
