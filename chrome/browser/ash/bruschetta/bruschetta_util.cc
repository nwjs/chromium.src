// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_util.h"

#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bruschetta/bruschetta_installer_view.h"
#include "components/prefs/pref_service.h"

namespace bruschetta {

namespace {
absl::optional<const base::Value::Dict*> GetConfigWithEnabledLevel(
    const Profile* profile,
    const std::string& config_id,
    prefs::PolicyEnabledState enabled_level) {
  const auto* config_ptr = profile->GetPrefs()
                               ->GetDict(prefs::kBruschettaVMConfiguration)
                               .FindDict(config_id);
  if (!config_ptr || config_ptr->FindInt(prefs::kPolicyEnabledKey) <
                         static_cast<int>(enabled_level)) {
    return absl::nullopt;
  }

  return config_ptr;
}
}  // namespace

const char kToolsDlc[] = "termina-tools-dlc";

const char kBruschettaVmName[] = "bru";
const char kBruschettaDisplayName[] = "Bruschetta";

const char kBiosPath[] = "Downloads/CROSVM_CODE.fd";
const char kPflashPath[] = "Downloads/CROSVM_VARS.google.fd";

const char* BruschettaResultString(const BruschettaResult res) {
#define ENTRY(name)            \
  case BruschettaResult::name: \
    return #name
  switch (res) {
    ENTRY(kUnknown);
    ENTRY(kSuccess);
    ENTRY(kDlcInstallError);
    ENTRY(kBiosNotAccessible);
    ENTRY(kStartVmFailed);
    ENTRY(kTimeout);
  }
#undef ENTRY
  return "unknown code";
}

guest_os::GuestId GetBruschettaAlphaId() {
  return MakeBruschettaId(kBruschettaVmName);
}

guest_os::GuestId MakeBruschettaId(std::string vm_name) {
  return guest_os::GuestId{guest_os::VmType::BRUSCHETTA, std::move(vm_name),
                           "penguin"};
}

absl::optional<const base::Value::Dict*> GetRunnableConfig(
    const Profile* profile,
    const std::string& config_id) {
  return GetConfigWithEnabledLevel(profile, config_id,
                                   prefs::PolicyEnabledState::RUN_ALLOWED);
}

base::FilePath BruschettaChromeOSBaseDirectory() {
  return base::FilePath("/mnt/shared");
}

absl::optional<const base::Value::Dict*> GetInstallableConfig(
    const Profile* profile,
    const std::string& config_id) {
  return GetConfigWithEnabledLevel(profile, config_id,
                                   prefs::PolicyEnabledState::INSTALL_ALLOWED);
}

bool HasInstallableConfig(const Profile* profile,
                          const std::string& config_id) {
  return GetInstallableConfig(profile, config_id).has_value();
}

bool IsInstalled(Profile* profile, const guest_os::GuestId& guest_id) {
  const base::Value* value = guest_os::GetContainerPrefValue(
      profile, guest_id, guest_os::prefs::kVmNameKey);
  return value != nullptr;
}

void RunInstaller(Profile* profile, const guest_os::GuestId& guest_id) {
  BruschettaInstallerView::Show(profile, guest_id);
}

}  // namespace bruschetta
