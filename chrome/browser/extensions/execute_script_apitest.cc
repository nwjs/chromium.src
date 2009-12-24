// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExecuteScript) {
  // We need a.com to be a little bit slow to trigger a race condition.
  host_resolver()->AddRuleWithLatency("a.com", "127.0.0.1", 500);
  host_resolver()->AddRule("b.com", "127.0.0.1");
  host_resolver()->AddRule("c.com", "127.0.0.1");
  StartHTTPServer();

  ASSERT_TRUE(RunExtensionTest("executescript")) << message_;
  ASSERT_TRUE(RunExtensionTest("executescript_in_frame")) << message_;
  ASSERT_TRUE(RunExtensionTest("executescript/permissions")) << message_;
}
