// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
#define CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"

class Browser;
class ExtensionService;
class Profile;
class ToolbarActionsBar;
class ToolbarActionViewController;

namespace extensions {
class ExtensionRegistry;
}

// The registry for all component toolbar actions. Component toolbar actions
// are actions that live in the toolbar (like extension actions), but are for
// components of Chrome, such as Media Router.
class ComponentToolbarActionsFactory {
 public:
  // Extension and component action IDs.
  static const char kCastBetaExtensionId[];
  static const char kCastExtensionId[];
  static const char kMediaRouterActionId[];

  explicit ComponentToolbarActionsFactory(Profile* profile);
  virtual ~ComponentToolbarActionsFactory();

  // Returns a set of IDs of the component actions that should be present when
  // the toolbar model is initialized.
  virtual std::set<std::string> GetInitialComponentIds();

  // Called when component actions are added or removed before the toolbar model
  // is initialized. Adds or removes |action_id| to/from |initial_ids_| as
  // necessary.
  void OnAddComponentActionBeforeInit(const std::string& action_id);
  void OnRemoveComponentActionBeforeInit(const std::string& action_id);

  // Returns the controller responsible for the component action associated with
  // |action_id| in |bar|. Declared virtual for testing.
  virtual std::unique_ptr<ToolbarActionViewController>
  GetComponentToolbarActionForId(const std::string& action_id,
                                 Browser* browser,
                                 ToolbarActionsBar* bar);

  // Unloads extensions that were migrated to component actions and therefore
  // are no longer needed.
  void UnloadMigratedExtensions(ExtensionService* service,
                                extensions::ExtensionRegistry* registry);

 private:
  // Unloads an extension if it is active.
  void UnloadExtension(ExtensionService* service,
                       extensions::ExtensionRegistry* registry,
                       const std::string& extension_id);
#if defined(NWJS_SDK)
  Profile* profile_;
#endif
  // IDs of component actions that should be added to the toolbar model when it
  // gets initialized.
  std::set<std::string> initial_ids_;

  DISALLOW_COPY_AND_ASSIGN(ComponentToolbarActionsFactory);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_COMPONENT_TOOLBAR_ACTIONS_FACTORY_H_
