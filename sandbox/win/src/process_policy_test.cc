// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/windows_version.h"
#include "sandbox/win/src/sandbox.h"
#include "sandbox/win/src/sandbox_factory.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "sandbox/win/tests/common/controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Creates a process with the |exe| and |command| parameter using the
// unicode and ascii version of the api.
sandbox::SboxTestResult CreateProcessHelper(const base::string16& exe,
                                            const base::string16& command) {
  base::win::ScopedProcessInformation pi;
  STARTUPINFOW si = {sizeof(si)};
  const wchar_t* exe_name = NULL;
  if (!exe.empty())
    exe_name = exe.c_str();

  scoped_ptr<wchar_t, base::FreeDeleter>  writable_command(
      _wcsdup(command.c_str()));

  // Create the process with the unicode version of the API.
  sandbox::SboxTestResult ret1 = sandbox::SBOX_TEST_FAILED;
  PROCESS_INFORMATION temp_process_info = {};
  if (::CreateProcessW(exe_name,
                       command.empty() ? NULL : writable_command.get(),
                       NULL,
                       NULL,
                       FALSE,
                       0,
                       NULL,
                       NULL,
                       &si,
                       &temp_process_info)) {
    pi.Set(temp_process_info);
    ret1 = sandbox::SBOX_TEST_SUCCEEDED;
  } else {
    DWORD last_error = GetLastError();
    if ((ERROR_NOT_ENOUGH_QUOTA == last_error) ||
        (ERROR_ACCESS_DENIED == last_error) ||
        (ERROR_FILE_NOT_FOUND == last_error)) {
      ret1 = sandbox::SBOX_TEST_DENIED;
    } else {
      ret1 = sandbox::SBOX_TEST_FAILED;
    }
  }

  pi.Close();

  // Do the same with the ansi version of the api
  STARTUPINFOA sia = {sizeof(sia)};
  sandbox::SboxTestResult ret2 = sandbox::SBOX_TEST_FAILED;

  std::string narrow_cmd_line =
      base::SysWideToMultiByte(command.c_str(), CP_UTF8);
  if (::CreateProcessA(
        exe_name ? base::SysWideToMultiByte(exe_name, CP_UTF8).c_str() : NULL,
        command.empty() ? NULL : &narrow_cmd_line[0],
        NULL, NULL, FALSE, 0, NULL, NULL, &sia, &temp_process_info)) {
    pi.Set(temp_process_info);
    ret2 = sandbox::SBOX_TEST_SUCCEEDED;
  } else {
    DWORD last_error = GetLastError();
    if ((ERROR_NOT_ENOUGH_QUOTA == last_error) ||
        (ERROR_ACCESS_DENIED == last_error) ||
        (ERROR_FILE_NOT_FOUND == last_error)) {
      ret2 = sandbox::SBOX_TEST_DENIED;
    } else {
      ret2 = sandbox::SBOX_TEST_FAILED;
    }
  }

  if (ret1 == ret2)
    return ret1;

  return sandbox::SBOX_TEST_FAILED;
}

}  // namespace

namespace sandbox {

SBOX_TESTS_COMMAND int Process_RunApp1(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  base::string16 path = MakePathToSys(argv[0], false);

  // TEST 1: Try with the path in the app_name.
  return CreateProcessHelper(path, base::string16());
}

SBOX_TESTS_COMMAND int Process_RunApp2(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  base::string16 path = MakePathToSys(argv[0], false);

  // TEST 2: Try with the path in the cmd_line.
  base::string16 cmd_line = L"\"";
  cmd_line += path;
  cmd_line += L"\"";
  return CreateProcessHelper(base::string16(), cmd_line);
}

SBOX_TESTS_COMMAND int Process_RunApp3(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  // TEST 3: Try file name in the cmd_line.
  return CreateProcessHelper(base::string16(), argv[0]);
}

SBOX_TESTS_COMMAND int Process_RunApp4(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  // TEST 4: Try file name in the app_name and current directory sets correctly.
  base::string16 system32 = MakePathToSys(L"", false);
  wchar_t current_directory[MAX_PATH + 1];
  DWORD ret = ::GetCurrentDirectory(MAX_PATH, current_directory);
  if (!ret)
    return SBOX_TEST_FIRST_ERROR;
  if (ret >= MAX_PATH)
    return SBOX_TEST_FAILED;

  current_directory[ret] = L'\\';
  current_directory[ret+1] = L'\0';
  if (!::SetCurrentDirectory(system32.c_str())) {
    return SBOX_TEST_SECOND_ERROR;
  }

  const int result4 = CreateProcessHelper(argv[0], base::string16());
  return ::SetCurrentDirectory(current_directory) ? result4 : SBOX_TEST_FAILED;
}

SBOX_TESTS_COMMAND int Process_RunApp5(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  base::string16 path = MakePathToSys(argv[0], false);

  // TEST 5: Try with the path in the cmd_line and arguments.
  base::string16 cmd_line = L"\"";
  cmd_line += path;
  cmd_line += L"\" /I";
  return CreateProcessHelper(base::string16(), cmd_line);
}

SBOX_TESTS_COMMAND int Process_RunApp6(int argc, wchar_t **argv) {
  if (argc != 1) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }
  if ((NULL == argv) || (NULL == argv[0])) {
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;
  }

  // TEST 6: Try with the file_name in the cmd_line and arguments.
  base::string16 cmd_line = argv[0];
  cmd_line += L" /I";
  return CreateProcessHelper(base::string16(), cmd_line);
}

// Creates a process and checks if it's possible to get a handle to it's token.
SBOX_TESTS_COMMAND int Process_GetChildProcessToken(int argc, wchar_t **argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if ((NULL == argv) || (NULL == argv[0]))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  base::string16 path = MakePathToSys(argv[0], false);

  STARTUPINFOW si = {sizeof(si)};

  PROCESS_INFORMATION temp_process_info = {};
  if (!::CreateProcessW(path.c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
                        NULL, NULL, &si, &temp_process_info)) {
      return SBOX_TEST_FAILED;
  }
  base::win::ScopedProcessInformation pi(temp_process_info);

  HANDLE token = NULL;
  BOOL result =
      ::OpenProcessToken(pi.process_handle(), TOKEN_IMPERSONATE, &token);
  DWORD error = ::GetLastError();

  base::win::ScopedHandle token_handle(token);

  if (!::TerminateProcess(pi.process_handle(), 0))
    return SBOX_TEST_FAILED;

  if (result && token)
    return SBOX_TEST_SUCCEEDED;

  if (ERROR_ACCESS_DENIED == error)
    return SBOX_TEST_DENIED;

  return SBOX_TEST_FAILED;
}

// Creates a suspended process using CreateProcessA then kill it.
SBOX_TESTS_COMMAND int Process_CreateProcessA(int argc, wchar_t** argv) {
  if (argc != 1)
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  if ((NULL == argv) || (NULL == argv[0]))
    return SBOX_TEST_FAILED_TO_EXECUTE_COMMAND;

  STARTUPINFOA si = {sizeof(si)};

  base::string16 path = MakePathToSys(argv[0], false);

  PROCESS_INFORMATION temp_process_info = {};
  // Create suspended to avoid popping calc.
  if (!::CreateProcessA(base::SysWideToMultiByte(path, CP_UTF8).c_str(), NULL,
                        NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si,
                        &temp_process_info)) {
    return SBOX_TEST_FAILED;
  }
  base::win::ScopedProcessInformation pi(temp_process_info);

  if (!::TerminateProcess(pi.process_handle(), 0))
    return SBOX_TEST_FAILED;

  return SBOX_TEST_SUCCEEDED;
}

SBOX_TESTS_COMMAND int Process_OpenToken(int argc, wchar_t **argv) {
  HANDLE token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, &token)) {
    if (ERROR_ACCESS_DENIED == ::GetLastError()) {
      return SBOX_TEST_DENIED;
    }
  } else {
    ::CloseHandle(token);
    return SBOX_TEST_SUCCEEDED;
  }

  return SBOX_TEST_FAILED;
}

TEST(ProcessPolicyTest, TestAllAccess) {
  // Check if the "all access" rule fails to be added when the token is too
  // powerful.
  TestRunner runner;

  // Check the failing case.
  runner.GetPolicy()->SetTokenLevel(USER_INTERACTIVE, USER_LOCKDOWN);
  EXPECT_EQ(SBOX_ERROR_UNSUPPORTED,
            runner.GetPolicy()->AddRule(TargetPolicy::SUBSYS_PROCESS,
                                        TargetPolicy::PROCESS_ALL_EXEC,
                                        L"this is not important"));

  // Check the working case.
  runner.GetPolicy()->SetTokenLevel(USER_INTERACTIVE, USER_INTERACTIVE);

  EXPECT_EQ(SBOX_ALL_OK,
            runner.GetPolicy()->AddRule(TargetPolicy::SUBSYS_PROCESS,
                                        TargetPolicy::PROCESS_ALL_EXEC,
                                        L"this is not important"));
}

TEST(ProcessPolicyTest, CreateProcessAW) {
  TestRunner runner;
  base::string16 maybe_virtual_exe_path = MakePathToSys(L"findstr.exe", false);
  base::string16 non_virtual_exe_path = MakePathToSys32(L"findstr.exe", false);
  ASSERT_TRUE(!maybe_virtual_exe_path.empty());

  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_MIN_EXEC,
                             maybe_virtual_exe_path.c_str()));

  if (non_virtual_exe_path != maybe_virtual_exe_path) {
    EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                               TargetPolicy::PROCESS_MIN_EXEC,
                               non_virtual_exe_path.c_str()));
  }

  // Need to add directory rules for the directories that we use in
  // SetCurrentDirectory.
  EXPECT_TRUE(runner.AddRuleSys32(TargetPolicy::FILES_ALLOW_DIR_ANY, L""));

  wchar_t current_directory[MAX_PATH];
  DWORD ret = ::GetCurrentDirectory(MAX_PATH, current_directory);
  ASSERT_TRUE(0 != ret && ret < MAX_PATH);

  wcscat_s(current_directory, MAX_PATH, L"\\");
  EXPECT_TRUE(runner.AddFsRule(TargetPolicy::FILES_ALLOW_DIR_ANY,
                               current_directory));

  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp1 calc.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp2 calc.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp3 calc.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp4 calc.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp5 calc.exe"));
  EXPECT_EQ(SBOX_TEST_DENIED, runner.RunTest(L"Process_RunApp6 calc.exe"));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_RunApp1 findstr.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_RunApp2 findstr.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_RunApp3 findstr.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_RunApp4 findstr.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_RunApp5 findstr.exe"));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_RunApp6 findstr.exe"));
}

TEST(ProcessPolicyTest, OpenToken) {
  TestRunner runner;
  EXPECT_EQ(SBOX_TEST_SUCCEEDED, runner.RunTest(L"Process_OpenToken"));
}

TEST(ProcessPolicyTest, TestGetProcessTokenMinAccess) {
  TestRunner runner;
  base::string16 exe_path = MakePathToSys(L"findstr.exe", false);
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_MIN_EXEC,
                             exe_path.c_str()));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"Process_GetChildProcessToken findstr.exe"));
}

TEST(ProcessPolicyTest, TestGetProcessTokenMaxAccess) {
  TestRunner runner(JOB_UNPROTECTED, USER_INTERACTIVE, USER_INTERACTIVE);
  base::string16 exe_path = MakePathToSys(L"findstr.exe", false);
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_ALL_EXEC,
                             exe_path.c_str()));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_GetChildProcessToken findstr.exe"));
}

TEST(ProcessPolicyTest, TestGetProcessTokenMinAccessNoJob) {
  TestRunner runner(JOB_NONE, USER_RESTRICTED_SAME_ACCESS, USER_LOCKDOWN);
  base::string16 exe_path = MakePathToSys(L"findstr.exe", false);
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_MIN_EXEC,
                             exe_path.c_str()));

  EXPECT_EQ(SBOX_TEST_DENIED,
            runner.RunTest(L"Process_GetChildProcessToken findstr.exe"));
}

TEST(ProcessPolicyTest, TestGetProcessTokenMaxAccessNoJob) {
  TestRunner runner(JOB_NONE, USER_INTERACTIVE, USER_INTERACTIVE);
  base::string16 exe_path = MakePathToSys(L"findstr.exe", false);
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_ALL_EXEC,
                             exe_path.c_str()));

  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_GetChildProcessToken findstr.exe"));
}

TEST(ProcessPolicyTest, TestCreateProcessA) {
  TestRunner runner;
  sandbox::TargetPolicy* policy = runner.GetPolicy();
  policy->SetJobLevel(JOB_NONE, 0);
  policy->SetTokenLevel(USER_UNPROTECTED, USER_UNPROTECTED);

  base::string16 exe_path = MakePathToSys(L"calc.exe", false);
  ASSERT_TRUE(!exe_path.empty());
  EXPECT_TRUE(runner.AddRule(TargetPolicy::SUBSYS_PROCESS,
                             TargetPolicy::PROCESS_ALL_EXEC, exe_path.c_str()));
  EXPECT_EQ(SBOX_TEST_SUCCEEDED,
            runner.RunTest(L"Process_CreateProcessA calc.exe"));
}

}  // namespace sandbox
