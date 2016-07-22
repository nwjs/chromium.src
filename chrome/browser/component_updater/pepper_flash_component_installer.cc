// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pepper_flash_component_installer.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pepper_flash.h"
#include "chrome/common/ppapi_utils.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/pepper_plugin_info.h"
#include "flapper_version.h"  // In SHARED_INTERMEDIATE_DIR.  NOLINT
#include "ppapi/shared_impl/ppapi_permissions.h"

#if defined(OS_LINUX)
#include "chrome/common/component_flash_hint_file_linux.h"
#endif  // defined(OS_LINUX)

using content::BrowserThread;
using content::PluginService;

namespace component_updater {

namespace {

#if defined(GOOGLE_CHROME_BUILD)
// CRX hash. The extension id is: mimojjlkmoijpicakmndhoigimigcmbb.
const uint8_t kSha2Hash[] = {0xc8, 0xce, 0x99, 0xba, 0xce, 0x89, 0xf8, 0x20,
                             0xac, 0xd3, 0x7e, 0x86, 0x8c, 0x86, 0x2c, 0x11,
                             0xb9, 0x40, 0xc5, 0x55, 0xaf, 0x08, 0x63, 0x70,
                             0x54, 0xf9, 0x56, 0xd3, 0xe7, 0x88, 0xba, 0x8c};
#endif  // defined(GOOGLE_CHROME_BUILD)

#if !defined(OS_LINUX) && defined(GOOGLE_CHROME_BUILD)
bool MakePepperFlashPluginInfo(const base::FilePath& flash_path,
                               const Version& flash_version,
                               bool out_of_process,
                               content::PepperPluginInfo* plugin_info) {
  if (!flash_version.IsValid())
    return false;
  const std::vector<uint32_t> ver_nums = flash_version.components();
  if (ver_nums.size() < 3)
    return false;

  plugin_info->is_internal = false;
  plugin_info->is_out_of_process = out_of_process;
  plugin_info->path = flash_path;
  plugin_info->name = content::kFlashPluginName;
  plugin_info->permissions = chrome::kPepperFlashPermissions;

  // The description is like "Shockwave Flash 10.2 r154".
  plugin_info->description = base::StringPrintf("%s %d.%d r%d",
                                                content::kFlashPluginName,
                                                ver_nums[0],
                                                ver_nums[1],
                                                ver_nums[2]);

  plugin_info->version = flash_version.GetString();

  content::WebPluginMimeType swf_mime_type(content::kFlashPluginSwfMimeType,
                                           content::kFlashPluginSwfExtension,
                                           content::kFlashPluginName);
  plugin_info->mime_types.push_back(swf_mime_type);
  content::WebPluginMimeType spl_mime_type(content::kFlashPluginSplMimeType,
                                           content::kFlashPluginSplExtension,
                                           content::kFlashPluginName);
  plugin_info->mime_types.push_back(spl_mime_type);
  return true;
}

bool IsPepperFlash(const content::WebPluginInfo& plugin) {
  // We try to recognize Pepper Flash by the following criteria:
  // * It is a Pepper plugin.
  // * It has the special Flash permissions.
  return plugin.is_pepper_plugin() &&
         (plugin.pepper_permissions & ppapi::PERMISSION_FLASH);
}

void RegisterPepperFlashWithChrome(const base::FilePath& path,
                                   const Version& version) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::PepperPluginInfo plugin_info;
  if (!MakePepperFlashPluginInfo(path, version, true, &plugin_info))
    return;

  base::FilePath bundled_flash_dir;
  PathService::Get(chrome::DIR_PEPPER_FLASH_PLUGIN, &bundled_flash_dir);
  base::FilePath system_flash_path;
  PathService::Get(chrome::FILE_PEPPER_FLASH_SYSTEM_PLUGIN, &system_flash_path);

  std::vector<content::WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);
  for (const auto& plugin : plugins) {
    if (!IsPepperFlash(plugin))
      continue;

    Version registered_version(base::UTF16ToUTF8(plugin.version));

    // If lower version, never register.
    if (registered_version.IsValid() &&
        version.CompareTo(registered_version) < 0) {
      return;
    }

    bool registered_is_bundled =
        !bundled_flash_dir.empty() && bundled_flash_dir.IsParent(plugin.path);
    bool registered_is_debug_system =
        !system_flash_path.empty() &&
        base::FilePath::CompareEqualIgnoreCase(plugin.path.value(),
                                               system_flash_path.value()) &&
        chrome::IsSystemFlashScriptDebuggerPresent();
    bool is_on_network = false;
#if defined(OS_WIN)
    // On Windows, component updated DLLs can't load off network drives.
    // See crbug.com/572131 for details.
    is_on_network = false; //base::IsOnNetworkDrive(path);
#endif
    // If equal version, register iff component is not on a network drive,
    // and the version of flash is not bundled, and not debug system.
    if (registered_version.IsValid() &&
        version.CompareTo(registered_version) == 0 &&
        (is_on_network || registered_is_bundled ||
         registered_is_debug_system)) {
      return;
    }

    // If the version is newer, remove the old one first.
    PluginService::GetInstance()->UnregisterInternalPlugin(plugin.path);
    break;
  }

  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  PluginService::GetInstance()->RefreshPlugins();
}

void NotifyPathServiceAndChrome(const base::FilePath& path,
                                const Version& version) {
  PathService::Override(chrome::DIR_PEPPER_FLASH_PLUGIN, path);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RegisterPepperFlashWithChrome,
                 path.Append(chrome::kPepperFlashPluginFilename),
                 version));
}
#endif  // !defined(OS_LINUX) && defined(GOOGLE_CHROME_BUILD)

#if defined(GOOGLE_CHROME_BUILD)
class FlashComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  FlashComponentInstallerTraits();
  ~FlashComponentInstallerTraits() override {}

 private:
  // The following methods override ComponentInstallerTraits.
  bool CanAutoUpdate() const override;
  bool RequiresNetworkEncryption() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                       const base::FilePath& install_dir) override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& path,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  DISALLOW_COPY_AND_ASSIGN(FlashComponentInstallerTraits);
};

FlashComponentInstallerTraits::FlashComponentInstallerTraits() {}

bool FlashComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool FlashComponentInstallerTraits::RequiresNetworkEncryption() const {
  return false;
}

bool FlashComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
#if defined(OS_LINUX)
  const base::FilePath flash_path =
      install_dir.Append(chrome::kPepperFlashPluginFilename);
  // Populate the component updated flash hint file so that the zygote can
  // locate and preload the latest version of flash.
  std::string version;
  if (!manifest.GetString("version", &version))
    return false;
  if (!component_flash_hint_file::RecordFlashUpdate(flash_path, flash_path,
                                                    version)) {
    return false;
  }
#endif  // defined(OS_LINUX)
  return true;
}

void FlashComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    std::unique_ptr<base::DictionaryValue> manifest) {
#if !defined(OS_LINUX)
  // Installation is done. Now tell the rest of chrome. Both the path service
  // and to the plugin service. On Linux, a restart is required to use the new
  // Flash version, so we do not do this.
  BrowserThread::GetBlockingPool()->PostTask(
      FROM_HERE, base::Bind(&NotifyPathServiceAndChrome, path, version));
#endif  // !defined(OS_LINUX)
}

bool FlashComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  Version unused;
  return chrome::CheckPepperFlashManifest(manifest, &unused);
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\PepperFlash\.
base::FilePath FlashComponentInstallerTraits::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("PepperFlash"));
}

void FlashComponentInstallerTraits::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(kSha2Hash, kSha2Hash + arraysize(kSha2Hash));
}

std::string FlashComponentInstallerTraits::GetName() const {
  return "pepper_flash";
}

update_client::InstallerAttributes
FlashComponentInstallerTraits::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace

void RegisterPepperFlashComponent(ComponentUpdateService* cus) {
#if defined(GOOGLE_CHROME_BUILD)
  // Component updated flash supersedes bundled flash therefore if that one
  // is disabled then this one should never install.
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableBundledPpapiFlash))
    return;
  std::unique_ptr<ComponentInstallerTraits> traits(
      new FlashComponentInstallerTraits);
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(std::move(traits));
  installer->Register(cus, base::Closure());
#endif  // defined(GOOGLE_CHROME_BUILD)
}

}  // namespace component_updater
