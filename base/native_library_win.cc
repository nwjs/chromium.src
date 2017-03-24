// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/native_library.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/iat_patch_function.h"
#include "chrome/common/chrome_paths.h"

namespace base {

typedef HMODULE (WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

namespace {

base::win::IATPatchFunction* FlashCreateProcessProxy = nullptr;

BOOL WINAPI CreateProcessAForFlash(
  _In_opt_ LPCSTR lpApplicationName,
  _Inout_opt_ LPSTR lpCommandLine,
  _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
  _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_ BOOL bInheritHandles,
  _In_ DWORD dwCreationFlags,
  _In_opt_ LPVOID lpEnvironment,
  _In_opt_ LPCSTR lpCurrentDirectory,
  _In_ LPSTARTUPINFOA lpStartupInfo,
  _Out_ LPPROCESS_INFORMATION lpProcessInformation) {
  bool unhook = false;
  if (FlashCreateProcessProxy != nullptr &&
    strstr(lpCommandLine, "cmd.exe /c echo NOT SANDBOXED") != NULL) {
    unhook = true;
    dwCreationFlags |= CREATE_NO_WINDOW;
  }

  typedef BOOL(WINAPI *CREATE_PROC) (
    LPCSTR,
    LPSTR,
    LPSECURITY_ATTRIBUTES,
    LPSECURITY_ATTRIBUTES,
    BOOL,
    DWORD,
    LPVOID,
    LPCSTR,
    LPSTARTUPINFOA,
    LPPROCESS_INFORMATION
    );

  if (FlashCreateProcessProxy != nullptr) {
    CREATE_PROC createProc = (CREATE_PROC)FlashCreateProcessProxy->original_function();
    BOOL retVal = createProc(
      lpApplicationName,
      lpCommandLine,
      lpProcessAttributes,
      lpThreadAttributes,
      bInheritHandles,
      dwCreationFlags,
      lpEnvironment,
      lpCurrentDirectory,
      lpStartupInfo,
      lpProcessInformation
    );

    if (unhook) {
      DWORD lastError = GetLastError();
      delete FlashCreateProcessProxy;
      FlashCreateProcessProxy = nullptr;
      SetLastError(lastError);
    }
    return retVal;
  }
  return FALSE;
}

bool IsFlash(const FilePath& library_path) {
  base::FilePath flash_filename;
  if (!PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN,
    &flash_filename))
    return false;

  return flash_filename == library_path;
}

NativeLibrary LoadNativeLibraryHelper(const FilePath& library_path,
                                      LoadLibraryFunction load_library_api,
                                      NativeLibraryLoadError* error) {
  // LoadLibrary() opens the file off disk.
  ThreadRestrictions::AssertIOAllowed();

  // Switch the current directory to the library directory as the library
  // may have dependencies on DLLs in this directory.
  bool restore_directory = false;
  FilePath current_directory;
  if (GetCurrentDirectory(&current_directory)) {
    FilePath plugin_path = library_path.DirName();
    if (!plugin_path.empty()) {
      SetCurrentDirectory(plugin_path);
      restore_directory = true;
    }
  }

  HMODULE module = (*load_library_api)(library_path.value().c_str());
  if (!module && error) {
    // GetLastError() needs to be called immediately after |load_library_api|.
    error->code = GetLastError();
  }

  if (module) {
    if (IsFlash(library_path)) {
      FlashCreateProcessProxy = new base::win::IATPatchFunction;
      if (NO_ERROR != 
        FlashCreateProcessProxy->Patch(library_path.value().c_str(), 
                                       "kernel32.dll", 
                                       "CreateProcessA", 
                                       CreateProcessAForFlash)) {
        delete FlashCreateProcessProxy;
        FlashCreateProcessProxy = nullptr;
      }
    }
  }

  if (restore_directory)
    SetCurrentDirectory(current_directory);

  return module;
}

}  // namespace

std::string NativeLibraryLoadError::ToString() const {
  return StringPrintf("%u", code);
}

// static
NativeLibrary LoadNativeLibraryWithOptions(const FilePath& library_path,
                                           const NativeLibraryOptions& options,
                                           NativeLibraryLoadError* error) {
  return LoadNativeLibraryHelper(library_path, LoadLibraryW, error);
}

NativeLibrary LoadNativeLibraryDynamically(const FilePath& library_path) {
  typedef HMODULE (WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

  LoadLibraryFunction load_library = reinterpret_cast<LoadLibraryFunction>(
      GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW"));

  return LoadNativeLibraryHelper(library_path, load_library, NULL);
}

// static
void UnloadNativeLibrary(NativeLibrary library) {
  FreeLibrary(library);
}

// static
void* GetFunctionPointerFromNativeLibrary(NativeLibrary library,
                                          StringPiece name) {
  return GetProcAddress(library, name.data());
}

// static
std::string GetNativeLibraryName(StringPiece name) {
  DCHECK(IsStringASCII(name));
  return name.as_string() + ".dll";
}

}  // namespace base
