// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;
using testing::Gt;

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kHostASubdomain[] = "subdomain.a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";
constexpr char kHostD[] = "d.test";

constexpr char kRequestOutcomeHistogram[] =
    "API.TopLevelStorageAccess.RequestOutcome";

class RequestStorageAccessForOriginBaseBrowserTest
    : public InProcessBrowserTest {
 protected:
  RequestStorageAccessForOriginBaseBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                            GetDisabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    std::vector<base::test::FeatureRefAndParams> enabled(
        {{net::features::kStorageAccessAPI, {}}});
    return enabled;
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void SetCrossSiteCookieOnHost(const std::string& host) {
    GURL host_url = GetURL(host);
    std::string cookie = base::StrCat({"cross-site=", host});
    content::SetCookie(browser()->profile(), host_url,
                       base::StrCat({cookie, ";SameSite=None;Secure"}));
    ASSERT_THAT(content::GetCookies(browser()->profile(), host_url),
                testing::HasSubstr(cookie));
  }

  void SetPartitionedCookieInContext(const std::string& top_level_host,
                                     const std::string& embedded_host) {
    GURL host_url = GetURL(embedded_host);
    std::string cookie =
        base::StrCat({"cross-site=", embedded_host, "(partitioned)"});
    net::CookiePartitionKey partition_key =
        net::CookiePartitionKey::FromURLForTesting(GetURL(top_level_host));
    content::SetPartitionedCookie(
        browser()->profile(), host_url,
        base::StrCat({cookie, ";SameSite=None;Secure;Partitioned"}),
        partition_key);
    ASSERT_THAT(content::GetCookies(
                    browser()->profile(), host_url,
                    net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                    net::CookiePartitionKeyCollection(partition_key)),
                testing::HasSubstr(cookie));
  }

  GURL GetURL(const std::string& host) {
    return https_server_.GetURL(host, "/");
  }

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateToNewTabWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  std::string GetFrameContent() {
    return storage::test::GetFrameContent(GetFrame());
  }

  void NavigateNestedFrameTo(const std::string& host, const std::string& path) {
    GURL url(https_server_.GetURL(host, path));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver load_observer(web_contents);
    ASSERT_TRUE(ExecuteScript(
        GetFrame(),
        base::StringPrintf("document.body.querySelector('iframe').src = '%s';",
                           url.spec().c_str())));
    load_observer.Wait();
  }

  std::string GetNestedFrameContent() {
    return storage::test::GetFrameContent(GetNestedFrame());
  }

  std::string ReadCookiesViaJS(content::RenderFrameHost* render_frame_host) {
    return content::EvalJs(render_frame_host, "document.cookie")
        .ExtractString();
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedFrame() {
    return ChildFrameAt(GetFrame(), 0);
  }

  std::string CookiesFromFetchWithCredentials(content::RenderFrameHost* frame,
                                              const std::string& host,
                                              const bool cors_enabled) {
    return storage::test::FetchWithCredentials(
        frame, https_server_.GetURL(host, "/echoheader?cookie"), cors_enabled);
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
};

class RequestStorageAccessForOriginBrowserTest
    : public RequestStorageAccessForOriginBaseBrowserTest {};

// Validates that expiry data is transferred over IPC to the Network Service.
IN_PROC_BROWSER_TEST_F(RequestStorageAccessForOriginBrowserTest,
                       ThirdPartyGrantsExpireOverIPC) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `kHostB` and `kHostC`.
  SetCrossSiteCookieOnHost(kHostB);
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "cross-site=b.test");
  SetCrossSiteCookieOnHost(kHostC);
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostC)),
            "cross-site=c.test");

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Manually create a pre-expired grant and ensure it doesn't grant access.
  base::Time expiration_time = base::Time::Now() - base::Minutes(5);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      GetURL(kHostB), GetURL(kHostA),
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW,
      {expiration_time, content_settings::SessionModel::UserSession});
  settings_map->SetContentSettingDefaultScope(
      GetURL(kHostC), GetURL(kHostA),
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, CONTENT_SETTING_ALLOW,
      {expiration_time, content_settings::SessionModel::UserSession});

  // Manually send our expired setting. This needs to be done manually because
  // normally this expired value would be filtered out before sending and time
  // cannot be properly mocked in a browser test.
  ContentSettingsForOneType settings;
  settings.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostB)),
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostA)),
      base::Value(CONTENT_SETTING_ALLOW), "preference",
      /*incognito=*/false, {.expiration = expiration_time}));
  settings.emplace_back(
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostC)),
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostA)),
      base::Value(CONTENT_SETTING_ALLOW), "preference",
      /*incognito=*/false);

  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetTopLevelStorageAccessSettings(settings, base::DoNothing());

  // document.hasStorageAccess() does not have cookie access with top-level
  // storage access grant.
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "None");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetNestedFrame(), kHostC,
                                            /*cors_enabled=*/true),
            "cross-site=c.test");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForOriginBrowserTest,
                       RsaForOriginDisabledByDefault) {
  NavigateToPageWithFrame(kHostA);
  // Ensure that the proposed extension is not available unless explicitly
  // enabled.
  EXPECT_TRUE(EvalJs(GetPrimaryMainFrame(),
                     "\"requestStorageAccessForOrigin\" in document === false")
                  .ExtractBool());
}

class RequestStorageAccessForOriginEnabledBrowserTest
    : public RequestStorageAccessForOriginBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        RequestStorageAccessForOriginBaseBrowserTest::GetEnabledFeatures();
    enabled.push_back(
        {blink::features::kStorageAccessAPIForOriginExtension, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForOriginEnabledBrowserTest,
                       SameOriginGrantedByDefault) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  NavigateToPageWithFrame(kHostA);

  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), "https://asdf.example"));
  EXPECT_FALSE(
      storage::test::RequestStorageAccessForOrigin(GetFrame(), "mattwashere"));
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), GetURL(kHostA).spec()));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForOriginEnabledBrowserTest,
                       TopLevelOpaqueOriginRejected) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("data:,Hello%2C%20World%21")));

  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForOriginEnabledBrowserTest,
                       RequestStorageAccessForOriginEmbeddedOriginScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  // Verify that the top-level scoping does not leak to the embedded URL, whose
  // origin must be used.
  NavigateToPageWithFrame(kHostB);

  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Regardless of the top-level site or origin scoping, the embedded origin
  // should be used.
  NavigateFrameTo(kHostASubdomain, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostASubdomain,
                                            /*cors_enabled=*/true),
            "None");
}

// Tests to validate First-Party Set use with `requestStorageAccessForOrigin`.
class RequestStorageAccessForOriginWithFirstPartySetsBrowserTest
    : public RequestStorageAccessForOriginBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RequestStorageAccessForOriginBaseBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostC, R"("])",
                      R"(, "serviceSites": ["https://)", kHostB, R"("]})"}));
  }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{blink::features::kStorageAccessAPIForOriginExtension, {}},
            {net::features::kStorageAccessAPI, {}}};
  }
};

// Validate that if a top-level document requests access that cookies become
// unblocked for just that top-level/third-party combination.
IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsBrowserTest,
    // TODO(crbug.com/1370096): Re-enable usage metric assertions.
    Permission_AutograntedWithinFirstPartySet) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);
  SetCrossSiteCookieOnHost(kHostC);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The request comes from `kHostA`, which is in a First-Party Set with
  // `khostB`. Note that `kHostB` would not be auto-granted access if it were
  // the requestor, because it is a service domain.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Repeated calls should also return true.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent for the cors-enabled subresource request.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");

  // Also validate that an additional site C was not granted access.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostC,
                                            /*cors_enabled=*/true),
            "None");

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  0 /*RequestOutcome::kGrantedByFirstPartySet*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsBrowserTest,
    Permission_AutodeniedForServiceDomain) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostB);

  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostA,
                                            /*cors_enabled=*/true),
            "None");
  // The promise should be rejected; `khostB` is a service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Re-navigate iframe to a cross-site, cookie-reading endpoint, and verify
  // that the cookie is not sent.
  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostA,
                                            /*cors_enabled=*/true),
            "None");

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  5 /*RequestOutcome::kDeniedByPrerequisites*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsBrowserTest,
    Permission_AutodeniedForServiceDomainInIframe) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "None");
  // `kHostB` cannot be granted access via `RequestStorageAccessForOrigin`,
  // because the call is not from the top-level page and because `kHostB` is a
  // service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), GetURL(kHostA).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "None");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "None");

  // However, a regular `requestStorageAccess` call should be granted;
  // requesting on behalf of another domain is what is not acceptable.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // When the frame subsequently navigates to an endpoint on kHostB,
  // kHostB's cookies are sent, and the iframe retains storage
  // access.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsBrowserTest,
    Permission_AutodeniedOutsideFirstPartySet) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostD);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // `kHostD` cannot be granted access via `RequestStorageAccessForOrigin` in
  // this configuration, because the requesting site (`kHostA`) is not in the
  // same First-Party Set as the requested site (`kHostD`).
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostD).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostD,
                                            /*cors_enabled=*/true),
            "None");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostD,
                                            /*cors_enabled=*/true),
            "None");

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  3 /*RequestOutcome::kDeniedByFirstPartySet*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsBrowserTest,
    RequestStorageAccessForOriginTopLevelScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "None");
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent for the cors-enabled subresource request.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request with cors disabled does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/false),
            "None");

  NavigateToPageWithFrame(kHostASubdomain);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // Storage access grants are scoped to the embedded origin on the top-level
  // site. Accordingly, the access is be granted for subresource request.
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
}

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsBrowserTest,
    RequestStorageAccessForOriginTopLevelScopingWhenRequestedFromSubdomain) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostASubdomain);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "None");
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent for the cors-enabled subresource request.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request with cors disabled does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/false),
            "None");

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // When top-level site scoping is enabled, the subdomain's grant counts for
  // the less-specific domain; otherwise, it does not.
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
}

// Tests to validate `requestStorageAccessForOrigin` behavior with FPS disabled.
// For now, that entails auto-denial of requests.
class RequestStorageAccessForOriginWithFirstPartySetsDisabledBrowserTest
    : public RequestStorageAccessForOriginBaseBrowserTest {
 public:
 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{blink::features::kStorageAccessAPIForOriginExtension, {}},
            {net::features::kStorageAccessAPI, {}}};
  }
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {features::kFirstPartySets};
  }
};

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForOriginWithFirstPartySetsDisabledBrowserTest,
    PermissionAutodenied) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostD);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // `kHostD` cannot be granted access via `RequestStorageAccessForOrigin` in
  // this configuration, because the requesting site (`kHostA`) is not in the
  // same First-Party Set as the requested site (`kHostD`).
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostD).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  5 /*RequestOutcome::kDeniedByPrerequisites*/),
              Gt(0));
}

// Tests to validate that, when the `requestStorageAccessForOrigin` extension is
// explicitly disabled, or if the larger Storage Access API is disabled, it does
// not leak onto the document object.
class RequestStorageAccessForOriginExplicitlyDisabledBrowserTest
    : public RequestStorageAccessForOriginBaseBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  RequestStorageAccessForOriginExplicitlyDisabledBrowserTest()
      : enable_standard_storage_access_api_(GetParam()) {}

 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    // The test should validate that either flag alone disables the API.
    // Note that enabling the extension and not the standard API means both are
    // disabled.
    if (enable_standard_storage_access_api_) {
      return {blink::features::kStorageAccessAPIForOriginExtension};
    }
    return {net::features::kStorageAccessAPI};
  }
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    // When the standard API is enabled, return the parent class's enabled
    // feature list. Otherwise, enable only the extension; this should not take
    // effect.
    if (enable_standard_storage_access_api_) {
      return RequestStorageAccessForOriginBaseBrowserTest::GetEnabledFeatures();
    }
    return {{blink::features::kStorageAccessAPIForOriginExtension, {}}};
  }

 private:
  bool enable_standard_storage_access_api_;
};

IN_PROC_BROWSER_TEST_P(
    RequestStorageAccessForOriginExplicitlyDisabledBrowserTest,
    RsaForOriginNotPresentOnDocumentWhenExplicitlyDisabled) {
  NavigateToPageWithFrame(kHostA);
  // Ensure that the proposed extension is not available unless explicitly
  // enabled.
  EXPECT_TRUE(EvalJs(GetPrimaryMainFrame(),
                     "\"requestStorageAccessForOrigin\" in document === false")
                  .ExtractBool());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    RequestStorageAccessForOriginExplicitlyDisabledBrowserTest,
    testing::Bool());

class RequestStorageAccessForOriginWithCHIPSBrowserTest
    : public RequestStorageAccessForOriginBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RequestStorageAccessForOriginBaseBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostC, R"("])",
                      R"(, "serviceSites": ["https://)", kHostB, R"("]})"}));
  }
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        RequestStorageAccessForOriginBaseBrowserTest::GetEnabledFeatures();
    enabled.push_back({net::features::kPartitionedCookies, {}});
    enabled.push_back(
        {blink::features::kStorageAccessAPIForOriginExtension, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForOriginWithCHIPSBrowserTest,
                       RequestStorageAccessForOrigin_CoexistsWithCHIPS) {
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostB);
  SetPartitionedCookieInContext(/*top_level_host=*/kHostA,
                                /*embedded_host=*/kHostB);

  NavigateToPageWithFrame(kHostA);

  // kHostB starts without unpartitioned cookies:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test(partitioned)");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test(partitioned)");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test(partitioned)");

  // kHostA can request storage access on behalf of kHostB, and it is granted
  // (by an implicit grant):
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  // When the frame makes a subresource request to an endpoint on kHostB,
  // kHostB's unpartitioned and partitioned cookies are sent, and the iframe
  // retains storage access.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test; cross-site=b.test(partitioned)");
}

}  // namespace
