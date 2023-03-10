// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/preloading/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/preloading.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

using UkmEntry = ukm::TestUkmRecorder::HumanReadableUkmEntry;
using ukm::builders::Preloading_Attempt;
using ukm::builders::Preloading_Prediction;
static const auto kMockElapsedTime =
    base::ScopedMockElapsedTimersForTest::kMockElapsedTime;

content::PreloadingFailureReason ToPreloadingFailureReason(
    PrerenderPredictionStatus status) {
  return static_cast<content::PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(content::PreloadingFailureReason::
                           kPreloadingFailureReasonContentEnd));
}

content::PreloadingFailureReason ToPreloadingFailureReason(
    SearchPrefetchServingReason reason) {
  return static_cast<content::PreloadingFailureReason>(
      static_cast<int>(reason) +
      static_cast<int>(content::PreloadingFailureReason::
                           kPreloadingFailureReasonContentEnd));
}

// Sets up testing context for the search preloading features: search prefetch
// and search prerender.
// These features are able to coordinate with the other: A prefetched result
// might be upgraded to prerender when needed (usually when service suggests
// clients to do so), and they share the prefetched response and other
// resources, so it is a unified test designed to test the interaction between
// these two features.
class SearchPreloadUnifiedBrowserTest : public PlatformBrowserTest {
 public:
  SearchPreloadUnifiedBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &SearchPreloadUnifiedBrowserTest::GetActiveWebContents,
            base::Unretained(this))) {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2,
             {{"implementation_type", "use_prefetch"}}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "1"},
              {"device_memory_threshold_MB", "0"}}},
        },
        /*disabled_features=*/{kSearchPrefetchBlockBeforeHeaders});
  }

  void SetUp() override {
    prerender_helper().SetUp(&search_engine_server_);
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up a generic server.
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up server for search engine.
    search_engine_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_TEST_NAMES);
    search_engine_server_.RegisterRequestHandler(base::BindRepeating(
        &SearchPreloadUnifiedBrowserTest::HandleSearchRequest,
        base::Unretained(this)));
    ASSERT_TRUE(search_engine_server_.Start());

    TemplateURLService* model = TemplateURLServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_TRUE(model);
    search_test_utils::WaitForTemplateURLServiceToLoad(model);
    ASSERT_TRUE(model->loaded());
    TemplateURLData data;
    data.SetShortName(kSearchDomain16);
    data.SetKeyword(data.short_name());
    data.SetURL(
        search_engine_server_
            .GetURL(kSearchDomain,
                    "/search_page.html?q={searchTerms}&{google:prefetchSource}"
                    "type=test")
            .spec());
    data.suggestions_url =
        search_engine_server_.GetURL(kSearchDomain, "/?q={searchTerms}").spec();
    TemplateURL* template_url = model->Add(std::make_unique<TemplateURL>(data));
    ASSERT_TRUE(template_url);
    model->SetUserSelectedDefaultSearchProvider(template_url);
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<content::test::PreloadingAttemptUkmEntryBuilder>(
            chrome_preloading_predictor::kDefaultSearchEngine);
    prediction_entry_builder_ =
        std::make_unique<content::test::PreloadingPredictionUkmEntryBuilder>(
            chrome_preloading_predictor::kDefaultSearchEngine);
    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleSearchRequest(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().spec().find("favicon") != std::string::npos) {
      return nullptr;
    }

    std::string content = R"(
      <html><body>
      PRERENDER: HI PREFETCH! \o/
      </body></html>
    )";
    base::StringPairs headers = {
        {"Content-Length", base::NumberToString(content.length())},
        {"content-type", "text/html"}};
    bool is_invalid_response_body =
        request.GetURL().spec().find("invalid_content") != std::string::npos;

    std::unique_ptr<DelayedResponse> resp = std::make_unique<DelayedResponse>(
        this, service_deferral_type_, headers,
        is_invalid_response_body ? "" : content);

    if (request.GetURL().spec().find("failed_terms") != std::string::npos) {
      resp->set_code(net::HTTP_SERVICE_UNAVAILABLE);
      return resp;
    }

    return resp;
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(search_engine_server_.ShutdownAndWaitUntilComplete());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const content::test::PreloadingAttemptUkmEntryBuilder&
  attempt_entry_builder() {
    return *attempt_entry_builder_;
  }

  const content::test::PreloadingPredictionUkmEntryBuilder&
  prediction_entry_builder() {
    return *prediction_entry_builder_;
  }

 protected:
  enum class PrerenderHint { kEnabled, kDisabled };
  enum class PrefetchHint { kEnabled, kDisabled };
  enum class UrlType {
    // For URLs that will be used for a real navigation.
    kReal,
    // For URLs that will be used for prefetch requests.
    kPrefetch,
    // For URLs that will be used for prerender requests.
    kPrerender
  };

  enum class DeferralType {
    // Do not defer HTTP responses.
    kNoDeferral = 0,
    // Ddefer the response header only.
    kDeferHeader = 1,
    // Only defer the response body.
    kDeferBody = 2,
    // Defer dispatching response head until a explicit signal, and then block
    // the response until receiving the next signal.
    kDeferHeaderThenBody = 3,
  };

  void SetUpContext() {
    // Have SearchPrefetchService and PrerenderManager prepared.
    PrerenderManager::CreateForWebContents(GetActiveWebContents());
    prerender_manager_ =
        PrerenderManager::FromWebContents(GetActiveWebContents());
    ASSERT_TRUE(prerender_manager_);
    search_prefetch_service_ = SearchPrefetchServiceFactory::GetForProfile(
        chrome_test_utils::GetProfile(this));
    ASSERT_NE(nullptr, search_prefetch_service_);
  }

  GURL GetSearchUrl(const std::string& search_terms, UrlType url_type) {
    // $1: the search terms that will be retrieved.
    // $2: parameter for prefetch request. Should be &pf=cs if the url is
    // expected to declare itself as a prefetch request. Otherwise it should be
    // an empty string.
    std::string url_template = "/search_page.html?q=$1$2&type=test";
    bool attach_prefetch_flag;
    switch (url_type) {
      case UrlType::kReal:
      case UrlType::kPrerender:
        attach_prefetch_flag = false;
        break;
      case UrlType::kPrefetch:
        attach_prefetch_flag = true;
        break;
    }
    return search_engine_server_.GetURL(
        kSearchDomain,
        base::ReplaceStringPlaceholders(
            url_template, {search_terms, attach_prefetch_flag ? "&pf=cs" : ""},
            nullptr));
  }

  GURL GetCanonicalSearchURL(const GURL& prefetch_url) {
    GURL canonical_search_url;
    HasCanoncialPreloadingOmniboxSearchURL(prefetch_url,
                                           chrome_test_utils::GetProfile(this),
                                           &canonical_search_url);
    return canonical_search_url;
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

  void ChangeAutocompleteResult(const std::string& original_query,
                                const std::string& search_terms,
                                PrerenderHint prerender_hint,
                                PrefetchHint prefetch_hint) {
    AutocompleteInput input(base::ASCIIToUTF16(original_query),
                            metrics::OmniboxEventProto::BLANK,
                            ChromeAutocompleteSchemeClassifier(
                                chrome_test_utils::GetProfile(this)));
    AutocompleteMatch autocomplete_match = CreateSearchSuggestionMatch(
        original_query, search_terms, prerender_hint, prefetch_hint);
    AutocompleteResult autocomplete_result;
    autocomplete_result.AppendMatches({autocomplete_match});
    search_prefetch_service()->OnResultChanged(GetActiveWebContents(),
                                               autocomplete_result);
  }

  void WaitUntilStatusChangesTo(
      const GURL& canonical_search_url,
      std::vector<SearchPrefetchStatus> acceptable_status) {
    while (true) {
      if (absl::optional<SearchPrefetchStatus> current_status =
              search_prefetch_service()->GetSearchPrefetchStatusForTesting(
                  canonical_search_url);
          current_status &&
          base::Contains(acceptable_status, current_status.value())) {
        break;
      }
      base::RunLoop run_loop;
      run_loop.RunUntilIdle();
    }
  }

  void NavigateToPrerenderedResult(const GURL& expected_prerender_url) {
    content::TestNavigationObserver observer(GetActiveWebContents());
    GetActiveWebContents()->OpenURL(content::OpenURLParams(
        expected_prerender_url, content::Referrer(),
        WindowOpenDisposition::CURRENT_TAB,
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                  ui::PAGE_TRANSITION_FROM_ADDRESS_BAR),
        /*is_renderer_initiated=*/false));
    observer.Wait();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

  PrerenderManager* prerender_manager() { return prerender_manager_; }

  SearchPrefetchService* search_prefetch_service() {
    return search_prefetch_service_;
  }

  // Instructs the search service whether to delay the response until receiving
  // a specific signal (From callers' prospective, calling
  // `DispatchDelayedResponseTask`). See comment of `DeferralType` for more
  // information.
  void set_service_deferral_type(DeferralType service_deferral_type) {
    service_deferral_type_ = service_deferral_type;
  }

  // Called on the thread the server is running. The custom defined responses
  // should call this method if they want to defer the network response.
  void AddDelayedResponseTask(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      base::OnceClosure response_closure) {
    ASSERT_TRUE(task_runner->BelongsToCurrentThread());
    base::AutoLock auto_lock(response_queue_lock_);
    delayed_response_task_.emplace(task_runner, std::move(response_closure));
    if (monitor_callback_)
      std::move(monitor_callback_).Run();
  }

  // Called on the main thread. This will resume one delayed response.
  void DispatchDelayedResponseTask() {
    ASSERT_TRUE(
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    base::RunLoop run_loop;
    {
      base::AutoLock auto_lock(response_queue_lock_);
      if (!delayed_response_task_.empty()) {
        delayed_response_task_.front().Run();
        delayed_response_task_.pop();
        return;
      }
      monitor_callback_ = run_loop.QuitClosure();
    }
    run_loop.Run();
  }

 private:
  // A DelayedResponseTask instance is created on the thread that server is
  // running on, and be destroyed on the main thread. A lock is guarding the
  // access to created instances.
  class DelayedResponseTask {
   public:
    DelayedResponseTask(
        const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
        base::OnceClosure response_closure)
        : task_runner_(task_runner),
          response_closure_(std::move(response_closure)) {}

    void Run() {
      ASSERT_TRUE(
          content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
      task_runner_->PostTask(FROM_HERE, std::move(response_closure_));
    }

   private:
    // Task runner of the thread on which a service server is running.
    const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    // Closure for making response dispatching controllable. The closure should
    // be executed on the thread that the server is running on, as it sends
    // network response.
    base::OnceClosure response_closure_;
  };

  // Passes the delegates needed to respond to a request to the
  // SearchPreloadUnifiedBrowserTest test fixture, so that it can control when
  // and what to respond.
  class DelayedResponse : public net::test_server::BasicHttpResponse {
   public:
    // Build a custom defined response that might be deferred based on
    // `deferral_type`. See the comment of `DeferralType` for more information
    // about the deferral type. Pass an empty string to `response_body` if the
    // response (note, not the header) should fail.
    DelayedResponse(SearchPreloadUnifiedBrowserTest* test_harness,
                    DeferralType deferral_type,
                    base::StringPairs headers,
                    const std::string& response_body)
        : test_harness_(test_harness),
          service_deferral_type_(deferral_type),
          headers_(headers),
          body_(response_body) {}

    DelayedResponse(const DelayedResponse&) = delete;
    DelayedResponse& operator=(const DelayedResponse&) = delete;

    // net::test_server::BasicHttpResponse implementation.
    void SendResponse(base::WeakPtr<net::test_server::HttpResponseDelegate>
                          delegate) override {
      switch (service_deferral_type_) {
        case DeferralType::kNoDeferral:
          delegate->SendHeadersContentAndFinish(
              code(), net::GetHttpReasonPhrase(code()), headers_, body_);
          break;
        case DeferralType::kDeferHeader:
          test_harness_->AddDelayedResponseTask(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::BindOnce(&net::test_server::HttpResponseDelegate::
                                 SendHeadersContentAndFinish,
                             delegate, code(), net::GetHttpReasonPhrase(code()),
                             headers_, body_));
          break;
        case DeferralType::kDeferBody:
          test_harness_->AddDelayedResponseTask(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::BindOnce(&net::test_server::HttpResponseDelegate::
                                 SendContentsAndFinish,
                             delegate, body_));
          delegate->SendResponseHeaders(
              code(), net::GetHttpReasonPhrase(code()), headers_);
          break;
        case DeferralType::kDeferHeaderThenBody:
          test_harness_->AddDelayedResponseTask(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::BindOnce(
                  &net::test_server::HttpResponseDelegate::SendResponseHeaders,
                  delegate, code(), "OK", headers_));
          test_harness_->AddDelayedResponseTask(
              base::SingleThreadTaskRunner::GetCurrentDefault(),
              base::BindOnce(&net::test_server::HttpResponseDelegate::
                                 SendContentsAndFinish,
                             delegate, body_));
          break;
      }
    }

   private:
    // The test fixture that can manipulate the respones.
    raw_ptr<SearchPreloadUnifiedBrowserTest> test_harness_;

    // The deferral mode. See comment of `DeferralType` for more information.
    DeferralType service_deferral_type_ = DeferralType::kNoDeferral;

    // Predefined response headers.
    base::StringPairs headers_;

    // Predefined response body. The response body will fail due to the
    // CONTENT_LENGTH_MISMATCH error if it is set to an empty string.
    std::string body_ = "";
  };

  AutocompleteMatch CreateSearchSuggestionMatch(
      const std::string& original_query,
      const std::string& search_terms,
      PrerenderHint prerender_hint,
      PrefetchHint prefetch_hint) {
    AutocompleteMatch match;
    match.search_terms_args = std::make_unique<TemplateURLRef::SearchTermsArgs>(
        base::UTF8ToUTF16(search_terms));
    match.search_terms_args->original_query = base::UTF8ToUTF16(original_query);
    match.destination_url = GetSearchUrl(search_terms, UrlType::kReal);
    match.keyword = base::UTF8ToUTF16(original_query);
    if (prerender_hint == PrerenderHint::kEnabled)
      match.RecordAdditionalInfo("should_prerender", "true");
    if (prefetch_hint == PrefetchHint::kEnabled)
      match.RecordAdditionalInfo("should_prefetch", "true");
    return match;
  }

  constexpr static char kSearchDomain[] = "a.test";
  constexpr static char16_t kSearchDomain16[] = u"a.test";
  raw_ptr<PrerenderManager, DanglingUntriaged> prerender_manager_ = nullptr;
  raw_ptr<SearchPrefetchService, DanglingUntriaged> search_prefetch_service_ =
      nullptr;
  net::test_server::EmbeddedTestServer search_engine_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};

  DeferralType service_deferral_type_ = DeferralType::kNoDeferral;
  std::queue<DelayedResponseTask> delayed_response_task_
      GUARDED_BY(response_queue_lock_);
  base::OnceClosure monitor_callback_ GUARDED_BY(response_queue_lock_);
  base::Lock response_queue_lock_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<content::test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;
  std::unique_ptr<content::test::PreloadingPredictionUkmEntryBuilder>
      prediction_entry_builder_;

  content::test::PrerenderTestHelper prerender_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
};

// Tests that the SearchSuggestionService can trigger prerendering after the
// corresponding prefetch request succeeds.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderHintReceivedBeforeSucceed) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // Snapshot those samples recorded before the main test.
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.PrefetchServingReason2", 1);

  std::string search_query = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint expected_prerender_url, and prerendering
  // for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kPrerendered, 1);

  // Prefetch should be triggered as well.
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);

  // No prerender requests went through network, so there should be only one
  // request and it is with the prefetch flag attached.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrerenderActivated, 1);

  // On prerender activation, `URLLoaderRequestInterceptor` would not be called,
  // so no more sample should be recorded.
  histogram_tester.ExpectTotalCount(
      "Omnibox.SearchPrefetch.PrefetchServingReason2", 1);
  {
    // Check that we store one entry corresponding to the prerender prediction
    // and attempt with prefetch hints.
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto attempt_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    auto prediction_ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Prediction::kEntryName,
        content::test::kPreloadingPredictionUkmMetrics);
    EXPECT_EQ(attempt_ukm_entries.size(), 2u);
    EXPECT_EQ(prediction_ukm_entries.size(), 1u);

    // Prerender should succeed and should be used for the next navigation.
    std::vector<UkmEntry> expected_prediction_entries = {
        prediction_entry_builder().BuildEntry(ukm_source_id,
                                              /*confidence=*/80,
                                              /*accurate_prediction=*/true),
    };
    std::vector<UkmEntry> expected_attempt_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::
                kTriggeredButUpgradedToPrerender,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(attempt_ukm_entries,
                testing::UnorderedElementsAreArray(expected_attempt_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               attempt_ukm_entries, expected_attempt_entries);
    EXPECT_THAT(prediction_ukm_entries,
                testing::UnorderedElementsAreArray(expected_prediction_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(
               prediction_ukm_entries, expected_prediction_entries);
  }

  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
}

// Tests that the SearchSuggestionService can trigger prerendering if it
// receives prerender hints after the previous prefetch request succeeds.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderHintReceivedAfterSucceed) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // Wait until prefetch request succeeds.
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(expected_prefetch_url),
      {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});
  std::string search_query_2 = "prer";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint `expected_prefetch_url`, and
  // prerendering for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);

  // No prerender requests went through network, so there should be only one
  // request and it is with the prefetch flag attached.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  // Activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrerenderActivated, 1);

  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 3u);

    // Prerender should succeed and should be used for the next navigation.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::
                kTriggeredButUpgradedToPrerender,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kDuplicate,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  // No prerender requests went through network.
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
}

// Tests that the SearchSuggestionService will not trigger prerender if the
// prefetch failed.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       FailedPrefetchCannotBeUpgraded) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query = "fail";
  std::string prerender_query = "failed_terms";

  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // Prefetch should be triggered, and the prefetch request should fail.
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(
              GetSearchUrl(prerender_query, UrlType::kPrerender)));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(GetSearchUrl(prerender_query, UrlType::kPrerender)),
      {SearchPrefetchStatus::kRequestFailed});

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.FetchResult.SuggestionPrefetch", false, 1);
  EXPECT_FALSE(prerender_manager()->HasSearchResultPagePrerendered());
}

// Tests that the SearchSuggestionService will not trigger prerender if the
// suggestions changes before SearchSuggestionService receives a servable
// response.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       SuggestionChangeBeforeStartPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  set_service_deferral_type(DeferralType::kDeferHeader);

  // 1. Type the first query.
  std::string search_query_1 = "hang";
  std::string prerender_query_1 = "hang_response";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrefetch);
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch should be triggered.
  auto prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kInFlight});

  // 3. Type a different query which results in different suggestions.
  std::string search_query_2 = "pre";
  ChangeAutocompleteResult(search_query_2, search_query_2,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // 4. The old prefetch should be cancelled.
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(
              GetSearchUrl(prerender_query_1, UrlType::kPrerender)));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(GetSearchUrl(
                               prerender_query_1, UrlType::kPrerender)),
                           {SearchPrefetchStatus::kRequestCancelled});

  EXPECT_FALSE(prerender_manager()->HasSearchResultPagePrerendered());
}

// Tests prerender is cancelled after SearchPrefetchService cancels prefetch
// requests.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       SuggestionChangeAfterStartPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query_1 = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(
              GetSearchUrl(prerender_query_1, UrlType::kPrerender)));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kPrerendered, 1);

  // 3. Type a different query which results in different suggestions.
  std::string search_query_2 = "pre";
  ChangeAutocompleteResult(search_query_2, search_query_2,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // 4. The prerender will be destroyed automatically.
  prerender_observer.WaitForDestroyed();
  histogram_tester.ExpectUniqueSample(
      internal::kHistogramPrerenderPredictionStatusDefaultSearchEngine,
      PrerenderPredictionStatus::kCancelled, 1);

  // Navigate away to flush the metrics.
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), expected_prerender_url));
  {
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 3u);

    // Prerender shouldn't be used for the next navigation as it will be deleted
    // when suggestions change.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            ToPreloadingFailureReason(
                SearchPrefetchServingReason::kRequestWasCancelled),
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            ToPreloadingFailureReason(PrerenderPredictionStatus::kCancelled),
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }
}

// Tests the activated prerendered page records navigation timings correctly.
// Though the prerender happens before the activation navigation, the timings
// should not be a negative value, so that the activated page can measure the
// timing correctly.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       SetLoaderTimeCorrectly) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // 2. Wait until prefetch completed.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kComplete});

  // 3. Type a longer one.
  std::string search_query_2 = "preren";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);

  // 4. Activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();

  // Check the response time is non-negative.
  std::string script =
      "window.domAutomationController.send(window.performance.timing."
      "responseEnd - window.performance.timing.responseStart)";
  EXPECT_LE(0, content::EvalJs(GetActiveWebContents(), script,
                               content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  // Check the response start is after (or the same as) request start.
  script =
      "window.domAutomationController.send(window.performance.timing."
      "responseStart - window.performance.timing.requestStart)";
  EXPECT_LE(0, content::EvalJs(GetActiveWebContents(), script,
                               content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));

  // Check request start is after (or the same as) navigation start.
  script =
      "window.domAutomationController.send(window.performance.timing."
      "requestStart - window.performance.timing.navigationStart)";
  EXPECT_LE(0, content::EvalJs(GetActiveWebContents(), script,
                               content::EXECUTE_SCRIPT_USE_MANUAL_REPLY));
}

// Tests that prerender fails as well if the prefetch response that prerender
// uses fails.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       NavigationFailsAfterPrefetchServedTheResponse) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  set_service_deferral_type(DeferralType::kDeferBody);
  SetUpContext();

  // 1. Type the first query.
  std::string search_query_1 = "invalid";
  std::string prerender_query_1 = "invalid_content";

  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered.
  registry_observer.WaitForTrigger(expected_prerender_url);

  // 3. Wait until prerender uses the prefetched response.
  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kPrerendered});
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));

  // 4. Make the prerender fail to read the response body by sending "Finish"
  // signal before sending content body.
  DispatchDelayedResponseTask();
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);

  // 5. The prerender will be destroyed because of the failing request.
  prerender_observer.WaitForDestroyed();

  // Navigate away to flush the metrics.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kNavigatedUrl));

  {
    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    // DispatchDelayedResponseTask will dispatch DidFailLoadWithError resulting
    // in prerender cancelling with status 123 i.e., =>
    // PrerenderFinalStatus::DidFailLoad.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::
                kTriggeredButUpgradedToPrerender,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kFailure,
            static_cast<content::PreloadingFailureReason>(123),
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  // Prerender should not retry the request.
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
}

// Tests prerender is cancelled after SearchPrefetchService cancels prefetch
// requests.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest, DoNotRefetchSameTerms) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kNavigatedUrl = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered, and chrome is waiting for
  // the body.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);

  // 3. Type a different query which results in the same suggestion.
  std::string search_query_2 = "prer";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 4. Do not prefetch/prerender again.
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);

  // Navigate away to flush the metrics.
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kNavigatedUrl));
  {
    // Check that we log the correct PreloadingEligibility metrics when
    // prerender is consumed.
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 4u);

    ukm::SourceId ukm_source_id =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::
                kTriggeredButUpgradedToPrerender,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kReady,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kDuplicate,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            ToPreloadingEligibility(
                ChromePreloadingEligibility::kPrerenderConsumed),
            content::PreloadingHoldbackStatus::kUnspecified,
            content::PreloadingTriggeringOutcome::kUnspecified,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/false),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }

  histogram_tester.ExpectBucketCount(
      "Omnibox.SearchPrefetch.PrefetchEligibilityReason2.SuggestionPrefetch",
      SearchPrefetchEligibilityReason::kAttemptedQueryRecently, 1);
}

class SearchPreloadUnifiedHoldbackBrowserTest
    : public SearchPreloadUnifiedBrowserTest {
 public:
  SearchPreloadUnifiedHoldbackBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2,
             {{"implementation_type", "use_prefetch"}}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "4"},
              {"device_memory_threshold_MB", "0"}}},
            {features::kPrerender2Holdback, {{}}},
        },
        /*disabled_features=*/{kSearchPrefetchBlockBeforeHeaders});
  }
  ~SearchPreloadUnifiedHoldbackBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that we log correct metrics for Prerender holdback in case of Search
// Prerender.
// TODO(https://crbug.com/1350862): This test is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedHoldbackBrowserTest,
                       DISABLED_PrerenderUnifiedHoldbackTest) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kDisabled, PrefetchHint::kEnabled);

  // Wait until prefetch request succeeds.
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  WaitUntilStatusChangesTo(
      GetCanonicalSearchURL(expected_prefetch_url),
      {SearchPrefetchStatus::kCanBeServed, SearchPrefetchStatus::kComplete});
  std::string search_query_2 = "prer";
  ChangeAutocompleteResult(search_query_2, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // The suggestion service should hint `expected_prefetch_url`, and
  // prerendering for this url should start.
  registry_observer.WaitForTrigger(expected_prerender_url);

  // Navigate to flush the metrics.
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  ASSERT_TRUE(
      content::NavigateToURL(GetActiveWebContents(), expected_prerender_url));
  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 3u);

    // Prerender should be under holdback and not succeed.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kDuplicate,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kHoldback,
            content::PreloadingTriggeringOutcome::kUnspecified,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }
}

// Disables BFCache for testing back forward navigation can reuse the HTTP
// Cache.
class HTTPCacheSearchPreloadUnifiedBrowserTest
    : public SearchPreloadUnifiedBrowserTest {
 public:
  HTTPCacheSearchPreloadUnifiedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2,
             {{"implementation_type", "use_prefetch"}}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "4"},
              {"device_memory_threshold_MB", "0"}}},
        },
        // Disable BackForwardCache to ensure that the page is not restored from
        // the cache.
        /*disabled_features=*/{kSearchPrefetchBlockBeforeHeaders,
                               features::kBackForwardCache});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test back or forward navigations can use the HTTP Cache.
IN_PROC_BROWSER_TEST_F(HTTPCacheSearchPreloadUnifiedBrowserTest,
                       BackwardHitHttpCache) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  std::string search_query_1 = "pre";
  std::string prerender_query_1 = "prerender";
  GURL expected_prefetch_url_1 =
      GetSearchUrl(prerender_query_1, UrlType::kPrefetch);
  GURL expected_prerender_url_1 =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);
  auto trigger_and_activate = [&](const std::string& search_query,
                                  const std::string& prerender_query) {
    GURL expected_prefetch_url =
        GetSearchUrl(prerender_query, UrlType::kPrefetch);
    GURL expected_prerender_url =
        GetSearchUrl(prerender_query, UrlType::kPrerender);
    ChangeAutocompleteResult(search_query, prerender_query,
                             PrerenderHint::kEnabled, PrefetchHint::kEnabled);
    registry_observer.WaitForTrigger(expected_prerender_url);
    WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prefetch_url),
                             {SearchPrefetchStatus::kCanBeServed,
                              SearchPrefetchStatus::kPrerendered});
    // No prerender requests went through network, so there should be only one
    // request and it is with the prefetch flag attached.
    EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
    EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

    // Activate.
    content::test::PrerenderHostObserver prerender_observer(
        *GetActiveWebContents(), expected_prerender_url);
    NavigateToPrerenderedResult(expected_prerender_url);
    prerender_observer.WaitForActivation();

    // No prerender requests went through network.
    EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
    EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  };

  trigger_and_activate(search_query_1, prerender_query_1);
  // Trigger another preloading attempt and navigate to that page.
  trigger_and_activate("pref", "prefetch");

  // Navigate back. Chrome is supposed to read the response from the cache,
  // instead of sending another request.
  content::TestNavigationObserver back_load_observer(GetActiveWebContents());
  GetActiveWebContents()->GetController().GoBack();
  back_load_observer.Wait();
  EXPECT_EQ(expected_prerender_url_1,
            GetActiveWebContents()->GetLastCommittedURL());
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url_1));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url_1));
}

// Tests the started prerender is destroyed after prefetch request expired.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderGetDestroyedAfterPrefetchExpired) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();

  // Trigger prerender and prefetch.
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());
  std::string search_query = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  ChangeAutocompleteResult(search_query, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  auto prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_TRUE(prefetch_status.has_value());

  WaitUntilStatusChangesTo(GetCanonicalSearchURL(expected_prerender_url),
                           {SearchPrefetchStatus::kPrerendered});

  // Fire the timer to make all prefetch requests expire.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  search_prefetch_service()->FireAllExpiryTimerForTesting();
  prerender_observer.WaitForDestroyed();
  prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prerender_url));
  EXPECT_FALSE(prefetch_status.has_value());

  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrerendered, 1);
}

// TODO(https://cubug.com/1282624): This test should run on Android after we're
// able to interact with Android UI.
// TODO(https://crbug.com/1342481): On LacrOS, the window's bound changes
// unexpectedly, and it stops auto completing.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest, TriggerAndActivate) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);

  // 2. Prepare some context.
  AutocompleteInput input(
      base::ASCIIToUTF16(prerender_query), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));

  // 3. Trigger prerender and prefetch.
  autocomplete_controller->Start(input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kPrerendered, 1);

  // 4. Click and activate.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);
  prerender_observer.WaitForActivation();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrerenderActivated, 1);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
}

// Tests the metrics for analyzing the unideal scenario that prerender fails
// after taking response away. Without prerender, these prefetches could have
// helped improve the performance of loading SRPs, so it is necessary to
// understand the percentage of failing ones.
IN_PROC_BROWSER_TEST_F(SearchPreloadUnifiedBrowserTest,
                       PrerenderFailAfterResponseServed) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query = "prerender";
  GURL expected_prefetch_url =
      GetSearchUrl(prerender_query, UrlType::kPrefetch);
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query, UrlType::kPrerender);
  GURL expected_real_url = GetSearchUrl(prerender_query, UrlType::kReal);

  // 2. Prepare some context.
  AutocompleteInput input(
      base::ASCIIToUTF16(prerender_query), metrics::OmniboxEventProto::BLANK,
      ChromeAutocompleteSchemeClassifier(browser()->profile()));
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  AutocompleteController* autocomplete_controller =
      omnibox->model()->autocomplete_controller();

  // Prevent the stop timer from killing the hints fetch early.
  autocomplete_controller->SetStartStopTimerDurationForTesting(
      base::Seconds(10));

  // 3. Trigger prerender and prefetch.
  autocomplete_controller->Start(input);
  ui_test_utils::WaitForAutocompleteDone(browser());
  ChangeAutocompleteResult(search_query_1, prerender_query,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          GetCanonicalSearchURL(expected_prefetch_url));
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kPrerendered, 1);

  // 4. Fail the prerender by navigating it to another page.
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  int host_id = prerender_helper().GetHostForUrl(expected_prerender_url);
  ASSERT_NE(host_id, content::RenderFrameHost::kNoFrameTreeNodeId);
  prerender_helper().NavigatePrerenderedPage(host_id, expected_prefetch_url);
  prerender_observer.WaitForDestroyed();
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(0, prerender_helper().GetRequestCount(expected_prerender_url));

  // 5. Click the result.
  content::TestNavigationObserver navigation_observer(GetActiveWebContents(),
                                                      1);
  omnibox->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);
  navigation_observer.Wait();

  // 6. Fire the timer to make all prefetch requests expire
  search_prefetch_service()->FireAllExpiryTimerForTesting();
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchFinalStatus.SuggestionPrefetch",
      SearchPrefetchStatus::kPrerenderedAndClicked, 1);
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_prefetch_url));
  EXPECT_EQ(1, prerender_helper().GetRequestCount(expected_real_url));
}

class NoCancelSearchPreloadUnifiedBrowserTest
    : public SearchPreloadUnifiedBrowserTest {
 public:
  NoCancelSearchPreloadUnifiedBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kSupportSearchSuggestionForPrerender2,
             {{"implementation_type", "use_prefetch"}}},
            {kSearchPrefetchSkipsCancel, {}},
            {kSearchPrefetchServicePrefetching,
             {{"max_attempts_per_caching_duration", "3"},
              {"cache_size", "4"},
              {"device_memory_threshold_MB", "0"}}},
        },
        // Disable BackForwardCache to ensure that the page is not restored from
        // the cache.
        /*disabled_features=*/{kSearchPrefetchBlockBeforeHeaders,
                               features::kBackForwardCache});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests prerender is not cancelled after SearchPrefetchService cancels prefetch
// requests.
IN_PROC_BROWSER_TEST_F(NoCancelSearchPreloadUnifiedBrowserTest,
                       SuggestionChangeAfterStartPrerender) {
  base::HistogramTester histogram_tester;
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(GetActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), kInitialUrl));
  SetUpContext();
  content::test::PrerenderHostRegistryObserver registry_observer(
      *GetActiveWebContents());

  // 1. Type the first query.
  std::string search_query_1 = "pre";
  std::string prerender_query_1 = "prerender";
  GURL expected_prerender_url =
      GetSearchUrl(prerender_query_1, UrlType::kPrerender);
  GURL canonical_search_url = GetCanonicalSearchURL(expected_prerender_url);
  ChangeAutocompleteResult(search_query_1, prerender_query_1,
                           PrerenderHint::kEnabled, PrefetchHint::kEnabled);

  // 2. Prefetch and prerender should be triggered.
  registry_observer.WaitForTrigger(expected_prerender_url);
  prerender_helper().WaitForPrerenderLoadCompletion(*GetActiveWebContents(),
                                                    expected_prerender_url);
  EXPECT_TRUE(prerender_manager()->HasSearchResultPagePrerendered());
  absl::optional<SearchPrefetchStatus> prefetch_status =
      search_prefetch_service()->GetSearchPrefetchStatusForTesting(
          canonical_search_url);
  EXPECT_TRUE(prefetch_status.has_value());
  EXPECT_EQ(prefetch_status.value(), SearchPrefetchStatus::kPrerendered);
  content::test::PrerenderHostObserver prerender_observer(
      *GetActiveWebContents(), expected_prerender_url);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.SearchPrefetch.PrefetchServingReason2.Prerender",
      SearchPrefetchServingReason::kPrerendered, 1);

  // 3. Type a different query which results in different suggestions.
  std::string search_query_2 = "pre";
  ChangeAutocompleteResult(search_query_2, search_query_2,
                           PrerenderHint::kDisabled, PrefetchHint::kDisabled);

  // 4. Navigate to the initial prerender.
  content::NavigationHandleObserver activation_observer(GetActiveWebContents(),
                                                        expected_prerender_url);
  NavigateToPrerenderedResult(expected_prerender_url);
  prerender_observer.WaitForActivation();
  {
    ukm::SourceId ukm_source_id = activation_observer.next_page_ukm_source_id();
    auto ukm_entries = test_ukm_recorder()->GetEntries(
        Preloading_Attempt::kEntryName,
        content::test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(ukm_entries.size(), 2u);

    // Prerender should be used for the next navigation as it won't be cancelled
    // when suggestions change.
    std::vector<UkmEntry> expected_entries = {
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrefetch,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::
                kTriggeredButUpgradedToPrerender,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
        attempt_entry_builder().BuildEntry(
            ukm_source_id, content::PreloadingType::kPrerender,
            content::PreloadingEligibility::kEligible,
            content::PreloadingHoldbackStatus::kAllowed,
            content::PreloadingTriggeringOutcome::kSuccess,
            content::PreloadingFailureReason::kUnspecified,
            /*accurate=*/true,
            /*ready_time=*/kMockElapsedTime),
    };
    EXPECT_THAT(ukm_entries,
                testing::UnorderedElementsAreArray(expected_entries))
        << content::test::ActualVsExpectedUkmEntriesToString(ukm_entries,
                                                             expected_entries);
  }
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace
