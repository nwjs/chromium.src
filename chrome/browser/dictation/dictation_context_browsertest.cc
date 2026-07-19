// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_context.h"

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_browser_test_base.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/listener_stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace dictation {

class DictationContextBrowserTest : public DictationBrowserTestBase {
 public:
  DictationContextBrowserTest()
      : scoped_feature_list_(CreateEnablingFeatureList()) {}
  ~DictationContextBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    DictationBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        kDictationTestExtensionId);
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, APCCaptured) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadTestExtensionInManualMode(profile());

  // This test page has a bit of text content.
  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  dictation_service().StartSession(
      *GetBrowserWindowInterface(),
      std::make_unique<Target>(web_contents()->GetPrimaryMainFrame(), ""));

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  DictationContext context = ExtensionGetDictationContext(
      profile(), provider->stream_id_for_testing());

  // Verify that the annotated page content was captured.
  ASSERT_TRUE(context.annotated_page_content.has_value());
  ASSERT_TRUE(context.annotated_page_content->has_root_node());

  const auto& root = context.annotated_page_content->root_node();
  ASSERT_GT(root.children_nodes_size(), 0);

  const auto& first_child = root.children_nodes(0);
  ASSERT_TRUE(first_child.content_attributes().has_text_data());
  EXPECT_EQ(base::TrimWhitespaceASCII(
                first_child.content_attributes().text_data().text_content(),
                base::TRIM_ALL),
            "Non empty simple page");
  EXPECT_EQ(context.annotated_page_content->main_frame_data().title(), "OK");
}

IN_PROC_BROWSER_TEST_F(DictationContextBrowserTest, SelectedTextCaptured) {
  ASSERT_TRUE(embedded_test_server()->Start());

  LoadTestExtensionInManualMode(profile());

  const GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  // Start the session with a non-empty selected text.
  dictation_service().StartSession(
      *GetBrowserWindowInterface(),
      std::make_unique<Target>(web_contents()->GetPrimaryMainFrame(),
                               "hello world"));

  SessionController* controller = dictation_service().session_controller();
  ASSERT_NE(controller, nullptr);

  ListenerStreamProvider* provider = static_cast<ListenerStreamProvider*>(
      controller->attached_stream_provider());
  ASSERT_NE(provider, nullptr);

  ExtensionWaitForStreamStart(profile(), provider->stream_id_for_testing());
  DictationContext context = ExtensionGetDictationContext(
      profile(), provider->stream_id_for_testing());

  // Verify that the editable content was captured and matches.
  ASSERT_TRUE(context.editable_content.has_value());
  EXPECT_EQ(*context.editable_content, "hello world");
}

}  // namespace dictation
