// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/win_util.h"

#include <aclapi.h>
#include <cfgmgr32.h>
#include <lm.h>
#include <powrprof.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>  // Must be before propkey.
#include <initguid.h>
#include <inspectable.h>
#include <propkey.h>
#include <propvarutil.h>
#include <psapi.h>
#include <roapi.h>
#include <sddl.h>
#include <setupapi.h>
#include <signal.h>
#include <stddef.h>
#include <stdlib.h>
#include <tchar.h> // Must be before tpcshrd.h or for any use of _T macro
#include <tpcshrd.h>
#include <uiviewsettingsinterop.h>
#include <windows.ui.viewmanagement.h>
#include <winstring.h>
#include <wrl/wrappers/corewrappers.h>

#include <memory>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

// Sets the value of |property_key| to |property_value| in |property_store|.
bool SetPropVariantValueForPropertyStore(
    IPropertyStore* property_store,
    const PROPERTYKEY& property_key,
    const ScopedPropVariant& property_value) {
  DCHECK(property_store);

  HRESULT result = property_store->SetValue(property_key, property_value.get());
  if (result == S_OK)
    result = property_store->Commit();
  return SUCCEEDED(result);
}

void __cdecl ForceCrashOnSigAbort(int) {
  *((volatile int*)0) = 0x1337;
}

typedef decltype(GetProcessMitigationPolicy)* GetProcessMitigationPolicyType;

class LazyIsUser32AndGdi32Available {
 public:
  LazyIsUser32AndGdi32Available() : value_(!IsWin32kSyscallsDisabled()) {}

  ~LazyIsUser32AndGdi32Available() {}

  bool value() { return value_; }

 private:
  static bool IsWin32kSyscallsDisabled() {
    // Can't disable win32k prior to windows 8.
    if (base::win::GetVersion() < base::win::VERSION_WIN8)
      return false;

    GetProcessMitigationPolicyType get_process_mitigation_policy_func =
        reinterpret_cast<GetProcessMitigationPolicyType>(GetProcAddress(
            GetModuleHandle(L"kernel32.dll"), "GetProcessMitigationPolicy"));

    if (!get_process_mitigation_policy_func)
      return false;

    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
    if (get_process_mitigation_policy_func(GetCurrentProcess(),
                                           ProcessSystemCallDisablePolicy,
                                           &policy, sizeof(policy))) {
      return policy.DisallowWin32kSystemCalls != 0;
    }

    return false;
  }

  const bool value_;

  DISALLOW_COPY_AND_ASSIGN(LazyIsUser32AndGdi32Available);
};

const wchar_t kWindows8OSKRegPath[] =
    L"Software\\Classes\\CLSID\\{054AAE20-4BEA-4347-8A35-64A533254A9D}"
    L"\\LocalServer32";

// Returns the current platform role. We use the PowerDeterminePlatformRoleEx
// API for that.
POWER_PLATFORM_ROLE GetPlatformRole() {
  return PowerDeterminePlatformRoleEx(POWER_PLATFORM_ROLE_V2);
}

}  // namespace

// Uses the Windows 10 WRL API's to query the current system state. The API's
// we are using in the function below are supported in Win32 apps as per msdn.
// It looks like the API implementation is buggy at least on Surface 4 causing
// it to always return UserInteractionMode_Touch which as per documentation
// indicates tablet mode.
bool IsWindows10TabletMode(HWND hwnd) {
  if (GetVersion() < VERSION_WIN10)
    return false;

  using RoGetActivationFactoryFunction = decltype(&RoGetActivationFactory);
  using WindowsCreateStringFunction = decltype(&WindowsCreateString);

  static RoGetActivationFactoryFunction get_factory = nullptr;
  static WindowsCreateStringFunction create_string = nullptr;

  if (!get_factory) {
    DCHECK_EQ(create_string, static_cast<WindowsCreateStringFunction>(
        nullptr));

    HMODULE combase_dll = ::LoadLibrary(L"combase.dll");
    if (!combase_dll)
      return false;

    get_factory = reinterpret_cast<RoGetActivationFactoryFunction>(
        ::GetProcAddress(combase_dll, "RoGetActivationFactory"));
    if (!get_factory) {
      CHECK(false);
      return false;
    }

    create_string = reinterpret_cast<WindowsCreateStringFunction>(
        ::GetProcAddress(combase_dll, "WindowsCreateString"));
    if (!create_string) {
      CHECK(false);
      return false;
    }
  }

  HRESULT hr = E_FAIL;
  // This HSTRING is allocated on the heap and is leaked.
  static HSTRING view_settings_guid = NULL;
  if (!view_settings_guid) {
    hr = create_string(
        RuntimeClass_Windows_UI_ViewManagement_UIViewSettings,
        static_cast<UINT32>(
            wcslen(RuntimeClass_Windows_UI_ViewManagement_UIViewSettings)),
        &view_settings_guid);
    if (FAILED(hr))
      return false;
  }

  base::win::ScopedComPtr<IUIViewSettingsInterop> view_settings_interop;
  hr = get_factory(view_settings_guid,
                   __uuidof(IUIViewSettingsInterop),
                   view_settings_interop.ReceiveVoid());
  if (FAILED(hr))
    return false;

  base::win::ScopedComPtr<ABI::Windows::UI::ViewManagement::IUIViewSettings>
      view_settings;
  // TODO(ananta)
  // Avoid using GetForegroundWindow here and pass in the HWND of the window
  // intiating the request to display the keyboard.
  hr = view_settings_interop->GetForWindow(
      hwnd,
      __uuidof(ABI::Windows::UI::ViewManagement::IUIViewSettings),
      view_settings.ReceiveVoid());
  if (FAILED(hr))
    return false;

  ABI::Windows::UI::ViewManagement::UserInteractionMode mode =
      ABI::Windows::UI::ViewManagement::UserInteractionMode_Mouse;
  view_settings->get_UserInteractionMode(&mode);
  return mode == ABI::Windows::UI::ViewManagement::UserInteractionMode_Touch;
}

// Returns true if a physical keyboard is detected on Windows 8 and up.
// Uses the Setup APIs to enumerate the attached keyboards and returns true
// if the keyboard count is 1 or more.. While this will work in most cases
// it won't work if there are devices which expose keyboard interfaces which
// are attached to the machine.
bool IsKeyboardPresentOnSlate(std::string* reason) {
  bool result = false;

  if (GetVersion() < VERSION_WIN8) {
    *reason = "Detection not supported";
    return false;
  }

  // This function is only supported for Windows 8 and up.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableUsbKeyboardDetect)) {
    if (reason)
      *reason = "Detection disabled";
    return false;
  }

  // This function should be only invoked for machines with touch screens.
  if ((GetSystemMetrics(SM_DIGITIZER) & NID_INTEGRATED_TOUCH)
        != NID_INTEGRATED_TOUCH) {
    if (reason) {
      *reason += "NID_INTEGRATED_TOUCH\n";
      result = true;
    } else {
      return true;
    }
  }

  // If it is a tablet device we assume that there is no keyboard attached.
  if (IsTabletDevice(reason)) {
    if (reason)
      *reason += "Tablet device.\n";
    return false;
  } else {
    if (reason) {
      *reason += "Not a tablet device";
      result = true;
    } else {
      return true;
    }
  }

  // To determine whether a keyboard is present on the device, we do the
  // following:-
  // 1. Check whether the device supports auto rotation. If it does then
  //    it possibly supports flipping from laptop to slate mode. If it
  //    does not support auto rotation, then we assume it is a desktop
  //    or a normal laptop and assume that there is a keyboard.

  // 2. If the device supports auto rotation, then we get its platform role
  //    and check the system metric SM_CONVERTIBLESLATEMODE to see if it is
  //    being used in slate mode. If yes then we return false here to ensure
  //    that the OSK is displayed.

  // 3. If step 1 and 2 fail then we check attached keyboards and return true
  //    if we find ACPI\* or HID\VID* keyboards.

  typedef BOOL (WINAPI* GetAutoRotationState)(PAR_STATE state);

  GetAutoRotationState get_rotation_state =
      reinterpret_cast<GetAutoRotationState>(::GetProcAddress(
          GetModuleHandle(L"user32.dll"), "GetAutoRotationState"));

  if (get_rotation_state) {
    AR_STATE auto_rotation_state = AR_ENABLED;
    get_rotation_state(&auto_rotation_state);
    if ((auto_rotation_state & AR_NOSENSOR) ||
        (auto_rotation_state & AR_NOT_SUPPORTED)) {
      // If there is no auto rotation sensor or rotation is not supported in
      // the current configuration, then we can assume that this is a desktop
      // or a traditional laptop.
      if (reason) {
        *reason += (auto_rotation_state & AR_NOSENSOR) ? "AR_NOSENSOR\n" :
                                                         "AR_NOT_SUPPORTED\n";
        result = true;
      } else {
        return true;
      }
    }
  }

  const GUID KEYBOARD_CLASS_GUID =
      { 0x4D36E96B, 0xE325,  0x11CE,
          { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 } };

  // Query for all the keyboard devices.
  HDEVINFO device_info =
      SetupDiGetClassDevs(&KEYBOARD_CLASS_GUID, NULL, NULL, DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE) {
    if (reason)
      *reason += "No keyboard info\n";
    return result;
  }

  // Enumerate all keyboards and look for ACPI\PNP and HID\VID devices. If
  // the count is more than 1 we assume that a keyboard is present. This is
  // under the assumption that there will always be one keyboard device.
  for (DWORD i = 0;; ++i) {
    SP_DEVINFO_DATA device_info_data = { 0 };
    device_info_data.cbSize = sizeof(device_info_data);
    if (!SetupDiEnumDeviceInfo(device_info, i, &device_info_data))
      break;

    // Get the device ID.
    wchar_t device_id[MAX_DEVICE_ID_LEN];
    CONFIGRET status = CM_Get_Device_ID(device_info_data.DevInst,
                                        device_id,
                                        MAX_DEVICE_ID_LEN,
                                        0);
    if (status == CR_SUCCESS) {
      // To reduce the scope of the hack we only look for ACPI and HID\\VID
      // prefixes in the keyboard device ids.
      if (StartsWith(device_id, L"ACPI", CompareCase::INSENSITIVE_ASCII) ||
          StartsWith(device_id, L"HID\\VID", CompareCase::INSENSITIVE_ASCII)) {
        if (reason) {
          *reason += "device: ";
          *reason += WideToUTF8(device_id);
          *reason += '\n';
        }
        // The heuristic we are using is to check the count of keyboards and
        // return true if the API's report one or more keyboards. Please note
        // that this will break for non keyboard devices which expose a
        // keyboard PDO.
        result = true;
      }
    }
  }
  return result;
}

static bool g_crash_on_process_detach = false;

void GetNonClientMetrics(NONCLIENTMETRICS_XP* metrics) {
  DCHECK(metrics);
  metrics->cbSize = sizeof(*metrics);
  const bool success = !!SystemParametersInfo(
      SPI_GETNONCLIENTMETRICS,
      metrics->cbSize,
      reinterpret_cast<NONCLIENTMETRICS*>(metrics),
      0);
  DCHECK(success);
}

bool GetUserSidString(std::wstring* user_sid) {
  // Get the current token.
  HANDLE token = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    return false;
  ScopedHandle token_scoped(token);

  DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
  std::unique_ptr<BYTE[]> user_bytes(new BYTE[size]);
  TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

  if (!::GetTokenInformation(token, TokenUser, user, size, &size))
    return false;

  if (!user->User.Sid)
    return false;

  // Convert the data to a string.
  wchar_t* sid_string;
  if (!::ConvertSidToStringSid(user->User.Sid, &sid_string))
    return false;

  *user_sid = sid_string;

  ::LocalFree(sid_string);

  return true;
}

bool UserAccountControlIsEnabled() {
  // This can be slow if Windows ends up going to disk.  Should watch this key
  // for changes and only read it once, preferably on the file thread.
  //   http://code.google.com/p/chromium/issues/detail?id=61644
  ThreadRestrictions::ScopedAllowIO allow_io;

  RegKey key(HKEY_LOCAL_MACHINE,
             L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
             KEY_READ);
  DWORD uac_enabled;
  if (key.ReadValueDW(L"EnableLUA", &uac_enabled) != ERROR_SUCCESS)
    return true;
  // Users can set the EnableLUA value to something arbitrary, like 2, which
  // Vista will treat as UAC enabled, so we make sure it is not set to 0.
  return (uac_enabled != 0);
}

bool SetBooleanValueForPropertyStore(IPropertyStore* property_store,
                                     const PROPERTYKEY& property_key,
                                     bool property_bool_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromBoolean(property_bool_value,
                                        property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store,
                                             property_key,
                                             property_value);
}

bool SetStringValueForPropertyStore(IPropertyStore* property_store,
                                    const PROPERTYKEY& property_key,
                                    const wchar_t* property_string_value) {
  ScopedPropVariant property_value;
  if (FAILED(InitPropVariantFromString(property_string_value,
                                       property_value.Receive()))) {
    return false;
  }

  return SetPropVariantValueForPropertyStore(property_store,
                                             property_key,
                                             property_value);
}

bool SetAppIdForPropertyStore(IPropertyStore* property_store,
                              const wchar_t* app_id) {
  // App id should be less than 64 chars and contain no space. And recommended
  // format is CompanyName.ProductName[.SubProduct.ProductNumber].
  // See http://msdn.microsoft.com/en-us/library/dd378459%28VS.85%29.aspx
  DCHECK(lstrlen(app_id) < 64 && wcschr(app_id, L' ') == NULL);

  return SetStringValueForPropertyStore(property_store,
                                        PKEY_AppUserModel_ID,
                                        app_id);
}

static const char16 kAutoRunKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool AddCommandToAutoRun(HKEY root_key, const string16& name,
                         const string16& command) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.WriteValue(name.c_str(), command.c_str()) ==
      ERROR_SUCCESS);
}

bool RemoveCommandFromAutoRun(HKEY root_key, const string16& name) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_SET_VALUE);
  return (autorun_key.DeleteValue(name.c_str()) == ERROR_SUCCESS);
}

bool ReadCommandFromAutoRun(HKEY root_key,
                            const string16& name,
                            string16* command) {
  RegKey autorun_key(root_key, kAutoRunKeyPath, KEY_QUERY_VALUE);
  return (autorun_key.ReadValue(name.c_str(), command) == ERROR_SUCCESS);
}

void SetShouldCrashOnProcessDetach(bool crash) {
  g_crash_on_process_detach = crash;
}

bool ShouldCrashOnProcessDetach() {
  return g_crash_on_process_detach;
}

void SetAbortBehaviorForCrashReporting() {
  // Prevent CRT's abort code from prompting a dialog or trying to "report" it.
  // Disabling the _CALL_REPORTFAULT behavior is important since otherwise it
  // has the sideffect of clearing our exception filter, which means we
  // don't get any crash.
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

  // Set a SIGABRT handler for good measure. We will crash even if the default
  // is left in place, however this allows us to crash earlier. And it also
  // lets us crash in response to code which might directly call raise(SIGABRT)
  signal(SIGABRT, ForceCrashOnSigAbort);
}

bool IsTabletDevice(std::string* reason) {
  if (GetVersion() < VERSION_WIN8) {
    if (reason)
      *reason = "Tablet device detection not supported below Windows 8\n";
    return false;
  }

  if (IsWindows10TabletMode(::GetForegroundWindow()))
    return true;

  if (GetSystemMetrics(SM_MAXIMUMTOUCHES) == 0) {
    if (reason) {
      *reason += "Device does not support touch.\n";
    } else {
      return false;
    }
  }

  // If the device is docked, the user is treating the device as a PC.
  if (GetSystemMetrics(SM_SYSTEMDOCKED) != 0) {
    if (reason) {
      *reason += "SM_SYSTEMDOCKED\n";
    } else {
      return false;
    }
  }

  // PlatformRoleSlate was added in Windows 8+.
  POWER_PLATFORM_ROLE role = GetPlatformRole();
  bool mobile_power_profile = (role == PlatformRoleMobile);
  bool slate_power_profile = (role == PlatformRoleSlate);

  bool is_tablet = false;
  bool is_tablet_pc = false;
  if (mobile_power_profile || slate_power_profile) {
    is_tablet_pc = !GetSystemMetrics(SM_TABLETPC);
    is_tablet = !GetSystemMetrics(SM_CONVERTIBLESLATEMODE);
    if (!is_tablet) {
      if (reason) {
        *reason += "Not in slate mode.\n";
      } else {
        return false;
      }
    } else {
      if (reason) {
        *reason += (role == PlatformRoleMobile) ? "PlatformRoleMobile\n" :
                                                  "PlatformRoleSlate\n";
      }
    }
  } else {
    if (reason)
      *reason += "Device role is not mobile or slate.\n";
  }
  return is_tablet && is_tablet_pc;
}

bool DisplayVirtualKeyboard() {
  if (GetVersion() < VERSION_WIN8)
    return false;

  if (IsKeyboardPresentOnSlate(nullptr))
    return false;

  static LazyInstance<string16>::Leaky osk_path = LAZY_INSTANCE_INITIALIZER;

  if (osk_path.Get().empty()) {
    // We need to launch TabTip.exe from the location specified under the
    // LocalServer32 key for the {{054AAE20-4BEA-4347-8A35-64A533254A9D}}
    // CLSID.
    // TabTip.exe is typically found at
    // c:\program files\common files\microsoft shared\ink on English Windows.
    // We don't want to launch TabTip.exe from
    // c:\program files (x86)\common files\microsoft shared\ink. This path is
    // normally found on 64 bit Windows.
    RegKey key(HKEY_LOCAL_MACHINE, kWindows8OSKRegPath,
               KEY_READ | KEY_WOW64_64KEY);
    DWORD osk_path_length = 1024;
    if (key.ReadValue(NULL,
                      WriteInto(&osk_path.Get(), osk_path_length),
                      &osk_path_length,
                      NULL) != ERROR_SUCCESS) {
      DLOG(WARNING) << "Failed to read on screen keyboard path from registry";
      return false;
    }
    size_t common_program_files_offset =
        osk_path.Get().find(L"%CommonProgramFiles%");
    // Typically the path to TabTip.exe read from the registry will start with
    // %CommonProgramFiles% which needs to be replaced with the corrsponding
    // expanded string.
    // If the path does not begin with %CommonProgramFiles% we use it as is.
    if (common_program_files_offset != string16::npos) {
      // Preserve the beginning quote in the path.
      osk_path.Get().erase(common_program_files_offset,
                           wcslen(L"%CommonProgramFiles%"));
      // The path read from the registry contains the %CommonProgramFiles%
      // environment variable prefix. On 64 bit Windows the SHGetKnownFolderPath
      // function returns the common program files path with the X86 suffix for
      // the FOLDERID_ProgramFilesCommon value.
      // To get the correct path to TabTip.exe we first read the environment
      // variable CommonProgramW6432 which points to the desired common
      // files path. Failing that we fallback to the SHGetKnownFolderPath API.

      // We then replace the %CommonProgramFiles% value with the actual common
      // files path found in the process.
      string16 common_program_files_path;
      std::unique_ptr<wchar_t[]> common_program_files_wow6432;
      DWORD buffer_size =
          GetEnvironmentVariable(L"CommonProgramW6432", NULL, 0);
      if (buffer_size) {
        common_program_files_wow6432.reset(new wchar_t[buffer_size]);
        GetEnvironmentVariable(L"CommonProgramW6432",
                               common_program_files_wow6432.get(),
                               buffer_size);
        common_program_files_path = common_program_files_wow6432.get();
        DCHECK(!common_program_files_path.empty());
      } else {
        ScopedCoMem<wchar_t> common_program_files;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL,
                                        &common_program_files))) {
          return false;
        }
        common_program_files_path = common_program_files;
      }

      osk_path.Get().insert(1, common_program_files_path);
    }
  }

  HINSTANCE ret = ::ShellExecuteW(NULL,
                                  L"",
                                  osk_path.Get().c_str(),
                                  NULL,
                                  NULL,
                                  SW_SHOW);
  return reinterpret_cast<intptr_t>(ret) > 32;
}

bool DismissVirtualKeyboard() {
  if (GetVersion() < VERSION_WIN8)
    return false;

  // We dismiss the virtual keyboard by generating the ESC keystroke
  // programmatically.
  const wchar_t kOSKClassName[] = L"IPTip_Main_Window";
  HWND osk = ::FindWindow(kOSKClassName, NULL);
  if (::IsWindow(osk) && ::IsWindowEnabled(osk)) {
    PostMessage(osk, WM_SYSCOMMAND, SC_CLOSE, 0);
    return true;
  }
  return false;
}

enum DomainEnrollementState {UNKNOWN = -1, NOT_ENROLLED, ENROLLED};
static volatile long int g_domain_state = UNKNOWN;

bool IsEnrolledToDomain() {
  // Doesn't make any sense to retry inside a user session because joining a
  // domain will only kick in on a restart.
  if (g_domain_state == UNKNOWN) {
    LPWSTR domain;
    NETSETUP_JOIN_STATUS join_status;
    if(::NetGetJoinInformation(NULL, &domain, &join_status) != NERR_Success)
      return false;
    ::NetApiBufferFree(domain);
    ::InterlockedCompareExchange(&g_domain_state,
                                 join_status == ::NetSetupDomainName ?
                                     ENROLLED : NOT_ENROLLED,
                                 UNKNOWN);
  }

  return g_domain_state == ENROLLED;
}

void SetDomainStateForTesting(bool state) {
  g_domain_state = state ? ENROLLED : NOT_ENROLLED;
}

bool IsUser32AndGdi32Available() {
  static base::LazyInstance<LazyIsUser32AndGdi32Available>::Leaky available =
      LAZY_INSTANCE_INITIALIZER;
  return available.Get().value();
}

bool GetLoadedModulesSnapshot(HANDLE process, std::vector<HMODULE>* snapshot) {
  DCHECK(snapshot);
  DCHECK_EQ(0u, snapshot->size());
  snapshot->resize(128);

  // We will retry at least once after first determining |bytes_required|. If
  // the list of modules changes after we receive |bytes_required| we may retry
  // more than once.
  int retries_remaining = 5;
  do {
    DWORD bytes_required = 0;
    // EnumProcessModules returns 'success' even if the buffer size is too
    // small.
    DCHECK_GE(std::numeric_limits<DWORD>::max(),
              snapshot->size() * sizeof(HMODULE));
    if (!::EnumProcessModules(
            process, &(*snapshot)[0],
            static_cast<DWORD>(snapshot->size() * sizeof(HMODULE)),
            &bytes_required)) {
      DPLOG(ERROR) << "::EnumProcessModules failed.";
      return false;
    }
    DCHECK_EQ(0u, bytes_required % sizeof(HMODULE));
    size_t num_modules = bytes_required / sizeof(HMODULE);
    if (num_modules <= snapshot->size()) {
      // Buffer size was too big, presumably because a module was unloaded.
      snapshot->erase(snapshot->begin() + num_modules, snapshot->end());
      return true;
    } else if (num_modules == 0) {
      DLOG(ERROR) << "Can't determine the module list size.";
      return false;
    } else {
      // Buffer size was too small. Try again with a larger buffer. A little
      // more room is given to avoid multiple expensive calls to
      // ::EnumProcessModules() just because one module has been added.
      snapshot->resize(num_modules + 8, NULL);
    }
  } while (--retries_remaining);

  DLOG(ERROR) << "Failed to enumerate modules.";
  return false;
}

void EnableFlicks(HWND hwnd) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;
  ::RemoveProp(hwnd, MICROSOFT_TABLETPENSERVICE_PROPERTY);
}

void DisableFlicks(HWND hwnd) {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;
  ::SetProp(hwnd, MICROSOFT_TABLETPENSERVICE_PROPERTY,
      reinterpret_cast<HANDLE>(TABLET_DISABLE_FLICKS |
          TABLET_DISABLE_FLICKFALLBACKKEYS));
}

}  // namespace win
}  // namespace base
