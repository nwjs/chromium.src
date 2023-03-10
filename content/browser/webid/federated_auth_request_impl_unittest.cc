// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/federated_auth_request_impl.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/test/delegated_idp_network_request_manager.h"
#include "content/browser/webid/test/federated_auth_request_request_token_callback_helper.h"
#include "content/browser/webid/test/mock_api_permission_delegate.h"
#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"
#include "content/browser/webid/test/mock_idp_network_request_manager.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/browser/webid/webid_utils.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_features.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RequestTokenStatus;
using AccountList = content::IdpNetworkRequestManager::AccountList;
using ApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using AuthRequestCallbackHelper =
    content::FederatedAuthRequestRequestTokenCallbackHelper;
using DismissReason = content::IdentityRequestDialogController::DismissReason;
using FedCmEntry = ukm::builders::Blink_FedCm;
using FedCmIdpEntry = ukm::builders::Blink_FedCmIdp;
using FetchStatus = content::IdpNetworkRequestManager::FetchStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using TokenStatus = content::FedCmRequestIdTokenStatus;
using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using SignInStateMatchStatus = content::FedCmSignInStateMatchStatus;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;

namespace content {

namespace {

constexpr char kProviderUrlFull[] = "https://idp.example/fedcm.json";
constexpr char kRpUrl[] = "https://rp.example/";
constexpr char kRpOtherUrl[] = "https://rp.example/random/";
constexpr char kAccountsEndpoint[] = "https://idp.example/accounts";
constexpr char kCrossOriginAccountsEndpoint[] = "https://idp2.example/accounts";
constexpr char kTokenEndpoint[] = "https://idp.example/token";
constexpr char kClientMetadataEndpoint[] =
    "https://idp.example/client_metadata";
constexpr char kMetricsEndpoint[] = "https://idp.example/metrics";
constexpr char kPrivacyPolicyUrl[] = "https://rp.example/pp";
constexpr char kTermsOfServiceUrl[] = "https://rp.example/tos";
constexpr char kClientId[] = "client_id_123";
constexpr char kNonce[] = "nonce123";
constexpr char kAccountId[] = "1234";
constexpr char kAccountIdNicolas[] = "nico_id";
constexpr char kAccountIdPeter[] = "peter_id";
constexpr char kAccountIdZach[] = "zach_id";
constexpr char kEmail[] = "ken@idp.example";

// Values will be added here as token introspection is implemented.
constexpr char kToken[] = "[not a real token]";
constexpr char kEmptyToken[] = "";

static const std::initializer_list<IdentityRequestAccount> kAccounts{{
    kAccountId,        // id
    kEmail,            // email
    "Ken R. Example",  // name
    "Ken",             // given_name
    GURL()             // picture
}};

static const std::initializer_list<IdentityRequestAccount> kMultipleAccounts{
    {
        kAccountIdNicolas,    // id
        "nicolas@email.com",  // email
        "Nicolas P",          // name
        "Nicolas",            // given_name
        GURL(),               // picture
        LoginState::kSignUp   // login_state
    },
    {
        kAccountIdPeter,     // id
        "peter@email.com",   // email
        "Peter K",           // name
        "Peter",             // given_name
        GURL(),              // picture
        LoginState::kSignIn  // login_state
    },
    {
        kAccountIdZach,      // id
        "zach@email.com",    // email
        "Zachary T",         // name
        "Zach",              // given_name
        GURL(),              // picture
        LoginState::kSignUp  // login_state
    }};

static const std::set<std::string> kWellKnown{kProviderUrlFull};

struct LoginHint {
  const char* email = "";
  const char* id = "";
  bool is_required = false;
};

struct IdentityProviderParameters {
  const char* provider;
  const char* client_id;
  const char* nonce;
  LoginHint login_hint;
};

// Parameters for a call to RequestToken.
struct RequestParameters {
  std::vector<IdentityProviderParameters> identity_providers;
  bool prefer_auto_sign_in;
  blink::mojom::RpContext rp_context;
};

// Expected return values from a call to RequestToken.
//
// DO NOT ADD NEW MEMBERS.
// Having a lot of members in RequestExpectations encourages bad test design.
// Specifically:
// - It encourages making the test harness more magic
// - It makes each test "test everything", making it really hard to determine
//   at a later date what the test was actually testing.

struct RequestExpectations {
  absl::optional<RequestTokenStatus> return_status;
  std::vector<FederatedAuthRequestResult> devtools_issue_statuses;
  absl::optional<std::string> selected_idp_config_url;
};

// Mock configuration values for test.
struct MockClientIdConfiguration {
  FetchStatus fetch_status;
  std::string privacy_policy_url;
  std::string terms_of_service_url;
};

struct MockWellKnown {
  std::set<std::string> provider_urls;
};

// Mock information returned from IdpNetworkRequestManager::FetchConfig().
struct MockConfig {
  FetchStatus fetch_status;
  std::string accounts_endpoint;
  std::string token_endpoint;
  std::string client_metadata_endpoint;
  std::string metrics_endpoint;
};

struct MockIdpInfo {
  MockWellKnown well_known;
  MockConfig config;
  MockClientIdConfiguration client_metadata;
  FetchStatus accounts_response;
  AccountList accounts;
};

// Action on accounts dialog taken by TestDialogController. Does not indicate a
// test expectation.
enum class AccountsDialogAction {
  kNone,
  kClose,
  kSelectFirstAccount,
};

// Action on IdP-sign-in-status-mismatch dialog taken by TestDialogController.
// Does not indicate a test expectation.
enum class IdpSigninStatusMismatchDialogAction {
  kNone,
  kClose,
};

struct MockConfiguration {
  const char* token;
  base::flat_map<std::string, MockIdpInfo> idp_info;
  FetchStatus token_response;
  bool delay_token_response;
  AccountsDialogAction accounts_dialog_action;
  IdpSigninStatusMismatchDialogAction idp_signin_status_mismatch_dialog_action;
  bool wait_for_callback;
};

static const MockClientIdConfiguration kDefaultClientMetadata{
    {ParseStatus::kSuccess, net::HTTP_OK},
    kPrivacyPolicyUrl,
    kTermsOfServiceUrl};

static const IdentityProviderParameters kDefaultIdentityProviderConfig{
    kProviderUrlFull, kClientId, kNonce};

static const RequestParameters kDefaultRequestParameters{
    std::vector<IdentityProviderParameters>{kDefaultIdentityProviderConfig},
    /*prefer_auto_sign_in=*/false, blink::mojom::RpContext::kSignIn};

static const MockIdpInfo kDefaultIdentityProviderInfo{
    {kWellKnown},
    {
        {ParseStatus::kSuccess, net::HTTP_OK},
        kAccountsEndpoint,
        kTokenEndpoint,
        kClientMetadataEndpoint,
        kMetricsEndpoint,
    },
    kDefaultClientMetadata,
    {ParseStatus::kSuccess, net::HTTP_OK},
    kAccounts,
};

static const base::flat_map<std::string, MockIdpInfo> kSingleProviderInfo{
    {kProviderUrlFull, kDefaultIdentityProviderInfo}};

constexpr char kProviderTwoUrlFull[] = "https://idp2.example/fedcm.json";
static const MockIdpInfo kProviderTwoInfo{
    {{kProviderTwoUrlFull}},
    {
        {ParseStatus::kSuccess, net::HTTP_OK},
        "https://idp2.example/accounts",
        "https://idp2.example/token",
        "https://idp2.example/client_metadata",
        "https://idp2.example/metrics",
    },
    kDefaultClientMetadata,
    {ParseStatus::kSuccess, net::HTTP_OK},
    kMultipleAccounts};

static const MockConfiguration kConfigurationValid{
    kToken,
    kSingleProviderInfo,
    {ParseStatus::kSuccess, net::HTTP_OK},
    /*delay_token_response=*/false,
    AccountsDialogAction::kSelectFirstAccount,
    IdpSigninStatusMismatchDialogAction::kNone,
    /*wait_for_callback=*/true};

static const RequestExpectations kExpectationSuccess{
    RequestTokenStatus::kSuccess,
    {FederatedAuthRequestResult::kSuccess},
    kProviderUrlFull};

static const RequestParameters kDefaultMultiIdpRequestParameters{
    std::vector<IdentityProviderParameters>{
        {kProviderUrlFull, kClientId, kNonce},
        {kProviderTwoUrlFull, kClientId, kNonce}},
    /*prefer_auto_sign_in=*/false,
    /*rp_context=*/blink::mojom::RpContext::kSignIn};

MockConfiguration kConfigurationMultiIdpValid{
    kToken,
    {{kProviderUrlFull, kDefaultIdentityProviderInfo},
     {kProviderTwoUrlFull, kProviderTwoInfo}},
    {ParseStatus::kSuccess, net::HTTP_OK},
    false /* delay_token_response */,
    AccountsDialogAction::kSelectFirstAccount,
    IdpSigninStatusMismatchDialogAction::kNone,
    true /* wait_for_callback */};

url::Origin OriginFromString(const std::string& url_string) {
  return url::Origin::Create(GURL(url_string));
}

enum class FetchedEndpoint {
  CONFIG,
  CLIENT_METADATA,
  ACCOUNTS,
  TOKEN,
  WELL_KNOWN,
};

class TestIdpNetworkRequestManager : public MockIdpNetworkRequestManager {
 public:
  void SetTestConfig(const MockConfiguration& configuration) {
    config_ = configuration;
  }

  void RunDelayedCallbacks() {
    for (base::OnceClosure& delayed_callback : delayed_callbacks_) {
      std::move(delayed_callback).Run();
    }
    delayed_callbacks_.clear();
  }

  void FetchWellKnown(const GURL& provider,
                      FetchWellKnownCallback callback) override {
    ++num_fetched_[FetchedEndpoint::WELL_KNOWN];

    std::string provider_key = provider.spec();
    std::set<GURL> url_set(
        config_.idp_info[provider_key].well_known.provider_urls.begin(),
        config_.idp_info[provider_key].well_known.provider_urls.end());
    FetchStatus success{ParseStatus::kSuccess, net::HTTP_OK};
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), success, url_set));
  }

  void FetchConfig(const GURL& provider,
                   int idp_brand_icon_ideal_size,
                   int idp_brand_icon_minimum_size,
                   FetchConfigCallback callback) override {
    ++num_fetched_[FetchedEndpoint::CONFIG];

    std::string provider_key = provider.spec();
    IdpNetworkRequestManager::Endpoints endpoints;
    endpoints.token =
        GURL(config_.idp_info[provider_key].config.token_endpoint);
    endpoints.accounts =
        GURL(config_.idp_info[provider_key].config.accounts_endpoint);
    endpoints.client_metadata =
        GURL(config_.idp_info[provider_key].config.client_metadata_endpoint);
    endpoints.metrics =
        GURL(config_.idp_info[provider_key].config.metrics_endpoint);

    IdentityProviderMetadata idp_metadata;
    idp_metadata.config_url = provider;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       config_.idp_info[provider_key].config.fetch_status,
                       endpoints, idp_metadata));
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    ++num_fetched_[FetchedEndpoint::CLIENT_METADATA];

    // Find the info of the provider with the same client metadata endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.client_metadata_endpoint) == endpoint)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), info.client_metadata.fetch_status,
                       IdpNetworkRequestManager::ClientMetadata{
                           GURL(info.client_metadata.privacy_policy_url),
                           GURL(info.client_metadata.terms_of_service_url)}));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    ++num_fetched_[FetchedEndpoint::ACCOUNTS];

    // Find the info of the provider with the same accounts endpoint.
    MockIdpInfo info;
    for (const auto& idp_info : config_.idp_info) {
      info = idp_info.second;
      if (GURL(info.config.accounts_endpoint) == accounts_url)
        break;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), info.accounts_response,
                                  info.accounts));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& url_encoded_post_data,
                        TokenRequestCallback callback) override {
    ++num_fetched_[FetchedEndpoint::TOKEN];

    std::string delivered_token =
        config_.token_response.parse_status == ParseStatus::kSuccess
            ? config_.token
            : std::string();
    base::OnceCallback bound_callback = base::BindOnce(
        std::move(callback), config_.token_response, delivered_token);
    if (config_.delay_token_response) {
      delayed_callbacks_.push_back(std::move(bound_callback));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(bound_callback));
    }
  }

  std::map<FetchedEndpoint, size_t> num_fetched_;

 protected:
  MockConfiguration config_{kConfigurationValid};
  std::vector<base::OnceClosure> delayed_callbacks_;
};

// TestIdpNetworkRequestManager subclass which checks the values of the method
// params when executing an endpoint request.
class IdpNetworkRequestManagerParamChecker
    : public TestIdpNetworkRequestManager {
 public:
  void SetExpectations(const std::string& expected_client_id,
                       const std::string& expected_selected_account_id) {
    expected_client_id_ = expected_client_id;
    expected_selected_account_id_ = expected_selected_account_id;
  }

  void SetExpectedTokenPostData(
      const std::string& expected_url_encoded_post_data) {
    expected_url_encoded_post_data_ = expected_url_encoded_post_data;
  }

  void FetchClientMetadata(const GURL& endpoint,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::FetchClientMetadata(endpoint, client_id,
                                                      std::move(callback));
  }

  void SendAccountsRequest(const GURL& accounts_url,
                           const std::string& client_id,
                           AccountsRequestCallback callback) override {
    if (expected_client_id_)
      EXPECT_EQ(expected_client_id_, client_id);
    TestIdpNetworkRequestManager::SendAccountsRequest(accounts_url, client_id,
                                                      std::move(callback));
  }

  void SendTokenRequest(const GURL& token_url,
                        const std::string& account,
                        const std::string& url_encoded_post_data,
                        TokenRequestCallback callback) override {
    if (expected_selected_account_id_)
      EXPECT_EQ(expected_selected_account_id_, account);
    if (expected_url_encoded_post_data_)
      EXPECT_EQ(expected_url_encoded_post_data_, url_encoded_post_data);
    TestIdpNetworkRequestManager::SendTokenRequest(
        token_url, account, url_encoded_post_data, std::move(callback));
  }

 private:
  absl::optional<std::string> expected_client_id_;
  absl::optional<std::string> expected_selected_account_id_;
  absl::optional<std::string> expected_url_encoded_post_data_;
};

class TestDialogController
    : public NiceMock<MockIdentityRequestDialogController> {
 public:
  struct State {
    AccountList displayed_accounts;
    absl::optional<IdentityRequestAccount::SignInMode> sign_in_mode;
    bool did_show_idp_signin_status_mismatch_dialog{false};
    blink::mojom::RpContext rp_context;
  };

  explicit TestDialogController(MockConfiguration config)
      : accounts_dialog_action_(config.accounts_dialog_action),
        idp_signin_status_mismatch_dialog_action_(
            config.idp_signin_status_mismatch_dialog_action) {}

  ~TestDialogController() override = default;
  TestDialogController(TestDialogController&) = delete;
  TestDialogController& operator=(TestDialogController&) = delete;

  void SetState(State* state) { state_ = state; }

  void ShowAccountsDialog(
      WebContents* rp_web_contents,
      const std::string& rp_for_display,
      const std::vector<IdentityProviderData>& identity_provider_data,
      IdentityRequestAccount::SignInMode sign_in_mode,
      IdentityRequestDialogController::AccountSelectionCallback on_selected,
      IdentityRequestDialogController::DismissCallback dismiss_callback)
      override {
    if (!state_) {
      return;
    }

    state_->sign_in_mode = sign_in_mode;
    state_->rp_context = identity_provider_data[0].rp_context;

    base::span<const content::IdentityRequestAccount> accounts =
        identity_provider_data[0].accounts;
    state_->displayed_accounts = AccountList(accounts.begin(), accounts.end());

    switch (accounts_dialog_action_) {
      case AccountsDialogAction::kSelectFirstAccount: {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(on_selected),
                           identity_provider_data[0].idp_metadata.config_url,
                           accounts[0].id,
                           accounts[0].login_state == LoginState::kSignIn));
        break;
      }
      case AccountsDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::CLOSE_BUTTON));
        break;
      case AccountsDialogAction::kNone:
        break;
    }
  }

  void ShowFailureDialog(content::WebContents* rp_web_contents,
                         const std::string& rp_url,
                         const std::string& idp_url,
                         IdentityRequestDialogController::DismissCallback
                             dismiss_callback) override {
    if (!state_) {
      return;
    }

    state_->did_show_idp_signin_status_mismatch_dialog = true;
    switch (idp_signin_status_mismatch_dialog_action_) {
      case IdpSigninStatusMismatchDialogAction::kClose:
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(dismiss_callback),
                                      DismissReason::CLOSE_BUTTON));
        break;
      case IdpSigninStatusMismatchDialogAction::kNone:
        break;
    }
  }

 private:
  AccountsDialogAction accounts_dialog_action_{AccountsDialogAction::kNone};
  IdpSigninStatusMismatchDialogAction idp_signin_status_mismatch_dialog_action_{
      IdpSigninStatusMismatchDialogAction::kNone};

  // Pointer so that the state can be queried after FederatedAuthRequestImpl
  // destroys TestDialogController.
  raw_ptr<State> state_;
};

class TestApiPermissionDelegate : public MockApiPermissionDelegate {
 public:
  std::pair<url::Origin, ApiPermissionStatus> permission_override_ =
      std::make_pair(url::Origin(), ApiPermissionStatus::GRANTED);
  std::set<url::Origin> embargoed_origins_;

  ApiPermissionStatus GetApiPermissionStatus(
      const url::Origin& origin) override {
    if (embargoed_origins_.count(origin))
      return ApiPermissionStatus::BLOCKED_EMBARGO;

    return (origin == permission_override_.first)
               ? permission_override_.second
               : ApiPermissionStatus::GRANTED;
  }

  void RecordDismissAndEmbargo(const url::Origin& origin) override {
    embargoed_origins_.insert(origin);
  }

  void RemoveEmbargoAndResetCounts(const url::Origin& origin) override {
    embargoed_origins_.erase(origin);
  }
};

}  // namespace

class FederatedAuthRequestImplTest : public RenderViewHostImplTestHarness {
 protected:
  FederatedAuthRequestImplTest() {
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }
  ~FederatedAuthRequestImplTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    test_api_permission_delegate_ =
        std::make_unique<TestApiPermissionDelegate>();
    mock_permission_delegate_ =
        std::make_unique<NiceMock<MockPermissionDelegate>>();

    static_cast<TestWebContents*>(web_contents())
        ->NavigateAndCommit(GURL(kRpUrl), ui::PAGE_TRANSITION_LINK);

    federated_auth_request_impl_ = &FederatedAuthRequestImpl::CreateForTesting(
        *main_test_rfh(), test_api_permission_delegate_.get(),
        mock_permission_delegate_.get(),
        request_remote_.BindNewPipeAndPassReceiver());

    std::unique_ptr<TestIdpNetworkRequestManager> network_request_manager =
        std::make_unique<TestIdpNetworkRequestManager>();
    SetNetworkRequestManager(std::move(network_request_manager));

    federated_auth_request_impl_->SetTokenRequestDelayForTests(
        base::TimeDelta());
  }

  void SetNetworkRequestManager(
      std::unique_ptr<TestIdpNetworkRequestManager> manager) {
    test_network_request_manager_ = std::move(manager);
    // DelegatedIdpNetworkRequestManager is owned by
    // |federated_auth_request_impl_|.
    federated_auth_request_impl_->SetNetworkManagerForTests(
        std::make_unique<DelegatedIdpNetworkRequestManager>(
            test_network_request_manager_.get()));
  }

  // Sets the TestDialogController to be used for the next call of
  // RunAuthTest().
  void SetDialogController(
      std::unique_ptr<TestDialogController> dialog_controller) {
    custom_dialog_controller_ = std::move(dialog_controller);
  }

  void RunAuthTest(const RequestParameters& request_parameters,
                   const RequestExpectations& expectation,
                   const MockConfiguration& configuration) {
    if (!custom_dialog_controller_) {
      custom_dialog_controller_ =
          std::make_unique<TestDialogController>(configuration);
    }

    dialog_controller_state_ = TestDialogController::State();
    custom_dialog_controller_->SetState(&dialog_controller_state_);
    federated_auth_request_impl_->SetDialogControllerForTests(
        std::move(custom_dialog_controller_));

    test_network_request_manager_->SetTestConfig(configuration);

    std::vector<blink::mojom::IdentityProviderGetParametersPtr> idp_get_params;
    for (const auto& identity_provider :
         request_parameters.identity_providers) {
      std::vector<blink::mojom::IdentityProviderConfigPtr> idp_ptrs;
      blink::mojom::IdentityProviderLoginHintPtr login_hint_ptr =
          blink::mojom::IdentityProviderLoginHint::New(
              identity_provider.login_hint.email,
              identity_provider.login_hint.id,
              identity_provider.login_hint.is_required);
      blink::mojom::IdentityProviderConfigPtr idp_ptr =
          blink::mojom::IdentityProviderConfig::New(
              GURL(identity_provider.provider), identity_provider.client_id,
              identity_provider.nonce, std::move(login_hint_ptr));
      idp_ptrs.push_back(std::move(idp_ptr));
      blink::mojom::IdentityProviderGetParametersPtr get_params =
          blink::mojom::IdentityProviderGetParameters::New(
              std::move(idp_ptrs), request_parameters.prefer_auto_sign_in,
              request_parameters.rp_context);
      idp_get_params.push_back(std::move(get_params));
    }

    auto auth_response = PerformAuthRequest(std::move(idp_get_params),
                                            configuration.wait_for_callback);
    ASSERT_EQ(std::get<0>(auth_response), expectation.return_status);
    if (expectation.return_status == RequestTokenStatus::kSuccess) {
      EXPECT_EQ(configuration.token, std::get<2>(auth_response));
    } else {
      EXPECT_TRUE(std::get<2>(auth_response) == absl::nullopt ||
                  std::get<2>(auth_response) == kEmptyToken);
    }

    if (expectation.return_status == RequestTokenStatus::kSuccess) {
      EXPECT_TRUE(DidFetchWellKnownAndConfig());
      EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
      EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
      // FetchedEndpoint::CLIENT_METADATA is optional.

      EXPECT_TRUE(did_show_accounts_dialog());
    }

    if (expectation.selected_idp_config_url) {
      EXPECT_EQ(std::get<1>(auth_response),
                GURL(*expectation.selected_idp_config_url));
    } else {
      EXPECT_FALSE(std::get<1>(auth_response).has_value());
    }

    if (!expectation.devtools_issue_statuses.empty()) {
      std::map<FederatedAuthRequestResult, int> devtools_issue_counts;
      for (FederatedAuthRequestResult devtools_issue_status :
           expectation.devtools_issue_statuses) {
        if (devtools_issue_status == FederatedAuthRequestResult::kSuccess)
          continue;

        ++devtools_issue_counts[devtools_issue_status];
      }

      for (auto& [devtools_issue_status, expected_count] :
           devtools_issue_counts) {
        int issue_count = main_test_rfh()->GetFederatedAuthRequestIssueCount(
            devtools_issue_status);
        EXPECT_LE(expected_count, issue_count);
      }
      if (devtools_issue_counts.empty()) {
        int issue_count =
            main_test_rfh()->GetFederatedAuthRequestIssueCount(absl::nullopt);
        EXPECT_EQ(0, issue_count);
      }
      CheckConsoleMessages(expectation.devtools_issue_statuses);
    }
  }

  void CheckConsoleMessages(
      const std::vector<FederatedAuthRequestResult>& devtools_issue_statuses) {
    std::vector<std::string> messages =
        RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();

    bool did_expect_any_messages = false;
    size_t expected_message_index = messages.size() - 1;
    for (const auto& expected_status :
         base::Reversed(devtools_issue_statuses)) {
      if (expected_status == FederatedAuthRequestResult::kSuccess) {
        continue;
      }

      std::string expected_message =
          webid::GetConsoleErrorMessageFromResult(expected_status);
      did_expect_any_messages = true;
      ASSERT_GE(expected_message_index, 0u);
      EXPECT_EQ(expected_message, messages[expected_message_index]);
      --expected_message_index;
    }

    if (!did_expect_any_messages)
      EXPECT_EQ(0u, messages.size());
  }

  std::tuple<absl::optional<RequestTokenStatus>,
             absl::optional<GURL>,
             absl::optional<std::string>>
  PerformAuthRequest(std::vector<blink::mojom::IdentityProviderGetParametersPtr>
                         idp_get_params,
                     bool wait_for_callback) {
    request_remote_->RequestToken(std::move(idp_get_params),
                                  auth_helper_.callback());

    if (wait_for_callback)
      request_remote_.set_disconnect_handler(auth_helper_.quit_closure());

    // Ensure that the request makes its way to FederatedAuthRequestImpl.
    request_remote_.FlushForTesting();
    base::RunLoop().RunUntilIdle();
    if (wait_for_callback) {
      // Fast forward clock so that the pending
      // FederatedAuthRequestImpl::OnRejectRequest() task, if any, gets a
      // chance to run.
      task_environment()->FastForwardBy(base::Minutes(10));
      auth_helper_.WaitForCallback();

      request_remote_.set_disconnect_handler(base::OnceClosure());
    }
    return std::make_tuple(auth_helper_.status(),
                           auth_helper_.selected_idp_config_url(),
                           auth_helper_.token());
  }

  base::span<const content::IdentityRequestAccount> displayed_accounts() const {
    return dialog_controller_state_.displayed_accounts;
  }

  bool did_show_accounts_dialog() const {
    return !displayed_accounts().empty();
  }
  bool did_show_idp_signin_status_mismatch_dialog() const {
    return dialog_controller_state_.did_show_idp_signin_status_mismatch_dialog;
  }

  int CountNumLoginStateIsSignin() {
    int num_sign_in_login_state = 0;
    for (const auto& account : displayed_accounts()) {
      if (account.login_state == LoginState::kSignIn) {
        ++num_sign_in_login_state;
      }
    }
    return num_sign_in_login_state;
  }

  bool DidFetchAnyEndpoint() {
    for (auto& [endpoint, num] : test_network_request_manager_->num_fetched_) {
      if (num > 0) {
        return true;
      }
    }
    return false;
  }

  // Convenience method as WELL_KNOWN and CONFIG endpoints are fetched in
  // parallel.
  bool DidFetchWellKnownAndConfig() {
    return DidFetch(FetchedEndpoint::WELL_KNOWN) &&
           DidFetch(FetchedEndpoint::CONFIG);
  }

  bool DidFetch(FetchedEndpoint endpoint) { return NumFetched(endpoint) > 0u; }

  size_t NumFetched(FetchedEndpoint endpoint) {
    return test_network_request_manager_->num_fetched_[endpoint];
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  void ExpectRequestTokenStatusUKM(TokenStatus status) {
    ExpectRequestTokenStatusUKMInternal(status, FedCmEntry::kEntryName);
    ExpectRequestTokenStatusUKMInternal(status, FedCmIdpEntry::kEntryName);
  }

  void ExpectRequestTokenStatusUKMInternal(TokenStatus status,
                                           const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    if (entries.empty())
      FAIL() << "No RequestTokenStatus was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status_RequestToken");
      if (metric && *metric != static_cast<int>(status))
        FAIL() << "Unexpected status was recorded";
    }

    SUCCEED();
  }

  void ExpectTimingUKM(const std::string& metric_name) {
    ExpectTimingUKMInternal(metric_name, FedCmEntry::kEntryName);
    ExpectTimingUKMInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectTimingUKMInternal(const std::string& metric_name,
                               const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected UKM was not recorded";
  }

  void ExpectNoTimingUKM(const std::string& metric_name) {
    ExpectNoTimingUKMInternal(metric_name, FedCmEntry::kEntryName);
    ExpectNoTimingUKMInternal(metric_name, FedCmIdpEntry::kEntryName);
  }

  void ExpectNoTimingUKMInternal(const std::string& metric_name,
                                 const char* entry_name) {
    auto entries = ukm_recorder()->GetEntriesByName(entry_name);

    ASSERT_FALSE(entries.empty());

    for (const auto* const entry : entries) {
      if (ukm_recorder()->GetEntryMetric(entry, metric_name))
        FAIL() << "Unexpected UKM was recorded";
    }
    SUCCEED();
  }

  void ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus status) {
    auto entries = ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName);

    if (entries.empty())
      FAIL() << "No SignInStateMatchStatus was recorded";

    // There are multiple types of metrics under the same FedCM UKM. We need to
    // make sure that the metric only includes the expected one.
    for (const auto* const entry : entries) {
      const int64_t* metric =
          ukm_recorder()->GetEntryMetric(entry, "Status_SignInStateMatch");
      if (metric && *metric != static_cast<int>(status))
        FAIL() << "Unexpected status was recorded";
    }

    SUCCEED();
  }

  void CheckAllFedCmSessionIDs() {
    absl::optional<int> session_id;
    auto CheckUKMSessionID = [&](const auto& ukm_entries) {
      ASSERT_FALSE(ukm_entries.empty());
      for (const auto* const entry : ukm_entries) {
        const auto* const metric =
            ukm_recorder()->GetEntryMetric(entry, "FedCmSessionID");
        EXPECT_TRUE(metric)
            << "All UKM events should have the SessionID metric";
        if (!session_id.has_value()) {
          session_id = *metric;
        } else {
          ASSERT_EQ(*metric, *session_id)
              << "All UKM events should have the same SessionID";
        }
      }
    };
    CheckUKMSessionID(ukm_recorder()->GetEntriesByName(FedCmEntry::kEntryName));
    CheckUKMSessionID(
        ukm_recorder()->GetEntriesByName(FedCmIdpEntry::kEntryName));
  }

  void ComputeLoginStateAndReorderAccounts(
      const blink::mojom::IdentityProviderConfigPtr& identity_provider,
      IdpNetworkRequestManager::AccountList& accounts) {
    federated_auth_request_impl_->ComputeLoginStateAndReorderAccounts(
        identity_provider, accounts);
  }

 protected:
  mojo::Remote<blink::mojom::FederatedAuthRequest> request_remote_;
  raw_ptr<FederatedAuthRequestImpl> federated_auth_request_impl_;

  std::unique_ptr<TestIdpNetworkRequestManager> test_network_request_manager_;

  std::unique_ptr<TestApiPermissionDelegate> test_api_permission_delegate_;
  std::unique_ptr<NiceMock<MockPermissionDelegate>> mock_permission_delegate_;

  AuthRequestCallbackHelper auth_helper_;

  // Enables test to inspect TestDialogController state after
  // FederatedAuthRequestImpl destroys TestDialogController. Recreated during
  // each run of RunAuthTest().
  TestDialogController::State dialog_controller_state_;

  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<TestDialogController> custom_dialog_controller_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

// Test successful FedCM request.
TEST_F(FederatedAuthRequestImplTest, SuccessfulRequest) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  // Check that client metadata is fetched. Using `kExpectationSuccess`
  // expectation does not check that the client metadata was fetched because
  // client metadata is optional.
  EXPECT_TRUE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test successful well-known fetching.
TEST_F(FederatedAuthRequestImplTest, WellKnownSuccess) {
  // Use IdpNetworkRequestManagerParamChecker to validate passed-in parameters
  // to IdpNetworkRequestManager methods.
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the provider url is not in the well-known.
TEST_F(FederatedAuthRequestImplTest, WellKnownNotInList) {
  RequestExpectations request_not_in_list = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt};

  const char* idp_config_url =
      kDefaultRequestParameters.identity_providers[0].provider;
  const char* kWellKnownMismatchConfigUrl = "https://mismatch.example";
  EXPECT_NE(std::string(idp_config_url), kWellKnownMismatchConfigUrl);

  MockConfiguration config = kConfigurationValid;
  config.idp_info[idp_config_url].well_known = {{kWellKnownMismatchConfigUrl}};
  RunAuthTest(kDefaultRequestParameters, request_not_in_list, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that not having the filename in the well-known fails.
TEST_F(FederatedAuthRequestImplTest, WellKnownHasNoFilename) {
  MockConfiguration config{kConfigurationValid};
  config.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{GURL(kProviderUrlFull).GetWithoutFilename().spec()};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, config);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that request fails if config is missing token endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingTokenEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_assertion_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that request fails if config is missing accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, MissingAccountsEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));

  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Test that client metadata endpoint is not required in config.
TEST_F(FederatedAuthRequestImplTest, MissingClientMetadataEndpoint) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.client_metadata_endpoint = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

// Test that request fails if the accounts endpoint is in a different origin
// than identity provider.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointDifferentOriginIdp) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      kCrossOriginAccountsEndpoint;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test that request fails if the idp is not https.
TEST_F(FederatedAuthRequestImplTest, ProviderNotTrustworthy) {
  IdentityProviderParameters identity_provider{"http://idp.example/fedcm.json",
                                               kClientId, kNonce};
  RequestParameters request{
      std::vector<IdentityProviderParameters>{identity_provider},
      /*prefer_auto_sign_in=*/false,
      /*rp_context=*/blink::mojom::RpContext::kSignIn};
  MockConfiguration configuration = kConfigurationValid;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(request, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.RequestIdToken",
      TokenStatus::kIdpNotPotentiallyTrustworthy, 1);
}

// Test that request fails if accounts endpoint cannot be reached.
TEST_F(FederatedAuthRequestImplTest, AccountEndpointCannotBeReached) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kNoResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that request fails if account endpoint response cannot be parsed.
TEST_F(FederatedAuthRequestImplTest, AccountsCannotBeParsed) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that privacy policy URL or terms of service is not required in client
// metadata.
TEST_F(FederatedAuthRequestImplTest,
       ClientMetadataNoPrivacyPolicyOrTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that privacy policy URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoPrivacyPolicyUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull].client_metadata.privacy_policy_url =
      "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that terms of service URL is not required in client metadata.
TEST_F(FederatedAuthRequestImplTest, ClientMetadataNoTermsOfServiceUrl) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].client_metadata =
      kDefaultClientMetadata;
  configuration.idp_info[kProviderUrlFull]
      .client_metadata.terms_of_service_url = "";
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

// Test that request fails if all of the endpoints in the config are invalid.
TEST_F(FederatedAuthRequestImplTest, AllInvalidEndpoints) {
  // Both an empty url and cross origin urls are invalid endpoints.
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].config.accounts_endpoint =
      "https://cross-origin-1.com";
  configuration.idp_info[kProviderUrlFull].config.token_endpoint = "";
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetchWellKnownAndConfig());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
  std::vector<std::string> messages =
      RenderFrameHostTester::For(main_rfh())->GetConsoleMessages();
  ASSERT_EQ(2U, messages.size());
  EXPECT_EQ(
      "Config file is missing or has an invalid URL for the following "
      "endpoints:\n"
      "\"id_assertion_endpoint\"\n"
      "\"accounts_endpoint\"\n",
      messages[0]);
  EXPECT_EQ("Provider's FedCM config file is invalid.", messages[1]);
}

// Tests for Login State
TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignUpForFirstTimeUser) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignUp, displayed_accounts()[0].login_state);
}

TEST_F(FederatedAuthRequestImplTest, LoginStateShouldBeSignInForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  // CLIENT_METADATA only needs to be fetched for obtaining links to display in
  // the disclosure text. The disclosure text is not displayed for returning
  // users, thus fetching the client metadata endpoint should be skipped.
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateSuccessfulSignUpGrantsSharingPermission) {
  EXPECT_CALL(*mock_permission_delegate_, HasSharingPermission(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(
      *mock_permission_delegate_,
      GrantSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                             OriginFromString(kProviderUrlFull), kAccountId))
      .Times(1);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginStateFailedSignUpNotGrantSharingPermission) {
  EXPECT_CALL(*mock_permission_delegate_, HasSharingPermission(_, _, _, _))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_permission_delegate_, GrantSharingPermission(_, _, _, _))
      .Times(0);

  MockConfiguration configuration = kConfigurationValid;
  configuration.token_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::TOKEN));
}

// Test that auto sign-in with a single account where the account is a returning
// user sets the sign-in mode to auto.
TEST_F(FederatedAuthRequestImplTest,
       AutoSigninForSingleReturningUserSingleAccount) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoSignin);

  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignIn);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);
}

// Test that auto sign-in with multiple accounts and a single returning user
// sets the sign-in mode to auto.
TEST_F(FederatedAuthRequestImplTest,
       AutoSigninForSingleReturningUserMultipleAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoSignin);

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(displayed_accounts().size(), 3u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 1);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kAuto);
}

// Test that auto sign-in with multiple accounts and multiple returning users
// sets the sign-in mode to explicit.
TEST_F(FederatedAuthRequestImplTest,
       AutoSigninForMultipleReturningUsersMultipleAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoSignin);

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;

  AccountList multiple_accounts = kMultipleAccounts;
  multiple_accounts[0].login_state = LoginState::kSignIn;
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = multiple_accounts;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  ASSERT_EQ(displayed_accounts().size(), 3u);
  EXPECT_EQ(CountNumLoginStateIsSignin(), 2);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

// Test that auto sign-in for a first time user sets the sign-in mode to
// explicit.
TEST_F(FederatedAuthRequestImplTest, AutoSigninForFirstTimeUser) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmAutoSignin);

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.prefer_auto_sign_in = true;
  RunAuthTest(request_parameters, kExpectationSuccess, kConfigurationValid);

  ASSERT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignUp);
  EXPECT_EQ(dialog_controller_state_.sign_in_mode, SignInMode::kExplicit);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForSuccessfulSignInCase) {
  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kSuccess, 1);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.IsSignInUser", 1, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.ContinueOnDialog");
  ExpectTimingUKM("Timing.IdTokenResponse");
  ExpectTimingUKM("Timing.TurnaroundTime");
  ExpectNoTimingUKM("Timing.CancelOnDialog");

  ExpectRequestTokenStatusUKM(TokenStatus::kSuccess);
  CheckAllFedCmSessionIDs();
}

// Test that request fails if account picker is explicitly dismissed.
TEST_F(FederatedAuthRequestImplTest, MetricsForUIExplicitlyDismissed) {
  base::HistogramTester histogram_tester_;
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  for (const auto& idp_info : kConfigurationValid.idp_info) {
    ASSERT_EQ(idp_info.second.accounts.size(), 1u);
  }
  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));

  ukm_loop.Run();

  ASSERT_TRUE(did_show_accounts_dialog());
  EXPECT_EQ(displayed_accounts()[0].login_state, LoginState::kSignUp);

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kShouldEmbargo, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectTimingUKM("Timing.CancelOnDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kShouldEmbargo);
  CheckAllFedCmSessionIDs();
}

namespace {

// TestDialogController subclass which supports WeakPtr.
class WeakTestDialogController
    : public TestDialogController,
      public base::SupportsWeakPtr<WeakTestDialogController> {
 public:
  explicit WeakTestDialogController(MockConfiguration configuration)
      : TestDialogController(configuration) {}
  ~WeakTestDialogController() override = default;
  WeakTestDialogController(WeakTestDialogController&) = delete;
  WeakTestDialogController& operator=(WeakTestDialogController&) = delete;
};

}  // namespace

// Test that request is not completed if user ignores the UI.
TEST_F(FederatedAuthRequestImplTest, UIIsIgnored) {
  base::HistogramTester histogram_tester_;

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;

  auto dialog_controller =
      std::make_unique<WeakTestDialogController>(configuration);
  base::WeakPtr<WeakTestDialogController> weak_dialog_controller =
      dialog_controller->AsWeakPtr();
  SetDialogController(std::move(dialog_controller));

  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  task_environment()->FastForwardBy(base::Minutes(10));

  EXPECT_FALSE(auth_helper_.was_callback_called());

  // The dialog should have been shown. The dialog controller should not be
  // destroyed.
  ASSERT_TRUE(did_show_accounts_dialog());
  EXPECT_TRUE(weak_dialog_controller);

  // Only the time to show the account dialog gets recorded.
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.CancelOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Status.RequestIdToken", 0);
}

TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsVisible) {
  base::HistogramTester histogram_tester;
  // Sets RenderFrameHost to visible
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Pretends that the sharing permission has been granted for this account.
  EXPECT_CALL(*mock_permission_delegate_,
              HasSharingPermission(_, _, OriginFromString(kProviderUrlFull),
                                   kAccountId))
      .WillOnce(Return(true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_EQ(LoginState::kSignIn, displayed_accounts()[0].login_state);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 1, 1);
}

// Test that request fails if the web contents are hidden.
TEST_F(FederatedAuthRequestImplTest, MetricsForWebContentsInvisible) {
  base::HistogramTester histogram_tester;
  test_rvh()->SimulateWasShown();
  ASSERT_EQ(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  // Sets the RenderFrameHost to invisible
  test_rvh()->SimulateWasHidden();
  ASSERT_NE(test_rvh()->GetMainRenderFrameHost()->GetVisibilityState(),
            content::PageVisibilityState::kVisible);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorRpPageNotVisible},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.WebContentsVisible", 0, 1);
}

TEST_F(FederatedAuthRequestImplTest, DisabledWhenThirdPartyCookiesBlocked) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_THIRD_PARTY_COOKIES_BLOCKED);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kThirdPartyCookiesBlocked,
                                       1);
  ExpectRequestTokenStatusUKM(TokenStatus::kThirdPartyCookiesBlocked);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest, MetricsForFeatureIsDisabled) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithUnhandledRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Delete the request before DelayTimer kicks in.
  federated_auth_request_impl_->ResetAndDeleteThis();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

TEST_F(FederatedAuthRequestImplTest,
       MetricsForFeatureIsDisabledNotDoubleCountedWithAbortedRequest) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_VARIATIONS);

  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());

  // Abort the request before DelayTimer kicks in.
  federated_auth_request_impl_->CancelTokenRequest();

  // If double counted, these samples would not be unique so the following
  // checks will fail.
  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInFlags, 1);
  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInFlags);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is signed in and
// browser also observes that user is signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kAccounts.begin(), kAccounts.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = displayed_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states match if IDP claims that user is not signed in and
// browser also observes that user is not signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForNotSignedInOnBothIdpAndBrowser) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // By default, IDP claims user is not signed in.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.SignInStateMatch",
                                       SignInStateMatchStatus::kMatch, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kMatch);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is signed in but
// browser observes that user is not signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForOnlyIdpClaimedSignIn) {
  // Set browser observes user is not signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(false));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Set IDP claims user is signed in.
  MockConfiguration configuration = kConfigurationValid;
  AccountList displayed_accounts =
      AccountList(kAccounts.begin(), kAccounts.end());
  displayed_accounts[0].login_state = LoginState::kSignIn;
  configuration.idp_info[kProviderUrlFull].accounts = displayed_accounts;
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kIdpClaimedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(SignInStateMatchStatus::kIdpClaimedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that sign-in states mismatch if IDP claims that user is not signed in
// but browser observes that user is signed in.
TEST_F(FederatedAuthRequestImplTest, MetricsForOnlyBrowserObservedSignIn) {
  // Set browser observes user is signed in.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::CLIENT_METADATA));

  ukm_loop.Run();

  histogram_tester_.ExpectUniqueSample(
      "Blink.FedCm.Status.SignInStateMatch",
      SignInStateMatchStatus::kBrowserObservedSignIn, 1);
  ExpectSignInStateMatchStatusUKM(
      SignInStateMatchStatus::kBrowserObservedSignIn);
  CheckAllFedCmSessionIDs();
}

// Test that embargo is requested if the
// IdentityRequestDialogController::ShowAccountsDialog() callback requests it.
TEST_F(FederatedAuthRequestImplTest, RequestEmbargo) {
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /*selected_idp_config_url=*/absl::nullopt};

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.count(
      main_test_rfh()->GetLastCommittedOrigin()));
}

// Test that the embargo dismiss count is reset when the user grants consent via
// the FedCM dialog.
TEST_F(FederatedAuthRequestImplTest, RemoveEmbargoOnUserConsent) {
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
  EXPECT_TRUE(test_api_permission_delegate_->embargoed_origins_.empty());
}

// Test that token request fails if FEDERATED_IDENTITY_API content setting is
// disabled for the RP origin.
TEST_F(FederatedAuthRequestImplTest, ApiBlockedForOrigin) {
  test_api_permission_delegate_->permission_override_ =
      std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                     ApiPermissionStatus::BLOCKED_SETTINGS);
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// Test that token request succeeds if FEDERATED_IDENTITY_API content setting is
// enabled for RP origin but disabled for an unrelated origin.
TEST_F(FederatedAuthRequestImplTest, ApiBlockedForUnrelatedOrigin) {
  const url::Origin kUnrelatedOrigin = OriginFromString("https://rp2.example/");

  test_api_permission_delegate_->permission_override_ =
      std::make_pair(kUnrelatedOrigin, ApiPermissionStatus::BLOCKED_SETTINGS);
  ASSERT_NE(main_test_rfh()->GetLastCommittedOrigin(), kUnrelatedOrigin);
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

class FederatedAuthRequestImplTestCancelConsistency
    : public FederatedAuthRequestImplTest,
      public ::testing::WithParamInterface<int> {};
INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         FederatedAuthRequestImplTestCancelConsistency,
                         ::testing::Values(false, true),
                         ::testing::PrintToStringParamName());

// Test that the RP cannot use CancelTokenRequest() to determine whether
// Option 1: FedCM dialog is shown but user has not interacted with it
// Option 2: FedCM API is disabled via variations
TEST_P(FederatedAuthRequestImplTestCancelConsistency, AccountNotSelected) {
  const bool fedcm_disabled = GetParam();

  if (fedcm_disabled) {
    test_api_permission_delegate_->permission_override_ =
        std::make_pair(main_test_rfh()->GetLastCommittedOrigin(),
                       ApiPermissionStatus::BLOCKED_VARIATIONS);
  }

  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  configuration.wait_for_callback = false;
  RequestExpectations expectation = {/*return_status=*/absl::nullopt,
                                     /*devtools_issue_statuses=*/{},
                                     /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectation, configuration);
  EXPECT_FALSE(auth_helper_.was_callback_called());

  request_remote_->CancelTokenRequest();
  request_remote_.FlushForTesting();
  EXPECT_TRUE(auth_helper_.was_callback_called());
  EXPECT_EQ(RequestTokenStatus::kErrorCanceled, auth_helper_.status());
}

namespace {

// TestDialogController which disables FedCM API when FedCM account selection
// dialog is shown.
class DisableApiWhenDialogShownDialogController : public TestDialogController {
 public:
  DisableApiWhenDialogShownDialogController(
      MockConfiguration configuration,
      TestApiPermissionDelegate* api_permission_delegate,
      const url::Origin rp_origin_to_disable)
      : TestDialogController(configuration),
        api_permission_delegate_(api_permission_delegate),
        rp_origin_to_disable_(rp_origin_to_disable) {}
  ~DisableApiWhenDialogShownDialogController() override = default;

  DisableApiWhenDialogShownDialogController(
      const DisableApiWhenDialogShownDialogController&) = delete;
  DisableApiWhenDialogShownDialogController& operator=(
      DisableApiWhenDialogShownDialogController&) = delete;

  void ShowAccountsDialog(
      content::WebContents* rp_web_contents,
      const std::string& rp_for_display,
      const std::vector<IdentityProviderData>& identity_provider_data,
      SignInMode sign_in_mode,
      IdentityRequestDialogController::AccountSelectionCallback on_selected,
      IdentityRequestDialogController::DismissCallback dismiss_callback)
      override {
    // Disable FedCM API
    api_permission_delegate_->permission_override_ = std::make_pair(
        rp_origin_to_disable_, ApiPermissionStatus::BLOCKED_SETTINGS);

    // Call parent class method in order to store callback parameters.
    TestDialogController::ShowAccountsDialog(
        rp_web_contents, rp_for_display, std::move(identity_provider_data),
        sign_in_mode, std::move(on_selected), std::move(dismiss_callback));
  }

 private:
  raw_ptr<TestApiPermissionDelegate> api_permission_delegate_;
  url::Origin rp_origin_to_disable_;
};

}  // namespace

// Test that the request fails if user proceeds with the sign in workflow after
// disabling the API while an existing accounts dialog is shown.
TEST_F(FederatedAuthRequestImplTest, ApiDisabledAfterAccountsDialogShown) {
  base::HistogramTester histogram_tester_;

  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(FedCmEntry::kEntryName,
                                        ukm_loop.QuitClosure());

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorDisabledInSettings},
      /*selected_idp_config_url=*/absl::nullopt};

  url::Origin rp_origin_to_disable = main_test_rfh()->GetLastCommittedOrigin();
  SetDialogController(
      std::make_unique<DisableApiWhenDialogShownDialogController>(
          kConfigurationValid, test_api_permission_delegate_.get(),
          rp_origin_to_disable));

  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_FALSE(DidFetch(FetchedEndpoint::TOKEN));

  ukm_loop.Run();

  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ShowAccountsDialog",
                                     1);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.ContinueOnDialog", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.IdTokenResponse", 0);
  histogram_tester_.ExpectTotalCount("Blink.FedCm.Timing.TurnaroundTime", 0);

  histogram_tester_.ExpectUniqueSample("Blink.FedCm.Status.RequestIdToken",
                                       TokenStatus::kDisabledInSettings, 1);

  ExpectTimingUKM("Timing.ShowAccountsDialog");
  ExpectNoTimingUKM("Timing.ContinueOnDialog");
  ExpectNoTimingUKM("Timing.IdTokenResponse");
  ExpectNoTimingUKM("Timing.TurnaroundTime");

  ExpectRequestTokenStatusUKM(TokenStatus::kDisabledInSettings);
  CheckAllFedCmSessionIDs();
}

// Test the disclosure_text_shown value in the token post data for sign-up case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextShownForFirstTimeUser) {
  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=" + std::string(kAccountId) + "&disclosure_text_shown=true");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test the disclosure_text_shown value in the token post data for returning
// user case.
TEST_F(FederatedAuthRequestImplTest, DisclosureTextNotShownForReturningUser) {
  // Pretend the sharing permission has been granted for this account.
  EXPECT_CALL(
      *mock_permission_delegate_,
      HasSharingPermission(OriginFromString(kRpUrl), OriginFromString(kRpUrl),
                           OriginFromString(kProviderUrlFull), kAccountId))
      .WillOnce(Return(true));

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData("client_id=" + std::string(kClientId) +
                                    "&nonce=" + std::string(kNonce) +
                                    "&account_id=" + std::string(kAccountId) +
                                    "&disclosure_text_shown=false");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that the values in the token post data are escaped according to the
// application/x-www-form-urlencoded spec.
TEST_F(FederatedAuthRequestImplTest, TokenEndpointPostDataEscaping) {
  const std::string kAccountIdWithSpace("account id");
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts[0].id = kAccountIdWithSpace;

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectedTokenPostData(
      "client_id=" + std::string(kClientId) + "&nonce=" + std::string(kNonce) +
      "&account_id=account+id&disclosure_text_shown=true");
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess, configuration);
}

namespace {

// TestIdpNetworkRequestManager subclass which runs the `account_list_task`
// passed-in to the constructor prior to the accounts endpoint returning.
class IdpNetworkRequestManagerClientMetadataTaskRunner
    : public TestIdpNetworkRequestManager {
 public:
  explicit IdpNetworkRequestManagerClientMetadataTaskRunner(
      base::OnceClosure client_metadata_task)
      : client_metadata_task_(std::move(client_metadata_task)) {}

  IdpNetworkRequestManagerClientMetadataTaskRunner(
      const IdpNetworkRequestManagerClientMetadataTaskRunner&) = delete;
  IdpNetworkRequestManagerClientMetadataTaskRunner& operator=(
      const IdpNetworkRequestManagerClientMetadataTaskRunner&) = delete;

  void FetchClientMetadata(const GURL& client_metadata_endpoint_url,
                           const std::string& client_id,
                           FetchClientMetadataCallback callback) override {
    // Make copies because running the task might destroy
    // FederatedAuthRequestImpl and invalidate the references.
    GURL client_metadata_endpoint_url_copy = client_metadata_endpoint_url;
    std::string client_id_copy = client_id;

    if (client_metadata_task_)
      std::move(client_metadata_task_).Run();
    TestIdpNetworkRequestManager::FetchClientMetadata(
        client_metadata_endpoint_url_copy, client_id_copy, std::move(callback));
  }

 private:
  base::OnceClosure client_metadata_task_;
};

void NavigateToUrl(content::WebContents* web_contents, const GURL& url) {
  static_cast<TestWebContents*>(web_contents)
      ->NavigateAndCommit(url, ui::PAGE_TRANSITION_LINK);
}

}  // namespace

// Test that the account chooser is not shown if the page navigates prior to the
// client metadata endpoint request completing and BFCache is enabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheEnabled) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kBackForwardCache},
      /*disabled_features=*/{features::kBackForwardCacheMemoryControls});
  ASSERT_TRUE(content::IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that the account chooser is not shown if the page navigates prior to the
// accounts endpoint request completing and BFCache is disabled.
TEST_F(FederatedAuthRequestImplTest,
       NavigateDuringClientMetadataFetchBFCacheDisabled) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kBackForwardCache);
  ASSERT_FALSE(content::IsBackForwardCacheEnabled());

  SetNetworkRequestManager(
      std::make_unique<IdpNetworkRequestManagerClientMetadataTaskRunner>(
          base::BindOnce(&NavigateToUrl, web_contents(), GURL(kRpOtherUrl))));

  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that the accounts are reordered so that accounts with a LoginState equal
// to kSignIn are listed before accounts with a LoginState equal to kSignUp.
TEST_F(FederatedAuthRequestImplTest, ReorderMultipleAccounts) {
  // Run an auth test to initialize variables.
  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);

  AccountList multiple_accounts = kMultipleAccounts;
  blink::mojom::IdentityProviderLoginHintPtr login_hint_ptr =
      blink::mojom::IdentityProviderLoginHint::New(/*email=*/"", /*id=*/"",
                                                   /*login_hint=*/false);
  blink::mojom::IdentityProviderConfigPtr identity_provider =
      blink::mojom::IdentityProviderConfig::New(
          GURL(kProviderUrlFull), kClientId, kNonce, std::move(login_hint_ptr));
  ComputeLoginStateAndReorderAccounts(identity_provider, multiple_accounts);

  // Check the account order using the account ids.
  ASSERT_EQ(multiple_accounts.size(), 3u);
  EXPECT_EQ(multiple_accounts[0].id, kAccountIdPeter);
  EXPECT_EQ(multiple_accounts[1].id, kAccountIdNicolas);
  EXPECT_EQ(multiple_accounts[2].id, kAccountIdZach);
}

// Test that first API call with a given IDP is not affected by the
// IdpSigninStatus bit.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestFirstTimeFetchSuccess) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), true))
      .Times(1);

  std::unique_ptr<IdpNetworkRequestManagerParamChecker> checker =
      std::make_unique<IdpNetworkRequestManagerParamChecker>();
  checker->SetExpectations(kClientId, kAccountId);
  SetNetworkRequestManager(std::move(checker));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that first API call with a given IDP will not show a UI in case of
// failure during fetching accounts.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestFirstTimeFetchNoFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false))
      .Times(1);
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that a failure UI will be displayed if the accounts fetch is failed but
// the IdpSigninStatus claims that the user is signed in.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusTestShowFailureUi) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              GetIdpSigninStatus(OriginFromString(kProviderUrlFull)))
      .WillRepeatedly(Return(true));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  configuration.idp_signin_status_mismatch_dialog_action =
      IdpSigninStatusMismatchDialogAction::kClose;
  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that API calls will fail before sending any network request if
// IdpSigninStatus shows that the user is not signed in with the IDP. No failure
// UI is displayed.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusTestApiFailedIfUserNotSignedInWithIdp) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusFieldTrialParamName, "true"}});

  EXPECT_CALL(*mock_permission_delegate_,
              GetIdpSigninStatus(OriginFromString(kProviderUrlFull)))
      .WillOnce(Return(false));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kError},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, kConfigurationValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
  EXPECT_FALSE(did_show_idp_signin_status_mismatch_dialog());
}

// Test that when IdpSigninStatus API is in the metrics-only mode, that an IDP
// signed-out status stays signed-out regardless of what is returned by the
// accounts endpoint.
TEST_F(FederatedAuthRequestImplTest, IdpSigninStatusMetricsModeStaysSignedout) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_permission_delegate_, SetIdpSigninStatus(_, _)).Times(0);

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API does not have any state for an IDP, that
// the state transitions to sign-in if the accounts endpoint returns a
// non-empty list of accounts.
TEST_F(
    FederatedAuthRequestImplTest,
    IdpSigninStatusMetricsModeUndefinedTransitionsToSignedinWhenHaveAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillRepeatedly(Return(absl::nullopt));
  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), true));

  RunAuthTest(kDefaultRequestParameters, kExpectationSuccess,
              kConfigurationValid);
}

// Test that when IdpSigninStatus API is in metrics-only mode, that IDP sign-in
// status transitions to signed-out if the accounts endpoint returns no
// information.
TEST_F(FederatedAuthRequestImplTest,
       IdpSigninStatusMetricsModeTransitionsToSignedoutWhenNoAccounts) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeatureWithParameters(
      features::kFedCm,
      {{features::kFedCmIdpSigninStatusMetricsOnlyFieldTrialParamName,
        "true"}});

  EXPECT_CALL(*mock_permission_delegate_, GetIdpSigninStatus(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_permission_delegate_,
              SetIdpSigninStatus(OriginFromString(kProviderUrlFull), false));

  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts_response.parse_status =
      ParseStatus::kInvalidResponseError;
  RequestExpectations expectations = {
      RequestTokenStatus::kError, {}, absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Tests that multiple IDPs provided results in an error if the
// `kFedCmMultipleIdentityProviders` flag is disabled.
TEST_F(FederatedAuthRequestImplTest, MultiIdpError) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(features::kFedCmMultipleIdentityProviders);

  RequestExpectations expectations = {
      RequestTokenStatus::kError, {}, absl::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations,
              kConfigurationMultiIdpValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// Test successful multi IDP FedCM request.
TEST_F(FederatedAuthRequestImplTest, AllSuccessfulMultiIdpRequest) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccess,
              kConfigurationMultiIdpValid);
  EXPECT_EQ(2u, NumFetched(FetchedEndpoint::ACCOUNTS));
}

// Test fetching information for the 1st IdP failing, and succeeding for the
// second.
TEST_F(FederatedAuthRequestImplTest, FirstIdpWellKnownInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the 1st provider's request by having an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/kProviderTwoUrlFull};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::TOKEN), 1u);
}

// Test fetching information for the 1st IdP succeeding, and failing for the
// second.
TEST_F(FederatedAuthRequestImplTest, SecondIdpWellKnownInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the 2nd provider's request by having an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderTwoUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kSuccess,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/kProviderUrlFull};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::ACCOUNTS), 1u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::TOKEN), 1u);
}

// Test fetching information for all of the IdPs failing.
TEST_F(FederatedAuthRequestImplTest, AllWellKnownsInvalid) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  // Intentionally fail the requests for both IdPs by returning an invalid
  // well-known file.
  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.idp_info[kProviderUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};
  configuration.idp_info[kProviderTwoUrlFull].well_known.provider_urls =
      std::set<std::string>{"https://not-in-list.example"};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorConfigNotInWellKnown},
      /*selected_idp_config_url=*/absl::nullopt};

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_EQ(NumFetched(FetchedEndpoint::WELL_KNOWN), 2u);
  EXPECT_EQ(NumFetched(FetchedEndpoint::CONFIG), 2u);
  EXPECT_FALSE(DidFetch(FetchedEndpoint::ACCOUNTS));
}

// Test multi IDP FedCM request with duplicate IDPs should throw an error.
TEST_F(FederatedAuthRequestImplTest, DuplicateIdpMultiIdpRequest) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmMultipleIdentityProviders);

  RequestParameters request_parameters = kDefaultMultiIdpRequestParameters;
  request_parameters.identity_providers =
      std::vector<IdentityProviderParameters>{
          request_parameters.identity_providers[0],
          request_parameters.identity_providers[0]};

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};

  RunAuthTest(request_parameters, expectations, kConfigurationMultiIdpValid);
  EXPECT_FALSE(DidFetchAnyEndpoint());
  EXPECT_FALSE(did_show_accounts_dialog());
}

TEST_F(FederatedAuthRequestImplTest, TooManyRequests) {
  MockConfiguration configuration = kConfigurationValid;
  configuration.wait_for_callback = false;
  configuration.accounts_dialog_action = AccountsDialogAction::kNone;
  RequestExpectations expectations = {
      /*return_status=*/absl::nullopt,
      /*devtools_issue_statuses=*/{},
      /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  // Reset the network request manager so we can check that we fetch no
  // endpoints in the subsequent call.
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  SetNetworkRequestManager(std::make_unique<TestIdpNetworkRequestManager>());
  // The next FedCM request should fail since the initial request has not yet
  // been finalized.
  expectations = {RequestTokenStatus::kErrorTooManyRequests,
                  /*devtools_issue_statuses=*/{},
                  /*selected_idp_config_url=*/absl::nullopt};
  RunAuthTest(kDefaultRequestParameters, expectations, configuration);
  EXPECT_FALSE(DidFetchAnyEndpoint());
}

// TestIdpNetworkRequestManager subclass which records requests to metrics
// endpoint.
class IdpNetworkRequestMetricsRecorder : public TestIdpNetworkRequestManager {
 public:
  IdpNetworkRequestMetricsRecorder() = default;
  IdpNetworkRequestMetricsRecorder(const IdpNetworkRequestMetricsRecorder&) =
      delete;
  IdpNetworkRequestMetricsRecorder& operator=(
      const IdpNetworkRequestMetricsRecorder&) = delete;

  void SendSuccessfulTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      base::TimeDelta api_call_to_show_dialog_time,
      base::TimeDelta show_dialog_to_continue_clicked_time,
      base::TimeDelta account_selected_to_token_response_time,
      base::TimeDelta api_call_to_token_response_time) override {
    metrics_endpoints_notified_success_.push_back(metrics_endpoint_url);
  }

  void SendFailedTokenRequestMetrics(
      const GURL& metrics_endpoint_url,
      MetricsEndpointErrorCode error_code) override {
    metrics_endpoints_notified_failure_.push_back(metrics_endpoint_url);
  }

  const std::vector<GURL>& get_metrics_endpoints_notified_success() {
    return metrics_endpoints_notified_success_;
  }

  const std::vector<GURL>& get_metrics_endpoints_notified_failure() {
    return metrics_endpoints_notified_failure_;
  }

 private:
  std::vector<GURL> metrics_endpoints_notified_success_;
  std::vector<GURL> metrics_endpoints_notified_failure_;
};

// Test that the metrics endpoint is notified as a result of a successful
// multi-IDP FederatedAuthRequestImpl::RequestToken() call.
TEST_F(FederatedAuthRequestImplTest, MetricsEndpointMultiIdp) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmMetricsEndpoint,
                            features::kFedCmMultipleIdentityProviders},
      /*disabled_features=*/{});

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  RunAuthTest(kDefaultMultiIdpRequestParameters, kExpectationSuccess,
              kConfigurationMultiIdpValid);
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_success(),
              ElementsAre(kMetricsEndpoint));
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre("https://idp2.example/metrics"));
}

// Test that the metrics endpoint is notified when
// FederatedAuthRequestImpl::RequestToken() call fails.
TEST_F(FederatedAuthRequestImplTest, MetricsEndpointMultiIdpFail) {
  base::test::ScopedFeatureList list;
  list.InitWithFeatures(
      /*enabled_features=*/{features::kFedCmMetricsEndpoint,
                            features::kFedCmMultipleIdentityProviders},
      /*disabled_features=*/{});

  std::unique_ptr<IdpNetworkRequestMetricsRecorder> unique_metrics_recorder =
      std::make_unique<IdpNetworkRequestMetricsRecorder>();
  IdpNetworkRequestMetricsRecorder* metrics_recorder =
      unique_metrics_recorder.get();
  SetNetworkRequestManager(std::move(unique_metrics_recorder));

  RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kShouldEmbargo},
      /* selected_idp_config_url=*/absl::nullopt};

  MockConfiguration configuration = kConfigurationMultiIdpValid;
  configuration.accounts_dialog_action = AccountsDialogAction::kClose;

  RunAuthTest(kDefaultMultiIdpRequestParameters, expectations, configuration);
  EXPECT_TRUE(did_show_accounts_dialog());

  EXPECT_TRUE(
      metrics_recorder->get_metrics_endpoints_notified_success().empty());
  EXPECT_THAT(metrics_recorder->get_metrics_endpoints_notified_failure(),
              ElementsAre(kMetricsEndpoint, "https://idp2.example/metrics"));
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountIdMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.id = kAccountId;

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);
  EXPECT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountId);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountEmailMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.email = kEmail;

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);
  EXPECT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].email, kEmail);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountNoMatchNotRequired) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.id = "incorrect_login_hint";

  RunAuthTest(parameters, kExpectationSuccess, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
}

TEST_F(FederatedAuthRequestImplTest, LoginHintSingleAccountNoMatchRequired) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.id = "incorrect_login_hint";
  parameters.identity_providers[0].login_hint.is_required = true;
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty},
      /*selected_idp_config_url=*/absl::nullopt};

  RunAuthTest(parameters, expectations, kConfigurationValid);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

TEST_F(FederatedAuthRequestImplTest, LoginHintFirstAccountMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.id = kAccountIdNicolas;
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  EXPECT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountIdNicolas);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintLastAccountMatch) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.id = kAccountIdZach;
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  EXPECT_EQ(displayed_accounts().size(), 1u);
  EXPECT_EQ(displayed_accounts()[0].id, kAccountIdZach);
}

TEST_F(FederatedAuthRequestImplTest,
       LoginHintMultipleAccountsNoMatchNotRequired) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.email = "incorrect_login_hint";
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthTest(parameters, kExpectationSuccess, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_TRUE(did_show_accounts_dialog());
  EXPECT_EQ(displayed_accounts().size(), 3u);
}

TEST_F(FederatedAuthRequestImplTest, LoginHintMultipleAccountsNoMatchRequired) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmLoginHint);

  RequestParameters parameters = kDefaultRequestParameters;
  parameters.identity_providers[0].login_hint.email = "incorrect_login_hint";
  parameters.identity_providers[0].login_hint.is_required = true;
  const RequestExpectations expectations = {
      RequestTokenStatus::kError,
      {FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty},
      /*selected_idp_config_url=*/absl::nullopt};
  MockConfiguration configuration = kConfigurationValid;
  configuration.idp_info[kProviderUrlFull].accounts = kMultipleAccounts;

  RunAuthTest(parameters, expectations, configuration);
  EXPECT_TRUE(DidFetch(FetchedEndpoint::ACCOUNTS));
  EXPECT_FALSE(did_show_accounts_dialog());
}

// Test that when FedCmRpContext flag is enabled and rp_context is specified,
// the FedCM request succeeds with the specified rp_context.
TEST_F(FederatedAuthRequestImplTest, RpContextIsSetToNonDefaultValue) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmRpContext);

  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.rp_context = blink::mojom::RpContext::kContinue;
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  EXPECT_EQ(dialog_controller_state_.rp_context,
            blink::mojom::RpContext::kContinue);
}

// Test that when FedCmRpContext flag is at its default setting and rp_context
// is specified, the FedCM request ignores the specified rp_context and defaults
// to sign in.
TEST_F(FederatedAuthRequestImplTest, RpContextIsDefaultToSignIn) {
  RequestParameters request_parameters = kDefaultRequestParameters;
  request_parameters.rp_context = blink::mojom::RpContext::kContinue;
  MockConfiguration configuration = kConfigurationValid;
  configuration.accounts_dialog_action =
      AccountsDialogAction::kSelectFirstAccount;
  RunAuthTest(request_parameters, kExpectationSuccess, configuration);

  EXPECT_EQ(dialog_controller_state_.rp_context,
            blink::mojom::RpContext::kSignIn);
}

}  // namespace content
