// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_TEST_OVERRIDE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_TEST_OVERRIDE_H_

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"

class Profile;

namespace web_app {

#if BUILDFLAG(IS_LINUX)
struct LinuxFileRegistration {
  std::string xdg_command;
  std::string file_contents;
};
#endif

// This class is used to help test OS integration code and operations running on
// trybots. Among other complexities, trybots are often running multiple tests
// at the same times, so anything that operates in shared OS state could have
// side effects that this class attempts to solve. (For example, this class
// makes sure that on Mac, we 'install' the application to a temporary directory
// to avoid overwriting one from another test).
//
// The general rules for adding / using this are:
// - If the OS integration CAN be fully tested on a trybot, do so. The presence
//   of this class can allow customization of the integration if needed (e.g.
//   changing folders).
//   - If the information 'written' to the OS CAN be easily read back / verified
//     in a test, then no further work needed, and tests can do this.
//   - If the information 'written' to the OS CANNOT be easily read back /
//     verified in a test, then populate metadata in this object about the
//     final OS call for tests to check.
// - If the OS integration CANNOT be fully tested on a trybot (it doesn't work
// or
//   messes up the environment), then the presence of this object disables the
//   os integration, and information is populated about the final OS call in
//   this class.
//
// This class is used across multiple different sequenced task runners:
// - Created on the UI thread.
// - Accessed & sometimes modified by the shortcut task runner.
// - Accessed by the UI thread.
// It is up to the user to ensure thread safety of this class through
// ordering guarantees.
class OsIntegrationTestOverride
    : public base::RefCountedThreadSafe<OsIntegrationTestOverride> {
 public:
  using AppProtocolList =
      std::vector<std::tuple<AppId, std::vector<std::string>>>;
  // Destroying this class blocks the thread until all users of
  // GetOsIntegrationTestOverride() have completed.
  struct BlockingRegistration {
    BlockingRegistration();
    ~BlockingRegistration();

    scoped_refptr<OsIntegrationTestOverride> test_override;
  };

  OsIntegrationTestOverride(const OsIntegrationTestOverride&) = delete;

  // Overrides applicable directories for shortcut integration and returns an
  // object that:
  // 1) Contains the directories.
  // 2) Keeps the override active until the object is destroyed.
  // 3) DCHECK-fails on destruction if any of the shortcut directories / os
  //    hooks are NOT cleanup by the test. This ensures that trybots don't have
  //    old test artifacts on them that can make future tests flaky.
  // All installs that occur during the lifetime of the
  // OsIntegrationTestOverride MUST be uninstalled before it is
  // destroyed.
  // The returned value, on destruction, will block until all usages of the
  // GetOsIntegrationTestOverride() are destroyed.
  static std::unique_ptr<BlockingRegistration> OverrideForTesting(
      const base::FilePath& base_path = base::FilePath());

  // Looks into shortcuts stored for OS integration and returns if run on OS
  // login mode is enabled based on the location. This should only be run on
  // Windows, Mac and Linux.
  bool IsRunOnOsLoginEnabled(Profile* profile,
                             const AppId& app_id,
                             const std::string& app_name);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Reads the icon color for a specific shortcut created
  absl::optional<SkColor> GetShortcutIconTopLeftColor(
      Profile* profile,
      base::FilePath shortcut_dir,
      const AppId& app_id,
      const std::string& app_name);
#endif

  // Gets the current shortcut path based on a shortcut directory, app_id and
  // app_name. This should only be run on Windows, Mac and Linux.
  base::FilePath GetShortcutPath(Profile* profile,
                                 base::FilePath shortcut_dir,
                                 const AppId& app_id,
                                 const std::string& app_name);

  // Looks into the current shortcut paths to determine if a shortcut has
  // been created or not. This should only be run on Windows, Mac and Linux.
  bool IsShortcutCreated(Profile* profile,
                         const AppId& app_id,
                         const std::string& app_name);

  // Delete shortcuts stored in the test override for a specific app. This
  // should only be run on Windows, Mac and Linux.
  bool SimulateDeleteShortcutsByUser(Profile* profile,
                                     const AppId& app_id,
                                     const std::string& app_name);

  // Used to clear all shortcut override paths during tests. This should only be
  // run on Windows, Mac and Linux.
  bool ForceDeleteAllShortcuts();

#if BUILDFLAG(IS_WIN)
  bool DeleteDesktopDirOnWin();
  bool DeleteApplicationMenuDirOnWin();
  const base::FilePath& desktop() { return desktop_.GetPath(); }
  const base::FilePath& application_menu() {
    return application_menu_.GetPath();
  }
  const base::FilePath& quick_launch() { return quick_launch_.GetPath(); }
  const base::FilePath& startup() { return startup_.GetPath(); }
#elif BUILDFLAG(IS_MAC)
  bool DeleteChromeAppsDir();
  bool IsChromeAppsValid() { return chrome_apps_folder_.IsValid(); }
  const base::FilePath& chrome_apps_folder() {
    return chrome_apps_folder_.GetPath();
  }
  void EnableOrDisablePathOnLogin(const base::FilePath& file_path,
                                  bool enable_on_login);
#elif BUILDFLAG(IS_LINUX)
  bool DeleteDesktopDirOnLinux();
  const base::FilePath& desktop() { return desktop_.GetPath(); }
  const base::FilePath& startup() { return startup_.GetPath(); }
  const std::vector<LinuxFileRegistration>& linux_file_registration() {
    return linux_file_registration_;
  }
#endif

  // Creates a tuple of app_id to protocols and adds it to the vector
  // of registered protocols. There can be multiple entries for the same
  // app_id.
  void RegisterProtocolSchemes(const AppId& app_id,
                               std::vector<std::string> protocols);
  const AppProtocolList& protocol_scheme_registrations() {
    return protocol_scheme_registrations_;
  }

 private:
  friend class base::RefCountedThreadSafe<OsIntegrationTestOverride>;

  explicit OsIntegrationTestOverride(const base::FilePath& base_path);
  ~OsIntegrationTestOverride();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Reads an icon file (.ico/.png/.icns) and returns the color at the
  // top left position (0,0).
  SkColor GetIconTopLeftColorFromShortcutFile(
      const base::FilePath& shortcut_path);
#endif

#if BUILDFLAG(IS_WIN)
  base::ScopedTempDir desktop_;
  base::ScopedTempDir application_menu_;
  base::ScopedTempDir quick_launch_;
  base::ScopedTempDir startup_;
#elif BUILDFLAG(IS_MAC)
  base::ScopedTempDir chrome_apps_folder_;
  std::map<base::FilePath, bool> startup_enabled_;
#elif BUILDFLAG(IS_LINUX)
  base::ScopedTempDir desktop_;
  base::ScopedTempDir startup_;
  std::vector<LinuxFileRegistration> linux_file_registration_;
#endif

  // Records all registration events for a given app id & protocol list. Due to
  // simplification on the OS-side, unregistrations are not recorded, and
  // instead this list can be checked for an empty registration.
  AppProtocolList protocol_scheme_registrations_;

  // |on_destruction_| has it's closure set only once (when BlockingRegistration
  // is destroyed) and executed when OsIntegrationTestOverride is destroyed.
  // The destructor of BlockingRegistration explicitly sets this closure with a
  // global lock, then destroys the object, then waits on the closure, so it is
  // thread-compatible.
  base::ScopedClosureRunner on_destruction_;
};

scoped_refptr<OsIntegrationTestOverride> GetOsIntegrationTestOverride();

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_OS_INTEGRATION_TEST_OVERRIDE_H_
