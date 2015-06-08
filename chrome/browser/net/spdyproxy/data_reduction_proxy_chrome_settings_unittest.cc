// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

class DataReductionProxyChromeSettingsTest : public testing::Test {
 public:
  void SetUp() override {
    drp_chrome_settings_ =
        make_scoped_ptr(new DataReductionProxyChromeSettings());
    test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithParamsFlags(
                data_reduction_proxy::DataReductionProxyParams::kAllowed |
                data_reduction_proxy::DataReductionProxyParams::
                    kFallbackAllowed |
                data_reduction_proxy::DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(
                data_reduction_proxy::TestDataReductionProxyParams::
                    HAS_EVERYTHING &
                ~data_reduction_proxy::TestDataReductionProxyParams::
                    HAS_DEV_ORIGIN &
                ~data_reduction_proxy::TestDataReductionProxyParams::
                    HAS_DEV_FALLBACK_ORIGIN)
            .WithMockConfig()
            .SkipSettingsInitialization()
            .Build();
    config_ = test_context_->mock_config();
    drp_chrome_settings_->ResetConfigForTest(config_);
    dict_ = make_scoped_ptr(new base::DictionaryValue());

    PrefRegistrySimple* registry = test_context_->pref_service()->registry();
    registry->RegisterDictionaryPref(prefs::kProxy);
  }

  scoped_ptr<DataReductionProxyChromeSettings> drp_chrome_settings_;
  scoped_ptr<base::DictionaryValue> dict_;
  scoped_ptr<data_reduction_proxy::DataReductionProxyTestContext> test_context_;
  data_reduction_proxy::MockDataReductionProxyConfig* config_;
};

TEST_F(DataReductionProxyChromeSettingsTest, MigrateEmptyProxy) {
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);
  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      test_context_->pref_service());

  EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateSystemProxy) {
  dict_->SetString("mode", "system");
  test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      test_context_->pref_service());

  EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateDataReductionProxy) {
  const std::string kTestServers[] = {"http=http://proxy.googlezip.net",
                                      "http=https://my-drp.org",
                                      "https=https://tunneldrp.com"};

  for (const std::string& test_server : kTestServers) {
    dict_.reset(new base::DictionaryValue());
    dict_->SetString("mode", "fixed_servers");
    dict_->SetString("server", test_server);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_))
        .Times(1)
        .WillOnce(Return(true));

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
  }
}

TEST_F(DataReductionProxyChromeSettingsTest,
       MigrateGooglezipDataReductionProxy) {
  const std::string kTestServers[] = {
      "http=http://proxy-dev.googlezip.net",
      "http=https://arbitraryprefix.googlezip.net",
      "https=https://tunnel.googlezip.net"};

  for (const std::string& test_server : kTestServers) {
    dict_.reset(new base::DictionaryValue());
    // The proxy pref is set to a Data Reduction Proxy that doesn't match the
    // currently configured DRP, but the pref should still be cleared.
    dict_->SetString("mode", "fixed_servers");
    dict_->SetString("server", test_server);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_))
        .Times(1)
        .WillOnce(Return(false));

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
  }
}

TEST_F(DataReductionProxyChromeSettingsTest,
       MigratePacGooglezipDataReductionProxy) {
  const struct {
    const char* pac_url;
    bool expect_pref_cleared;
  } test_cases[] = {
      // PAC with bypass rules that returns 'HTTPS proxy.googlezip.net:443;
      // PROXY compress.googlezip.net:80; DIRECT'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkgeyAgaWYgKChzaEV4cE1hdGN"
       "oKHVybCwgJ2h0dHA6Ly93d3cuZ29vZ2xlLmNvbS9wb2xpY2llcy9wcml2YWN5KicpKSkgey"
       "AgICByZXR1cm4gJ0RJUkVDVCc7ICB9ICAgaWYgKHVybC5zdWJzdHJpbmcoMCwgNSkgPT0gJ"
       "2h0dHA6JykgeyAgICByZXR1cm4gJ0hUVFBTIHByb3h5Lmdvb2dsZXppcC5uZXQ6NDQzOyBQ"
       "Uk9YWSBjb21wcmVzcy5nb29nbGV6aXAubmV0OjgwOyBESVJFQ1QnOyAgfSAgcmV0dXJuICd"
       "ESVJFQ1QnO30=",
       true},
      // PAC with bypass rules that returns 'PROXY compress.googlezip.net:80;
      // DIRECT'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkgeyAgaWYgKChzaEV4cE1hdGN"
       "oKHVybCwgJ2h0dHA6Ly93d3cuZ29vZ2xlLmNvbS9wb2xpY2llcy9wcml2YWN5KicpKSkgey"
       "AgICByZXR1cm4gJ0RJUkVDVCc7ICB9ICAgaWYgKHVybC5zdWJzdHJpbmcoMCwgNSkgPT0gJ"
       "2h0dHA6JykgeyAgICByZXR1cm4gJ1BST1hZIGNvbXByZXNzLmdvb2dsZXppcC5uZXQ6ODA7"
       "IERJUkVDVCc7ICB9ICByZXR1cm4gJ0RJUkVDVCc7fQ==",
       true},
      // PAC with bypass rules that returns 'PROXY proxy-dev.googlezip.net:80;
      // DIRECT'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkgeyAgaWYgKChzaEV4cE1hdGN"
       "oKHVybCwgJ2h0dHA6Ly93d3cuZ29vZ2xlLmNvbS9wb2xpY2llcy9wcml2YWN5KicpKSkgey"
       "AgICByZXR1cm4gJ0RJUkVDVCc7ICB9ICAgaWYgKHVybC5zdWJzdHJpbmcoMCwgNSkgPT0gJ"
       "2h0dHA6JykgeyAgICByZXR1cm4gJ1BST1hZIHByb3h5LWRldi5nb29nbGV6aXAubmV0Ojgw"
       "OyBESVJFQ1QnOyAgfSAgcmV0dXJuICdESVJFQ1QnO30=",
       true},
      // Simple PAC that returns 'PROXY compress.googlezip.net:80'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkge3JldHVybiAnUFJPWFkgY29"
       "tcHJlc3MuZ29vZ2xlemlwLm5ldDo4MCc7fQo=",
       true},
      // Simple PAC that returns 'PROXY compress.googlezip.net'. Note that since
      // the port is not specified, the pref will not be cleared.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkge3JldHVybiAnUFJPWFkgY29"
       "tcHJlc3MuZ29vZ2xlemlwLm5ldCc7fQ==",
       false},
      // Simple PAC that returns 'PROXY mycustomdrp.net:80'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkge3JldHVybiAnUFJPWFkgb3J"
       "pZ2luLm5ldDo4MCc7fQo=",
       false},
      // Simple PAC that returns 'PROXY myprefixgooglezip.net:80'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkge3JldHVybiAnUFJPWFkgbXl"
       "wcmVmaXhnb29nbGV6aXAubmV0OjgwJzt9Cg==",
       false},
      // Simple PAC that returns 'PROXY compress.googlezip.net.mydomain.com:80'.
      {"data:application/"
       "x-ns-proxy-autoconfig;base64,"
       "ZnVuY3Rpb24gRmluZFByb3h5Rm9yVVJMKHVybCwgaG9zdCkge3JldHVybiAnUFJPWFkgY29"
       "tcHJlc3MuZ29vZ2xlemlwLm5ldC5teWRvbWFpbi5jb206ODAnO30K",
       false},
      // PAC URL that doesn't embed a script.
      {"http://compress.googlezip.net/pac", false},
  };

  for (const auto& test : test_cases) {
    dict_.reset(new base::DictionaryValue());
    dict_->SetString("mode", "pac_script");
    dict_->SetString("pac_url", test.pac_url);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    if (test.expect_pref_cleared) {
      EXPECT_EQ(NULL,
                test_context_->pref_service()->GetUserPref(prefs::kProxy));
    } else {
      const base::DictionaryValue* value;
      EXPECT_TRUE(test_context_->pref_service()
                      ->GetUserPref(prefs::kProxy)
                      ->GetAsDictionary(&value));
      std::string mode;
      EXPECT_TRUE(value->GetString("mode", &mode));
      EXPECT_EQ("pac_script", mode);
      std::string pac_url;
      EXPECT_TRUE(value->GetString("pac_url", &pac_url));
      EXPECT_EQ(test.pac_url, pac_url);
    }
  }
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateIgnoreOtherProxy) {
  const std::string kTestServers[] = {
      "http=https://youtube.com",
      "http=http://googlezip.net",
      "http=http://thisismyproxynotgooglezip.net",
      "https=http://arbitraryprefixgooglezip.net"};

  for (const std::string& test_server : kTestServers) {
    dict_.reset(new base::DictionaryValue());
    dict_->SetString("mode", "fixed_servers");
    dict_->SetString("server", test_server);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_))
        .Times(1)
        .WillOnce(Return(false));

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    base::DictionaryValue* value =
        (base::DictionaryValue*)test_context_->pref_service()->GetUserPref(
            prefs::kProxy);
    std::string mode;
    EXPECT_TRUE(value->GetString("mode", &mode));
    EXPECT_EQ("fixed_servers", mode);
    std::string server;
    EXPECT_TRUE(value->GetString("server", &server));
    EXPECT_EQ(test_server, server);
  }
}
