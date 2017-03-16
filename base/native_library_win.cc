// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2017 The NW.js community. Author: Jan Rucka
// <Jan.Rucka@firma.seznam.cz>

#include "base/native_library.h"

#include <windows.h>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/logging_chrome.h"

namespace base {

typedef HMODULE (WINAPI* LoadLibraryFunction)(const wchar_t* file_name);

namespace {

PROC g_orig_create_process = NULL;
HINSTANCE flash_dll;

bool IsValidDosHeader(PIMAGE_DOS_HEADER dosHeader) {
  if (IsBadReadPtr(dosHeader, sizeof(IMAGE_DOS_HEADER)))
    return false;

  if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
    return false;

  return true;
}

bool IsValidNTHeader(PIMAGE_NT_HEADERS ntHeader) {
  if (IsBadReadPtr(ntHeader, sizeof(PIMAGE_NT_HEADERS)))
    return false;

  if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
    return false;

  return true;
}

BOOL WINAPI ReplaceFunctionAddressInModule(HMODULE module, PROC origFunc, PROC newFunc) {
  if (!module || !newFunc || !origFunc || IsBadCodePtr(newFunc))
    return FALSE;

  PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)module;
  if (!IsValidDosHeader(dosHeader))
    return FALSE;

  // Find the NT Header by using the offset of e_lfanew value from hMod
  PIMAGE_NT_HEADERS ntHeader = (PIMAGE_NT_HEADERS)((DWORD)dosHeader + (DWORD)dosHeader->e_lfanew);
  if (!IsValidNTHeader(ntHeader))
    return FALSE;

  PIMAGE_IMPORT_DESCRIPTOR importDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)dosHeader +
    (DWORD)(ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));

  if (importDesc == (PIMAGE_IMPORT_DESCRIPTOR)ntHeader)
    return FALSE;

  while (importDesc->Name) {
    char *moduleName = (char *)((DWORD)dosHeader + (DWORD)(importDesc->Name));
    if (_stricmp(moduleName, "kernel32.dll") == 0) {
      PIMAGE_THUNK_DATA thunkData = (PIMAGE_THUNK_DATA)((DWORD)dosHeader + (DWORD)importDesc->FirstThunk);
      while (thunkData->u1.Function) {
        // get the pointer of the imported function and see if it matches up with the original
        if ((DWORD)thunkData->u1.Function == (DWORD)origFunc) {
          MEMORY_BASIC_INFORMATION mbi;
          DWORD oldProt;
          VirtualQuery(&thunkData->u1.Function, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
          VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_READWRITE, &oldProt);
          thunkData->u1.Function = (DWORD)newFunc;
          VirtualProtect(mbi.BaseAddress, mbi.RegionSize, oldProt, &oldProt);
          break;
        } else {
          ++thunkData;
        }
      }
    }
    ++importDesc;
  }
  return TRUE;
}

BOOL WINAPI MyCreateProcessA(
  _In_opt_ LPCSTR lpApplicationName,
  _Inout_opt_ LPSTR lpCommandLine,
  _In_opt_ LPSECURITY_ATTRIBUTES lpProcessAttributes,
  _In_opt_ LPSECURITY_ATTRIBUTES lpThreadAttributes,
  _In_ BOOL bInheritHandles,
  _In_ DWORD dwCreationFlags,
  _In_opt_ LPVOID lpEnvironment,
  _In_opt_ LPCSTR lpCurrentDirectory,
  _In_ LPSTARTUPINFOA lpStartupInfo,
  _Out_ LPPROCESS_INFORMATION lpProcessInformation
  ) {
  if (g_orig_create_process != NULL &&
      strstr(lpCommandLine, "cmd.exe /c echo NOT SANDBOXED") != NULL) {
    LOG(INFO) << "Prevent flash from calling: echo NOT SANDBOXED";
    HINSTANCE libInstance = LoadLibrary(TEXT("kernel32.dll"));
    g_orig_create_process = GetProcAddress(libInstance, "CreateProcessA");
    ReplaceFunctionAddressInModule(flash_dll,
      GetProcAddress(libInstance, "CreateProcessA"), g_orig_create_process);
    g_orig_create_process = NULL;
    return TRUE;
  }

  typedef BOOL(*CREATE_PROC) (
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

  CREATE_PROC createProc = (CREATE_PROC)g_orig_create_process;
  return createProc(
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
}

void SetCreateProcessProxy(HINSTANCE module) {
  HINSTANCE libInstance = LoadLibrary(TEXT("kernel32.dll"));
  g_orig_create_process = GetProcAddress(libInstance, "CreateProcessA");
  ReplaceFunctionAddressInModule(module, GetProcAddress(libInstance, "CreateProcessA"), PROC(MyCreateProcessA));
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
      flash_dll = module;
      // to prevent flickering
      SetCreateProcessProxy(module);
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
