// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/io_thread.h"

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/leak_tracker.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_usage/tab_id_annotator.h"
#include "chrome/browser/net/async_dns_field_trial.h"
#include "chrome/browser/net/chrome_network_delegate.h"
#include "chrome/browser/net/connect_interceptor.h"
#include "chrome/browser/net/dns_probe_service.h"
#include "chrome/browser/net/proxy_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "components/net_log/chrome_net_log.h"
#include "components/policy/core/common/policy_service.h"
#include "components/proxy_config/pref_proxy_config_tracker.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cookie_store_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/external_estimate_provider.h"
#include "net/base/host_mapping_rules.h"
#include "net/base/net_util.h"
#include "net/base/network_quality_estimator.h"
#include "net/base/sdch_manager.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_known_logs_static.h"
#include "net/cert/ct_log_verifier.h"
#include "net/cert/ct_policy_enforcer.h"
#include "net/cert/ct_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/cert/multi_threaded_cert_verifier.h"
#include "net/cookies/cookie_store.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver.h"
#include "net/dns/mapped_host_resolver.h"
#include "net/ftp/ftp_network_layer.h"
#include "net/http/http_auth_filter.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_server_properties_impl.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_script_fetcher_impl.h"
#include "net/proxy/proxy_service.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/tcp_client_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"
#include "net/url_request/data_protocol_handler.h"
#include "net/url_request/file_protocol_handler.h"
#include "net/url_request/ftp_protocol_handler.h"
#include "net/url_request/static_http_user_agent_settings.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_backoff_manager.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/url_constants.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "policy/policy_constants.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/event_router_forwarder.h"
#endif

#if defined(USE_NSS_CERTS) || defined(OS_IOS)
#include "net/cert_net/nss_ocsp.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "base/android/build_info.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/android/net/external_estimate_provider_android.h"
#include "components/data_usage/android/traffic_stats_amortizer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/cert_verify_proc_chromeos.h"
#include "chromeos/network/host_resolver_impl_chromeos.h"
#endif


#include "content/nw/src/policy_cert_verifier.h"

using content::BrowserThread;

class SafeBrowsingURLRequestContext;

// The IOThread object must outlive any tasks posted to the IO thread before the
// Quit task, so base::Bind() calls are not refcounted.

namespace {

const char kTCPFastOpenFieldTrialName[] = "TCPFastOpen";
const char kTCPFastOpenHttpsEnabledGroupName[] = "HttpsEnabled";

const char kQuicFieldTrialName[] = "QUIC";
const char kQuicFieldTrialEnabledGroupName[] = "Enabled";
const char kQuicFieldTrialHttpsEnabledGroupName[] = "HttpsEnabled";

// The SPDY trial composes two different trial plus control groups:
//  * A "holdback" group with SPDY disabled, and corresponding control
//  (SPDY/3.1). The primary purpose of the holdback group is to encourage site
//  operators to do feature detection rather than UA-sniffing. As such, this
//  trial runs continuously.
//  * A SPDY/4 experiment, for SPDY/4 (aka HTTP/2) vs SPDY/3.1 comparisons and
//  eventual SPDY/4 deployment.
const char kSpdyFieldTrialName[] = "SPDY";
const char kSpdyFieldTrialHoldbackGroupNamePrefix[] = "SpdyDisabled";
const char kSpdyFieldTrialSpdy31GroupNamePrefix[] = "Spdy31Enabled";
const char kSpdyFieldTrialSpdy4GroupNamePrefix[] = "Spdy4Enabled";
const char kSpdyFieldTrialParametrizedPrefix[] = "Parametrized";

// Field trial for network quality estimator. Seeds RTT and downstream
// throughput observations with values that correspond to the connection type
// determined by the operating system.
const char kNetworkQualityEstimatorFieldTrialName[] = "NetworkQualityEstimator";

// Field trial for NPN.
const char kNpnTrialName[] = "NPN";
const char kNpnTrialEnabledGroupNamePrefix[] = "Enable";
const char kNpnTrialDisabledGroupNamePrefix[] = "Disable";

#if defined(OS_MACOSX) && !defined(OS_IOS)
void ObserveKeychainEvents() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  net::CertDatabase::GetInstance()->SetMessageLoopForKeychainEvents();
}
#endif

// Gets file path into ssl_keylog_file from command line argument or
// environment variable. Command line argument has priority when
// both specified.
base::FilePath GetSSLKeyLogFile(const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kSSLKeyLogFile)) {
    base::FilePath path =
        command_line.GetSwitchValuePath(switches::kSSLKeyLogFile);
    if (!path.empty())
      return path;
    LOG(WARNING) << "ssl-key-log-file argument missing";
  }

  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string path_str;
  env->GetVar("SSLKEYLOGFILE", &path_str);
#if defined(OS_WIN)
  // base::Environment returns environment variables in UTF-8 on Windows.
  return base::FilePath(base::UTF8ToUTF16(path_str));
#else
  return base::FilePath(path_str);
#endif
}

// Used for the "system" URLRequestContext.
class SystemURLRequestContext : public net::URLRequestContext {
 public:
  SystemURLRequestContext() {
#if defined(USE_NSS_CERTS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(this);
#endif
  }

 private:
  ~SystemURLRequestContext() override {
    AssertNoURLRequests();
#if defined(USE_NSS_CERTS) || defined(OS_IOS)
    net::SetURLRequestContextForNSSHttpIO(NULL);
#endif
  }
};

scoped_ptr<net::HostResolver> CreateGlobalHostResolver(net::NetLog* net_log) {
  TRACE_EVENT0("startup", "IOThread::CreateGlobalHostResolver");
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  net::HostResolver::Options options;

  // Use the retry attempts override from the command-line, if any.
  if (command_line.HasSwitch(switches::kHostResolverRetryAttempts)) {
    std::string s =
        command_line.GetSwitchValueASCII(switches::kHostResolverRetryAttempts);
    // Parse the switch (it should be a non-negative integer).
    int n;
    if (base::StringToInt(s, &n) && n >= 0) {
      options.max_retry_attempts = static_cast<size_t>(n);
    } else {
      LOG(ERROR) << "Invalid switch for host resolver retry attempts: " << s;
    }
  }

  scoped_ptr<net::HostResolver> global_host_resolver;
#if defined OS_CHROMEOS
  global_host_resolver =
      chromeos::HostResolverImplChromeOS::CreateSystemResolver(options,
                                                               net_log);
#else
  global_host_resolver =
      net::HostResolver::CreateSystemResolver(options, net_log);
#endif

  // If hostname remappings were specified on the command-line, layer these
  // rules on top of the real host resolver. This allows forwarding all requests
  // through a designated test server.
  if (!command_line.HasSwitch(switches::kHostResolverRules))
    return global_host_resolver;

  scoped_ptr<net::MappedHostResolver> remapped_resolver(
      new net::MappedHostResolver(std::move(global_host_resolver)));
  remapped_resolver->SetRulesFromString(
      command_line.GetSwitchValueASCII(switches::kHostResolverRules));
  return std::move(remapped_resolver);
}

int GetSwitchValueAsInt(const base::CommandLine& command_line,
                        const std::string& switch_name) {
  int value;
  if (!base::StringToInt(command_line.GetSwitchValueASCII(switch_name),
                         &value)) {
    return 0;
  }
  return value;
}

// Returns the value associated with |key| in |params| or "" if the
// key is not present in the map.
const std::string& GetVariationParam(
    const std::map<std::string, std::string>& params,
    const std::string& key) {
  std::map<std::string, std::string>::const_iterator it = params.find(key);
  if (it == params.end())
    return base::EmptyString();

  return it->second;
}

// Parse kUseSpdy command line flag options, which may contain the following:
//
//   "off"                      : Disables SPDY support entirely.
//   "no-ping"                  : Disables SPDY ping connection testing.
//   "exclude=<host>"           : Disables SPDY support for the host <host>.
//   "no-compress"              : Disables SPDY header compression.
//   "init-max-streams=<limit>" : Specifies the maximum number of concurrent
//                                streams for a SPDY session, unless the
//                                specifies a different value via SETTINGS.
void ConfigureSpdyGlobalsFromUseSpdyArgument(const std::string& mode,
                                             IOThread::Globals* globals) {
  static const char kOff[] = "off";
  static const char kDisablePing[] = "no-ping";
  static const char kExclude[] = "exclude";  // Hosts to exclude
  static const char kDisableCompression[] = "no-compress";
  static const char kInitialMaxConcurrentStreams[] = "init-max-streams";

  for (const base::StringPiece& element : base::SplitStringPiece(
           mode, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<base::StringPiece> name_value = base::SplitStringPiece(
        element, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    const base::StringPiece option =
        name_value.size() > 0 ? name_value[0] : base::StringPiece();
    const base::StringPiece value =
        name_value.size() > 1 ? name_value[1] : base::StringPiece();

    if (option == kOff) {
      net::HttpStreamFactory::set_spdy_enabled(false);
      continue;
    }
    if (option == kDisablePing) {
      globals->enable_spdy_ping_based_connection_checking.set(false);
      continue;
    }
    if (option == kExclude) {
      globals->forced_spdy_exclusions.insert(
          net::HostPortPair::FromURL(GURL(value.as_string())));
      continue;
    }
    if (option == kDisableCompression) {
      globals->enable_spdy_compression.set(false);
      continue;
    }
    if (option == kInitialMaxConcurrentStreams) {
      int streams;
      if (base::StringToInt(value, &streams)) {
        globals->initial_max_spdy_concurrent_streams.set(streams);
        continue;
      }
    }
    LOG(DFATAL) << "Unrecognized spdy option: " << option.as_string();
  }
}

}  // namespace

class IOThread::LoggingNetworkChangeObserver
    : public net::NetworkChangeNotifier::IPAddressObserver,
      public net::NetworkChangeNotifier::ConnectionTypeObserver,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // |net_log| must remain valid throughout our lifetime.
  explicit LoggingNetworkChangeObserver(net::NetLog* net_log)
      : net_log_(net_log) {
    net::NetworkChangeNotifier::AddIPAddressObserver(this);
    net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }

  ~LoggingNetworkChangeObserver() override {
    net::NetworkChangeNotifier::RemoveIPAddressObserver(this);
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
    net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  }

  // NetworkChangeNotifier::IPAddressObserver implementation.
  void OnIPAddressChanged() override {
    VLOG(1) << "Observed a change to the network IP addresses";

    net_log_->AddGlobalEntry(net::NetLog::TYPE_NETWORK_IP_ADDRESSES_CHANGED);
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation.
  void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a change to network connectivity state "
            << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CONNECTIVITY_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

  // NetworkChangeNotifier::NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override {
    std::string type_as_string =
        net::NetworkChangeNotifier::ConnectionTypeToString(type);

    VLOG(1) << "Observed a network change to state " << type_as_string;

    net_log_->AddGlobalEntry(
        net::NetLog::TYPE_NETWORK_CHANGED,
        net::NetLog::StringCallback("new_connection_type", &type_as_string));
  }

 private:
  net::NetLog* net_log_;
  DISALLOW_COPY_AND_ASSIGN(LoggingNetworkChangeObserver);
};

class SystemURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  explicit SystemURLRequestContextGetter(IOThread* io_thread);

  // Implementation for net::UrlRequestContextGetter.
  net::URLRequestContext* GetURLRequestContext() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override;

 protected:
  ~SystemURLRequestContextGetter() override;

 private:
  IOThread* const io_thread_;  // Weak pointer, owned by BrowserProcess.
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  base::debug::LeakTracker<SystemURLRequestContextGetter> leak_tracker_;
};

SystemURLRequestContextGetter::SystemURLRequestContextGetter(
    IOThread* io_thread)
    : io_thread_(io_thread),
      network_task_runner_(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)) {
}

SystemURLRequestContextGetter::~SystemURLRequestContextGetter() {}

net::URLRequestContext* SystemURLRequestContextGetter::GetURLRequestContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(io_thread_->globals()->system_request_context.get());

  return io_thread_->globals()->system_request_context.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
SystemURLRequestContextGetter::GetNetworkTaskRunner() const {
  return network_task_runner_;
}

IOThread::Globals::
SystemRequestContextLeakChecker::SystemRequestContextLeakChecker(
    Globals* globals)
    : globals_(globals) {
  DCHECK(globals_);
}

IOThread::Globals::
SystemRequestContextLeakChecker::~SystemRequestContextLeakChecker() {
  if (globals_->system_request_context.get())
    globals_->system_request_context->AssertNoURLRequests();
}

IOThread::Globals::Globals()
    : system_request_context_leak_checker(this),
      ignore_certificate_errors(false),
      testing_fixed_http_port(0),
      testing_fixed_https_port(0),
      enable_user_alternate_protocol_ports(false) {
}

IOThread::Globals::~Globals() {}

// |local_state| is passed in explicitly in order to (1) reduce implicit
// dependencies and (2) make IOThread more flexible for testing.
IOThread::IOThread(
    PrefService* local_state,
    policy::PolicyService* policy_service,
    net_log::ChromeNetLog* net_log,
    extensions::EventRouterForwarder* extension_event_router_forwarder)
    : net_log_(net_log),
#if defined(ENABLE_EXTENSIONS)
      extension_event_router_forwarder_(extension_event_router_forwarder),
#endif
      globals_(NULL),
      is_spdy_disabled_by_policy_(false),
      is_quic_allowed_by_policy_(true),
      creation_time_(base::TimeTicks::Now()),
      weak_factory_(this) {
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_proxy =
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO);
  auth_schemes_ = local_state->GetString(prefs::kAuthSchemes);
  negotiate_disable_cname_lookup_.Init(
      prefs::kDisableAuthNegotiateCnameLookup, local_state,
      base::Bind(&IOThread::UpdateNegotiateDisableCnameLookup,
                 base::Unretained(this)));
  negotiate_disable_cname_lookup_.MoveToThread(io_thread_proxy);
  negotiate_enable_port_.Init(
      prefs::kEnableAuthNegotiatePort, local_state,
      base::Bind(&IOThread::UpdateNegotiateEnablePort, base::Unretained(this)));
  negotiate_enable_port_.MoveToThread(io_thread_proxy);
  auth_server_whitelist_.Init(
      prefs::kAuthServerWhitelist, local_state,
      base::Bind(&IOThread::UpdateServerWhitelist, base::Unretained(this)));
  auth_server_whitelist_.MoveToThread(io_thread_proxy);
  auth_delegate_whitelist_.Init(
      prefs::kAuthNegotiateDelegateWhitelist, local_state,
      base::Bind(&IOThread::UpdateDelegateWhitelist, base::Unretained(this)));
  auth_delegate_whitelist_.MoveToThread(io_thread_proxy);
#if defined(OS_ANDROID)
  auth_android_negotiate_account_type_.Init(
      prefs::kAuthAndroidNegotiateAccountType, local_state,
      base::Bind(&IOThread::UpdateAndroidAuthNegotiateAccountType,
                 base::Unretained(this)));
  auth_android_negotiate_account_type_.MoveToThread(io_thread_proxy);
#endif
#if defined(OS_POSIX) && !defined(OS_ANDROID)
  gssapi_library_name_ = local_state->GetString(prefs::kGSSAPILibraryName);
#endif
  pref_proxy_config_tracker_.reset(
      ProxyServiceFactory::CreatePrefProxyConfigTrackerOfLocalState(
          local_state));
  ChromeNetworkDelegate::InitializePrefsOnUIThread(
      &system_enable_referrers_,
      NULL,
      NULL,
      NULL,
      local_state);
  ssl_config_service_manager_.reset(
      ssl_config::SSLConfigServiceManager::CreateDefaultManager(
          local_state,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));

  base::Value* dns_client_enabled_default = new base::FundamentalValue(
      chrome_browser_net::ConfigureAsyncDnsFieldTrial());
  local_state->SetDefaultPrefValue(prefs::kBuiltInDnsClientEnabled,
                                   dns_client_enabled_default);
  chrome_browser_net::LogAsyncDnsPrefSource(
      local_state->FindPreference(prefs::kBuiltInDnsClientEnabled));

  dns_client_enabled_.Init(prefs::kBuiltInDnsClientEnabled,
                           local_state,
                           base::Bind(&IOThread::UpdateDnsClientEnabled,
                                      base::Unretained(this)));
  dns_client_enabled_.MoveToThread(io_thread_proxy);

  quick_check_enabled_.Init(prefs::kQuickCheckEnabled,
                            local_state);
  quick_check_enabled_.MoveToThread(io_thread_proxy);

#if defined(ENABLE_CONFIGURATION_POLICY)
  is_spdy_disabled_by_policy_ = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string())).Get(
          policy::key::kDisableSpdy) != NULL;

  const base::Value* value = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
      std::string())).GetValue(policy::key::kQuicAllowed);
  if (value)
    value->GetAsBoolean(&is_quic_allowed_by_policy_);
#endif  // ENABLE_CONFIGURATION_POLICY

  BrowserThread::SetDelegate(BrowserThread::IO, this);
}

IOThread::~IOThread() {
  // This isn't needed for production code, but in tests, IOThread may
  // be multiply constructed.
  BrowserThread::SetDelegate(BrowserThread::IO, NULL);

  pref_proxy_config_tracker_->DetachFromPrefService();
  DCHECK(!globals_);
}

IOThread::Globals* IOThread::globals() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return globals_;
}

void IOThread::SetGlobalsForTesting(Globals* globals) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!globals || !globals_);
  globals_ = globals;
}

net_log::ChromeNetLog* IOThread::net_log() {
  return net_log_;
}

void IOThread::ChangedToOnTheRecord() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::ChangedToOnTheRecordOnIOThread,
                 base::Unretained(this)));
}

net::URLRequestContextGetter* IOThread::system_url_request_context_getter() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!system_url_request_context_getter_.get()) {
    InitSystemRequestContext();
  }
  return system_url_request_context_getter_.get();
}

void IOThread::Init() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("466432 IOThread::InitAsync::Start"));
  TRACE_EVENT0("startup", "IOThread::InitAsync");
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

#if defined(USE_NSS_CERTS) || defined(OS_IOS)
  net::SetMessageLoopForNSSHttpIO();
#endif

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CommandLineForCurrentProcess"));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Export ssl keys if log file specified.
  base::FilePath ssl_keylog_file = GetSSLKeyLogFile(command_line);
  if (!ssl_keylog_file.empty()) {
    net::SSLClientSocket::SetSSLKeyLogFile(
        ssl_keylog_file,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }

  DCHECK(!globals_);
  globals_ = new Globals;

  // Add an observer that will emit network change events to the ChromeNetLog.
  // Assuming NetworkChangeNotifier dispatches in FIFO order, we should be
  // logging the network change before other IO thread consumers respond to it.
  network_change_observer_.reset(
      new LoggingNetworkChangeObserver(net_log_));

  // Setup the HistogramWatcher to run on the IO thread.
  net::NetworkChangeNotifier::InitHistogramWatcher();

#if defined(ENABLE_EXTENSIONS)
  globals_->extension_event_router_forwarder =
      extension_event_router_forwarder_;
#endif

  scoped_ptr<data_usage::DataUseAmortizer> data_use_amortizer;
#if BUILDFLAG(ANDROID_JAVA_UI)
  data_use_amortizer.reset(new data_usage::android::TrafficStatsAmortizer());
#endif

  globals_->data_use_aggregator.reset(new data_usage::DataUseAggregator(
      scoped_ptr<data_usage::DataUseAnnotator>(
          new chrome_browser_data_usage::TabIdAnnotator()),
      std::move(data_use_amortizer)));

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::ChromeNetworkDelegate"));
  scoped_ptr<ChromeNetworkDelegate> chrome_network_delegate(
      new ChromeNetworkDelegate(extension_event_router_forwarder(),
                                &system_enable_referrers_));
  // By default, data usage is considered off the record.
  chrome_network_delegate->set_data_use_aggregator(
      globals_->data_use_aggregator.get(),
      true /* is_data_usage_off_the_record */);

#if BUILDFLAG(ANDROID_JAVA_UI)
  globals_->external_data_use_observer.reset(
      new chrome::android::ExternalDataUseObserver(
          globals_->data_use_aggregator.get(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI)));
#endif

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateGlobalHostResolver"));
  globals_->system_network_delegate = std::move(chrome_network_delegate);
  globals_->host_resolver = CreateGlobalHostResolver(net_log_);

  std::map<std::string, std::string> network_quality_estimator_params;
  variations::GetVariationParams(kNetworkQualityEstimatorFieldTrialName,
                                 &network_quality_estimator_params);

  scoped_ptr<net::ExternalEstimateProvider> external_estimate_provider;
#if BUILDFLAG(ANDROID_JAVA_UI)
  external_estimate_provider.reset(
      new chrome::android::ExternalEstimateProviderAndroid());
#endif
  // Pass ownership.
  globals_->network_quality_estimator.reset(new net::NetworkQualityEstimator(
      std::move(external_estimate_provider), network_quality_estimator_params));

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::UpdateDnsClientEnabled::Start"));
  UpdateDnsClientEnabled();
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile6(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::UpdateDnsClientEnabled::End"));
#if defined(OS_CHROMEOS)
  // Creates a CertVerifyProc that doesn't allow any profile-provided certs.
  globals_->cert_verifier.reset(new net::MultiThreadedCertVerifier(
      new chromeos::CertVerifyProcChromeOS()));
#else
  globals_->cert_verifier.reset(new nw::PolicyCertVerifier());

#endif

  globals_->transport_security_state.reset(new net::TransportSecurityState());

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile8(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateLogVerifiers::Start"));
  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs(
      net::ct::CreateLogVerifiersForKnownLogs());

  // Add logs from command line
  if (command_line.HasSwitch(switches::kCertificateTransparencyLog)) {
    std::string switch_value = command_line.GetSwitchValueASCII(
        switches::kCertificateTransparencyLog);
    for (const base::StringPiece& curr_log : base::SplitStringPiece(
             switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
      std::vector<std::string> log_metadata = base::SplitString(
          curr_log, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      CHECK_GE(log_metadata.size(), 3u)
          << "CT log metadata missing: Switch format is "
          << "'description:base64_key:url_without_schema'.";
      std::string log_description(log_metadata[0]);
      std::string log_url(std::string("https://") + log_metadata[2]);
      std::string ct_public_key_data;
      CHECK(base::Base64Decode(log_metadata[1], &ct_public_key_data))
          << "Unable to decode CT public key.";
      scoped_refptr<const net::CTLogVerifier> external_log_verifier(
          net::CTLogVerifier::Create(ct_public_key_data, log_description,
                                     log_url));
      CHECK(external_log_verifier) << "Unable to parse CT public key.";
      VLOG(1) << "Adding log with description " << log_description;
      ct_logs.push_back(external_log_verifier);
    }
  }

  globals_->ct_logs.assign(ct_logs.begin(), ct_logs.end());

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile9(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateLogVerifiers::End"));
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile7(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateMultiLogVerifier"));
  net::MultiLogCTVerifier* ct_verifier = new net::MultiLogCTVerifier();
  globals_->cert_transparency_verifier.reset(ct_verifier);
  // Add built-in logs
  ct_verifier->AddLogs(globals_->ct_logs);

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile10(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CTPolicyEnforcer"));
  net::CTPolicyEnforcer* policy_enforcer = new net::CTPolicyEnforcer;
  globals_->ct_policy_enforcer.reset(policy_enforcer);

  globals_->ssl_config_service = GetSSLConfigService();

  CreateDefaultAuthHandlerFactory();
  globals_->http_server_properties.reset(new net::HttpServerPropertiesImpl());
  // For the ProxyScriptFetcher, we use a direct ProxyService.
  globals_->proxy_script_fetcher_proxy_service =
      net::ProxyService::CreateDirectWithNetLog(net_log_);
  // In-memory cookie store.
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile11(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateCookieStore::Start"));
  globals_->system_cookie_store =
        content::CreateCookieStore(content::CookieStoreConfig());
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateCookieStore::End"));
  // In-memory channel ID store.
  globals_->system_channel_id_service.reset(
      new net::ChannelIDService(
          new net::DefaultChannelIDStore(NULL),
          base::WorkerPool::GetTaskRunner(true)));
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12_1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateDnsProbeService"));
  globals_->dns_probe_service.reset(new chrome_browser_net::DnsProbeService());
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12_2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateHostMappingRules"));
  globals_->host_mapping_rules.reset(new net::HostMappingRules());
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12_3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CreateHTTPUserAgentSettings"));
  globals_->http_user_agent_settings.reset(
      new net::StaticHttpUserAgentSettings(std::string(), GetUserAgent()));
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12_4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::CommandLineConfiguration"));
  if (command_line.HasSwitch(switches::kHostRules)) {
    TRACE_EVENT_BEGIN0("startup", "IOThread::InitAsync:SetRulesFromString");
    globals_->host_mapping_rules->SetRulesFromString(
        command_line.GetSwitchValueASCII(switches::kHostRules));
    TRACE_EVENT_END0("startup", "IOThread::InitAsync:SetRulesFromString");
  }
  if (command_line.HasSwitch(switches::kIgnoreCertificateErrors))
    globals_->ignore_certificate_errors = true;
  if (command_line.HasSwitch(switches::kTestingFixedHttpPort)) {
    globals_->testing_fixed_http_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpPort);
  }
  if (command_line.HasSwitch(switches::kTestingFixedHttpsPort)) {
    globals_->testing_fixed_https_port =
        GetSwitchValueAsInt(command_line, switches::kTestingFixedHttpsPort);
  }
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile12_5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::QuicConfiguration"));
  ConfigureQuic(command_line);
  if (command_line.HasSwitch(
          switches::kEnableUserAlternateProtocolPorts)) {
    globals_->enable_user_alternate_protocol_ports = true;
  }
  globals_->enable_brotli.set(
      base::FeatureList::IsEnabled(features::kBrotliEncoding));
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile13(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::InitAsync::InitializeNetworkOptions"));
  InitializeNetworkOptions(command_line);

  TRACE_EVENT_BEGIN0("startup",
                     "IOThread::Init:ProxyScriptFetcherRequestContext");
  globals_->proxy_script_fetcher_context.reset(
      ConstructProxyScriptFetcherContext(globals_, net_log_));
  TRACE_EVENT_END0("startup",
                   "IOThread::Init:ProxyScriptFetcherRequestContext");

  const version_info::Channel channel = chrome::GetChannel();
  if (channel == version_info::Channel::UNKNOWN ||
      channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEV) {
    globals_->url_request_backoff_manager.reset(
        new net::URLRequestBackoffManager());
  }

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Start observing Keychain events. This needs to be done on the UI thread,
  // as Keychain services requires a CFRunLoop.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&ObserveKeychainEvents));
#endif

  // InitSystemRequestContext turns right around and posts a task back
  // to the IO thread, so we can't let it run until we know the IO
  // thread has started.
  //
  // Note that since we are at BrowserThread::Init time, the UI thread
  // is blocked waiting for the thread to start.  Therefore, posting
  // this task to the main thread's message loop here is guaranteed to
  // get it onto the message loop while the IOThread object still
  // exists.  However, the message might not be processed on the UI
  // thread until after IOThread is gone, so use a weak pointer.
  BrowserThread::PostTask(BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&IOThread::InitSystemRequestContext,
                                     weak_factory_.GetWeakPtr()));
}

void IOThread::CleanUp() {
  base::debug::LeakTracker<SafeBrowsingURLRequestContext>::CheckForLeaks();

#if defined(USE_NSS_CERTS) || defined(OS_IOS)
  net::ShutdownNSSHttpIO();
#endif

  system_url_request_context_getter_ = NULL;

  // Release objects that the net::URLRequestContext could have been pointing
  // to.

  // Shutdown the HistogramWatcher on the IO thread.
  net::NetworkChangeNotifier::ShutdownHistogramWatcher();

  // This must be reset before the ChromeNetLog is destroyed.
  network_change_observer_.reset();

  system_proxy_config_service_.reset();
  delete globals_;
  globals_ = NULL;

  base::debug::LeakTracker<SystemURLRequestContextGetter>::CheckForLeaks();
}

void IOThread::InitializeNetworkOptions(const base::CommandLine& command_line) {
  // Only handle use-spdy command line flags if "spdy.disabled" preference is
  // not disabled via policy.
  if (is_spdy_disabled_by_policy_) {
    base::FieldTrial* trial = base::FieldTrialList::Find(kSpdyFieldTrialName);
    if (trial)
      trial->Disable();
  } else {
    std::string group = base::FieldTrialList::FindFullName(kSpdyFieldTrialName);
    VariationParameters params;
    if (!variations::GetVariationParams(kSpdyFieldTrialName, &params)) {
      params.clear();
    }
    ConfigureSpdyGlobals(command_line, group, params, globals_);
  }

  ConfigureTCPFastOpen(command_line);

  ConfigureNPNGlobals(base::FieldTrialList::FindFullName(kNpnTrialName),
                      globals_);

  // TODO(rch): Make the client socket factory a per-network session
  // instance, constructed from a NetworkSession::Params, to allow us
  // to move this option to IOThread::Globals &
  // HttpNetworkSession::Params.
}

void IOThread::ConfigureTCPFastOpen(const base::CommandLine& command_line) {
  const std::string trial_group =
      base::FieldTrialList::FindFullName(kTCPFastOpenFieldTrialName);
  if (trial_group == kTCPFastOpenHttpsEnabledGroupName)
    globals_->enable_tcp_fast_open_for_ssl.set(true);
  bool always_enable_if_supported =
      command_line.HasSwitch(switches::kEnableTcpFastOpen);
  // Check for OS support of TCP FastOpen, and turn it on for all connections
  // if indicated by user.
  net::CheckSupportAndMaybeEnableTCPFastOpen(always_enable_if_supported);
}

// static
void IOThread::ConfigureSpdyGlobals(
    const base::CommandLine& command_line,
    base::StringPiece spdy_trial_group,
    const VariationParameters& spdy_trial_params,
    IOThread::Globals* globals) {
  if (command_line.HasSwitch(switches::kTrustedSpdyProxy)) {
    globals->trusted_spdy_proxy.set(
        command_line.GetSwitchValueASCII(switches::kTrustedSpdyProxy));
  }
  if (command_line.HasSwitch(switches::kIgnoreUrlFetcherCertRequests))
    net::URLFetcher::SetIgnoreCertificateRequests(true);

  if (command_line.HasSwitch(switches::kUseSpdy)) {
    std::string spdy_mode =
        command_line.GetSwitchValueASCII(switches::kUseSpdy);
    ConfigureSpdyGlobalsFromUseSpdyArgument(spdy_mode, globals);
    return;
  }

  globals->next_protos.clear();

  bool enable_quic = false;
  globals->enable_quic.CopyToIfSet(&enable_quic);
  if (enable_quic) {
    globals->next_protos.push_back(net::kProtoQUIC1SPDY3);
  }

  // No SPDY command-line flags have been specified. Examine trial groups.
  if (spdy_trial_group.starts_with(kSpdyFieldTrialHoldbackGroupNamePrefix)) {
    net::HttpStreamFactory::set_spdy_enabled(false);
  } else if (spdy_trial_group.starts_with(
                 kSpdyFieldTrialSpdy31GroupNamePrefix)) {
    globals->next_protos.push_back(net::kProtoSPDY31);
  } else if (spdy_trial_group.starts_with(
                 kSpdyFieldTrialSpdy4GroupNamePrefix)) {
    globals->next_protos.push_back(net::kProtoHTTP2);
    globals->next_protos.push_back(net::kProtoSPDY31);
  } else if (spdy_trial_group.starts_with(kSpdyFieldTrialParametrizedPrefix)) {
    bool spdy_enabled = false;
    if (base::LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_http2"), "true")) {
      globals->next_protos.push_back(net::kProtoHTTP2);
      spdy_enabled = true;
    }
    if (base::LowerCaseEqualsASCII(
            GetVariationParam(spdy_trial_params, "enable_spdy31"), "true")) {
      globals->next_protos.push_back(net::kProtoSPDY31);
      spdy_enabled = true;
    }
    // TODO(bnc): HttpStreamFactory::spdy_enabled_ is redundant with
    // globals->next_protos, can it be eliminated?
    net::HttpStreamFactory::set_spdy_enabled(spdy_enabled);
  } else {
    // By default, enable HTTP/2.
    globals->next_protos.push_back(net::kProtoHTTP2);
    globals->next_protos.push_back(net::kProtoSPDY31);
  }

  // Enable HTTP/1.1 in all cases as the last protocol.
  globals->next_protos.push_back(net::kProtoHTTP11);
}

// static
void IOThread::ConfigureNPNGlobals(base::StringPiece npn_trial_group,
                                   IOThread::Globals* globals) {
  if (npn_trial_group.starts_with(kNpnTrialEnabledGroupNamePrefix)) {
    globals->enable_npn.set(true);
  } else if (npn_trial_group.starts_with(kNpnTrialDisabledGroupNamePrefix)) {
    globals->enable_npn.set(false);
  }
}

void IOThread::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kAuthSchemes,
                               "basic,digest,ntlm,negotiate");
  registry->RegisterBooleanPref(prefs::kDisableAuthNegotiateCnameLookup, false);
  registry->RegisterBooleanPref(prefs::kEnableAuthNegotiatePort, false);
  registry->RegisterStringPref(prefs::kAuthServerWhitelist, std::string());
  registry->RegisterStringPref(prefs::kAuthNegotiateDelegateWhitelist,
                               std::string());
  registry->RegisterStringPref(prefs::kGSSAPILibraryName, std::string());
  registry->RegisterStringPref(prefs::kAuthAndroidNegotiateAccountType,
                               std::string());
  registry->RegisterBooleanPref(prefs::kEnableReferrers, true);
  data_reduction_proxy::RegisterPrefs(registry);
  registry->RegisterBooleanPref(prefs::kBuiltInDnsClientEnabled, true);
  registry->RegisterBooleanPref(prefs::kQuickCheckEnabled, true);
}

void IOThread::UpdateServerWhitelist() {
  globals_->http_auth_preferences->set_server_whitelist(
      auth_server_whitelist_.GetValue());
}

void IOThread::UpdateDelegateWhitelist() {
  globals_->http_auth_preferences->set_delegate_whitelist(
      auth_delegate_whitelist_.GetValue());
}

#if defined(OS_ANDROID)
void IOThread::UpdateAndroidAuthNegotiateAccountType() {
  globals_->http_auth_preferences->set_auth_android_negotiate_account_type(
      auth_android_negotiate_account_type_.GetValue());
}
#endif

void IOThread::UpdateNegotiateDisableCnameLookup() {
  globals_->http_auth_preferences->set_negotiate_disable_cname_lookup(
      negotiate_disable_cname_lookup_.GetValue());
}

void IOThread::UpdateNegotiateEnablePort() {
  globals_->http_auth_preferences->set_negotiate_enable_port(
      negotiate_enable_port_.GetValue());
}

void IOThread::CreateDefaultAuthHandlerFactory() {
  std::vector<std::string> supported_schemes = base::SplitString(
      auth_schemes_, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  globals_->http_auth_preferences.reset(new net::HttpAuthPreferences(
      supported_schemes
#if defined(OS_POSIX) && !defined(OS_ANDROID)
      ,
      gssapi_library_name_
#endif
      ));
  UpdateServerWhitelist();
  UpdateDelegateWhitelist();
  UpdateNegotiateDisableCnameLookup();
  UpdateNegotiateEnablePort();
#if defined(OS_ANDROID)
  UpdateAndroidAuthNegotiateAccountType();
#endif
  globals_->http_auth_handler_factory =
      net::HttpAuthHandlerRegistryFactory::Create(
          globals_->http_auth_preferences.get(), globals_->host_resolver.get());
}

void IOThread::ClearHostCache() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::HostCache* host_cache = globals_->host_resolver->GetHostCache();
  if (host_cache)
    host_cache->clear();
}

void IOThread::InitializeNetworkSessionParams(
    net::HttpNetworkSession::Params* params) {
  InitializeNetworkSessionParamsFromGlobals(*globals_, params);
}

void IOThread::InitializeNetworkSessionParamsFromGlobals(
    const IOThread::Globals& globals,
    net::HttpNetworkSession::Params* params) {
  //  The next two properties of the params don't seem to be
  // elements of URLRequestContext, so they must be set here.
  params->ct_policy_enforcer = globals.ct_policy_enforcer.get();
  params->host_mapping_rules = globals.host_mapping_rules.get();

  params->ignore_certificate_errors = globals.ignore_certificate_errors;
  params->testing_fixed_http_port = globals.testing_fixed_http_port;
  params->testing_fixed_https_port = globals.testing_fixed_https_port;
  globals.enable_tcp_fast_open_for_ssl.CopyToIfSet(
      &params->enable_tcp_fast_open_for_ssl);

  globals.initial_max_spdy_concurrent_streams.CopyToIfSet(
      &params->spdy_initial_max_concurrent_streams);
  globals.enable_spdy_compression.CopyToIfSet(
      &params->enable_spdy_compression);
  globals.enable_spdy_ping_based_connection_checking.CopyToIfSet(
      &params->enable_spdy_ping_based_connection_checking);
  globals.spdy_default_protocol.CopyToIfSet(
      &params->spdy_default_protocol);
  params->next_protos = globals.next_protos;
  globals.trusted_spdy_proxy.CopyToIfSet(&params->trusted_spdy_proxy);
  params->forced_spdy_exclusions = globals.forced_spdy_exclusions;
  globals.use_alternative_services.CopyToIfSet(
      &params->use_alternative_services);
  globals.alternative_service_probability_threshold.CopyToIfSet(
      &params->alternative_service_probability_threshold);

  globals.enable_npn.CopyToIfSet(&params->enable_npn);

  globals.enable_brotli.CopyToIfSet(&params->enable_brotli);

  globals.enable_quic.CopyToIfSet(&params->enable_quic);
  globals.enable_quic_for_proxies.CopyToIfSet(&params->enable_quic_for_proxies);
  globals.quic_always_require_handshake_confirmation.CopyToIfSet(
      &params->quic_always_require_handshake_confirmation);
  globals.quic_disable_connection_pooling.CopyToIfSet(
      &params->quic_disable_connection_pooling);
  globals.quic_load_server_info_timeout_srtt_multiplier.CopyToIfSet(
      &params->quic_load_server_info_timeout_srtt_multiplier);
  globals.quic_enable_connection_racing.CopyToIfSet(
      &params->quic_enable_connection_racing);
  globals.quic_enable_non_blocking_io.CopyToIfSet(
      &params->quic_enable_non_blocking_io);
  globals.quic_prefer_aes.CopyToIfSet(&params->quic_prefer_aes);
  globals.quic_disable_disk_cache.CopyToIfSet(
      &params->quic_disable_disk_cache);
  globals.quic_max_number_of_lossy_connections.CopyToIfSet(
      &params->quic_max_number_of_lossy_connections);
  globals.quic_packet_loss_threshold.CopyToIfSet(
      &params->quic_packet_loss_threshold);
  globals.quic_socket_receive_buffer_size.CopyToIfSet(
      &params->quic_socket_receive_buffer_size);
  globals.quic_delay_tcp_race.CopyToIfSet(&params->quic_delay_tcp_race);
  globals.enable_quic_port_selection.CopyToIfSet(
      &params->enable_quic_port_selection);
  globals.quic_max_packet_length.CopyToIfSet(&params->quic_max_packet_length);
  globals.quic_user_agent_id.CopyToIfSet(&params->quic_user_agent_id);
  globals.quic_supported_versions.CopyToIfSet(
      &params->quic_supported_versions);
  params->quic_connection_options = globals.quic_connection_options;
  globals.quic_close_sessions_on_ip_change.CopyToIfSet(
      &params->quic_close_sessions_on_ip_change);
  globals.quic_idle_connection_timeout_seconds.CopyToIfSet(
      &params->quic_idle_connection_timeout_seconds);
  globals.quic_disable_preconnect_if_0rtt.CopyToIfSet(
      &params->quic_disable_preconnect_if_0rtt);
  if (!globals.quic_host_whitelist.empty())
    params->quic_host_whitelist = globals.quic_host_whitelist;
  globals.quic_migrate_sessions_on_network_change.CopyToIfSet(
      &params->quic_migrate_sessions_on_network_change);
  globals.origin_to_force_quic_on.CopyToIfSet(
      &params->origin_to_force_quic_on);
  params->enable_user_alternate_protocol_ports =
      globals.enable_user_alternate_protocol_ports;
}

base::TimeTicks IOThread::creation_time() const {
  return creation_time_;
}

net::SSLConfigService* IOThread::GetSSLConfigService() {
  return ssl_config_service_manager_->Get();
}

void IOThread::ChangedToOnTheRecordOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Clear the host cache to avoid showing entries from the OTR session
  // in about:net-internals.
  ClearHostCache();
}

void IOThread::InitSystemRequestContext() {
  if (system_url_request_context_getter_.get())
    return;
  // If we're in unit_tests, IOThread may not be run.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::IO))
    return;
  system_proxy_config_service_ = ProxyServiceFactory::CreateProxyConfigService(
      pref_proxy_config_tracker_.get());
  system_url_request_context_getter_ =
      new SystemURLRequestContextGetter(this);
  // Safe to post an unretained this pointer, since IOThread is
  // guaranteed to outlive the IO BrowserThread.
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&IOThread::InitSystemRequestContextOnIOThread,
                 base::Unretained(this)));
}

void IOThread::InitSystemRequestContextOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!globals_->system_proxy_service.get());
  DCHECK(system_proxy_config_service_.get());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  globals_->system_proxy_service = ProxyServiceFactory::CreateProxyService(
      net_log_, globals_->proxy_script_fetcher_context.get(),
      globals_->system_network_delegate.get(),
      std::move(system_proxy_config_service_), command_line,
      quick_check_enabled_.GetValue());

  globals_->system_request_context.reset(
      ConstructSystemRequestContext(globals_, net_log_));
}

void IOThread::UpdateDnsClientEnabled() {
  globals()->host_resolver->SetDnsClientEnabled(*dns_client_enabled_);
}

void IOThread::ConfigureQuic(const base::CommandLine& command_line) {
  // Always fetch the field trial group to ensure it is reported correctly.
  // The command line flags will be associated with a group that is reported
  // so long as trial is actually queried.
  std::string group =
      base::FieldTrialList::FindFullName(kQuicFieldTrialName);
  VariationParameters params;
  if (!variations::GetVariationParams(kQuicFieldTrialName, &params)) {
    params.clear();
  }

  ConfigureQuicGlobals(command_line, group, params, is_quic_allowed_by_policy_,
                       globals_);
}

void IOThread::ConfigureQuicGlobals(
    const base::CommandLine& command_line,
    base::StringPiece quic_trial_group,
    const VariationParameters& quic_trial_params,
    bool quic_allowed_by_policy,
    IOThread::Globals* globals) {
  bool enable_quic = ShouldEnableQuic(command_line, quic_trial_group,
                                      quic_allowed_by_policy);
  globals->enable_quic.set(enable_quic);
  bool enable_quic_for_proxies = ShouldEnableQuicForProxies(
      command_line, quic_trial_group, quic_allowed_by_policy);
  globals->enable_quic_for_proxies.set(enable_quic_for_proxies);
  globals->use_alternative_services.set(
      ShouldQuicEnableAlternativeServices(command_line, quic_trial_params));
  if (enable_quic) {
    globals->quic_always_require_handshake_confirmation.set(
        ShouldQuicAlwaysRequireHandshakeConfirmation(quic_trial_params));
    globals->quic_disable_connection_pooling.set(
        ShouldQuicDisableConnectionPooling(quic_trial_params));
    int receive_buffer_size = GetQuicSocketReceiveBufferSize(quic_trial_params);
    if (receive_buffer_size != 0) {
      globals->quic_socket_receive_buffer_size.set(receive_buffer_size);
    }
    globals->quic_delay_tcp_race.set(ShouldQuicDelayTcpRace(quic_trial_params));
    float load_server_info_timeout_srtt_multiplier =
        GetQuicLoadServerInfoTimeoutSrttMultiplier(quic_trial_params);
    if (load_server_info_timeout_srtt_multiplier != 0) {
      globals->quic_load_server_info_timeout_srtt_multiplier.set(
          load_server_info_timeout_srtt_multiplier);
    }
    globals->quic_enable_connection_racing.set(
        ShouldQuicEnableConnectionRacing(quic_trial_params));
    globals->quic_enable_non_blocking_io.set(
        ShouldQuicEnableNonBlockingIO(quic_trial_params));
    globals->quic_disable_disk_cache.set(
        ShouldQuicDisableDiskCache(quic_trial_params));
    globals->quic_prefer_aes.set(
        ShouldQuicPreferAes(quic_trial_params));
    int max_number_of_lossy_connections = GetQuicMaxNumberOfLossyConnections(
        quic_trial_params);
    if (max_number_of_lossy_connections != 0) {
      globals->quic_max_number_of_lossy_connections.set(
          max_number_of_lossy_connections);
    }
    float packet_loss_threshold = GetQuicPacketLossThreshold(quic_trial_params);
    if (packet_loss_threshold != 0)
      globals->quic_packet_loss_threshold.set(packet_loss_threshold);
    globals->enable_quic_port_selection.set(
        ShouldEnableQuicPortSelection(command_line));
    globals->quic_connection_options =
        GetQuicConnectionOptions(command_line, quic_trial_params);
    globals->quic_close_sessions_on_ip_change.set(
        ShouldQuicCloseSessionsOnIpChange(quic_trial_params));
    int idle_connection_timeout_seconds = GetQuicIdleConnectionTimeoutSeconds(
        quic_trial_params);
    if (idle_connection_timeout_seconds != 0) {
      globals->quic_idle_connection_timeout_seconds.set(
          idle_connection_timeout_seconds);
    }
    globals->quic_disable_preconnect_if_0rtt.set(
        ShouldQuicDisablePreConnectIfZeroRtt(quic_trial_params));
    globals->quic_host_whitelist =
        GetQuicHostWhitelist(command_line, quic_trial_params);
    globals->quic_migrate_sessions_on_network_change.set(
        ShouldQuicMigrateSessionsOnNetworkChange(quic_trial_params));
  }

  size_t max_packet_length = GetQuicMaxPacketLength(command_line,
                                                    quic_trial_params);
  if (max_packet_length != 0) {
    globals->quic_max_packet_length.set(max_packet_length);
  }

  std::string quic_user_agent_id = chrome::GetChannelString();
  if (!quic_user_agent_id.empty())
    quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(
      version_info::GetProductNameAndVersionForUserAgent());
  quic_user_agent_id.push_back(' ');
  quic_user_agent_id.append(content::BuildOSCpuInfo());
  globals->quic_user_agent_id.set(quic_user_agent_id);

  net::QuicVersion version = GetQuicVersion(command_line, quic_trial_params);
  if (version != net::QUIC_VERSION_UNSUPPORTED) {
    net::QuicVersionVector supported_versions;
    supported_versions.push_back(version);
    globals->quic_supported_versions.set(supported_versions);
  }

  double threshold = GetAlternativeProtocolProbabilityThreshold(
      command_line, quic_trial_params);
  if (threshold >=0 && threshold <= 1) {
    globals->alternative_service_probability_threshold.set(threshold);
    globals->http_server_properties->SetAlternativeServiceProbabilityThreshold(
        threshold);
  }

  if (command_line.HasSwitch(switches::kOriginToForceQuicOn)) {
    net::HostPortPair quic_origin =
        net::HostPortPair::FromString(
            command_line.GetSwitchValueASCII(switches::kOriginToForceQuicOn));
    if (!quic_origin.IsEmpty()) {
      globals->origin_to_force_quic_on.set(quic_origin);
    }
  }
}

bool IOThread::ShouldEnableQuic(const base::CommandLine& command_line,
                                base::StringPiece quic_trial_group,
                                bool quic_allowed_by_policy) {
  if (command_line.HasSwitch(switches::kDisableQuic) || !quic_allowed_by_policy)
    return false;

  if (command_line.HasSwitch(switches::kEnableQuic))
    return true;

  return quic_trial_group.starts_with(kQuicFieldTrialEnabledGroupName) ||
      quic_trial_group.starts_with(kQuicFieldTrialHttpsEnabledGroupName);
}

bool IOThread::ShouldEnableQuicForProxies(const base::CommandLine& command_line,
                                          base::StringPiece quic_trial_group,
                                          bool quic_allowed_by_policy) {
  return ShouldEnableQuic(
      command_line, quic_trial_group, quic_allowed_by_policy) ||
      ShouldEnableQuicForDataReductionProxy();
}

bool IOThread::ShouldEnableQuicForDataReductionProxy() {
  const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDisableQuic))
    return false;

  return data_reduction_proxy::params::IsIncludedInQuicFieldTrial();
}

bool IOThread::ShouldEnableQuicPortSelection(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableQuicPortSelection))
    return false;

  if (command_line.HasSwitch(switches::kEnableQuicPortSelection))
    return true;

  return false;  // Default to disabling port selection on all channels.
}

net::QuicTagVector IOThread::GetQuicConnectionOptions(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicConnectionOptions)) {
    return net::QuicUtils::ParseQuicConnectionOptions(
        command_line.GetSwitchValueASCII(switches::kQuicConnectionOptions));
  }

  VariationParameters::const_iterator it =
      quic_trial_params.find("connection_options");
  if (it == quic_trial_params.end()) {
    return net::QuicTagVector();
  }

  return net::QuicUtils::ParseQuicConnectionOptions(it->second);
}

double IOThread::GetAlternativeProtocolProbabilityThreshold(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  double value;
  if (command_line.HasSwitch(
          switches::kAlternativeServiceProbabilityThreshold)) {
    if (base::StringToDouble(
            command_line.GetSwitchValueASCII(
                switches::kAlternativeServiceProbabilityThreshold),
            &value)) {
      return value;
    }
  }
  if (command_line.HasSwitch(switches::kEnableQuic)) {
    return 0;
  }
  // TODO(bnc): Remove when new parameter name rolls out and server
  // configuration is changed.
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params,
                            "alternate_protocol_probability_threshold"),
          &value)) {
    return value;
  }
  if (base::StringToDouble(
          GetVariationParam(quic_trial_params,
                            "alternative_service_probability_threshold"),
          &value)) {
    return value;
  }
  return -1;
}

bool IOThread::ShouldQuicAlwaysRequireHandshakeConfirmation(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "always_require_handshake_confirmation"),
      "true");
}

bool IOThread::ShouldQuicDisableConnectionPooling(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_connection_pooling"),
      "true");
}

float IOThread::GetQuicLoadServerInfoTimeoutSrttMultiplier(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(GetVariationParam(quic_trial_params,
                                             "load_server_info_time_to_srtt"),
                           &value)) {
    return static_cast<float>(value);
  }
  return 0.0f;
}

bool IOThread::ShouldQuicEnableConnectionRacing(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_connection_racing"),
      "true");
}

bool IOThread::ShouldQuicEnableNonBlockingIO(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "enable_non_blocking_io"),
      "true");
}

bool IOThread::ShouldQuicDisableDiskCache(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_disk_cache"), "true");
}

bool IOThread::ShouldQuicPreferAes(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "prefer_aes"), "true");
}

bool IOThread::ShouldQuicEnableAlternativeServices(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  return command_line.HasSwitch(switches::kEnableAlternativeServices) ||
         base::LowerCaseEqualsASCII(
             GetVariationParam(quic_trial_params, "use_alternative_services"),
             "true");
}

int IOThread::GetQuicMaxNumberOfLossyConnections(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "max_number_of_lossy_connections"),
                        &value)) {
    return value;
  }
  return 0;
}

float IOThread::GetQuicPacketLossThreshold(
    const VariationParameters& quic_trial_params) {
  double value;
  if (base::StringToDouble(GetVariationParam(quic_trial_params,
                                             "packet_loss_threshold"),
                           &value)) {
    return static_cast<float>(value);
  }
  return 0.0f;
}

int IOThread::GetQuicSocketReceiveBufferSize(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "receive_buffer_size"),
                        &value)) {
    return value;
  }
  return 0;
}

bool IOThread::ShouldQuicDelayTcpRace(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "delay_tcp_race"), "true");
}

bool IOThread::ShouldQuicCloseSessionsOnIpChange(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "close_sessions_on_ip_change"),
      "true");
}

int IOThread::GetQuicIdleConnectionTimeoutSeconds(
    const VariationParameters& quic_trial_params) {
  int value;
  if (base::StringToInt(GetVariationParam(quic_trial_params,
                                          "idle_connection_timeout_seconds"),
                        &value)) {
    return value;
  }
  return 0;
}

bool IOThread::ShouldQuicDisablePreConnectIfZeroRtt(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params, "disable_preconnect_if_0rtt"),
      "true");
}

std::unordered_set<std::string> IOThread::GetQuicHostWhitelist(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  std::string whitelist;
  if (command_line.HasSwitch(switches::kQuicHostWhitelist)) {
    whitelist = command_line.GetSwitchValueASCII(switches::kQuicHostWhitelist);
  } else {
    whitelist = GetVariationParam(quic_trial_params, "quic_host_whitelist");
  }
  std::unordered_set<std::string> hosts;
  for (const std::string& host :base::SplitString(whitelist, ",",
                                                  base::TRIM_WHITESPACE,
                                                  base::SPLIT_WANT_ALL)) {
    hosts.insert(host);
  }
  return hosts;
}

bool IOThread::ShouldQuicMigrateSessionsOnNetworkChange(
    const VariationParameters& quic_trial_params) {
  return base::LowerCaseEqualsASCII(
      GetVariationParam(quic_trial_params,
                        "migrate_sessions_on_network_change"),
      "true");
}

size_t IOThread::GetQuicMaxPacketLength(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicMaxPacketLength)) {
    unsigned value;
    if (!base::StringToUint(
            command_line.GetSwitchValueASCII(switches::kQuicMaxPacketLength),
            &value)) {
      return 0;
    }
    return value;
  }

  unsigned value;
  if (base::StringToUint(GetVariationParam(quic_trial_params,
                                           "max_packet_length"),
                         &value)) {
    return value;
  }
  return 0;
}

net::QuicVersion IOThread::GetQuicVersion(
    const base::CommandLine& command_line,
    const VariationParameters& quic_trial_params) {
  if (command_line.HasSwitch(switches::kQuicVersion)) {
    return ParseQuicVersion(
        command_line.GetSwitchValueASCII(switches::kQuicVersion));
  }

  return ParseQuicVersion(GetVariationParam(quic_trial_params, "quic_version"));
}

net::QuicVersion IOThread::ParseQuicVersion(const std::string& quic_version) {
  net::QuicVersionVector supported_versions = net::QuicSupportedVersions();
  for (size_t i = 0; i < supported_versions.size(); ++i) {
    net::QuicVersion version = supported_versions[i];
    if (net::QuicVersionToString(version) == quic_version) {
      return version;
    }
  }

  return net::QUIC_VERSION_UNSUPPORTED;
}

net::URLRequestContext* IOThread::ConstructSystemRequestContext(
    IOThread::Globals* globals,
    net::NetLog* net_log) {
  net::URLRequestContext* context = new SystemURLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_ssl_config_service(globals->ssl_config_service.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->system_proxy_service.get());

  globals->system_url_request_job_factory.reset(
      new net::URLRequestJobFactoryImpl());
  context->set_job_factory(globals->system_url_request_job_factory.get());

  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  context->set_network_quality_estimator(
      globals->network_quality_estimator.get());
  context->set_backoff_manager(globals->url_request_backoff_manager.get());

  context->set_http_server_properties(
      globals->http_server_properties->GetWeakPtr());

  net::HttpNetworkSession::Params system_params;
  InitializeNetworkSessionParamsFromGlobals(*globals, &system_params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &system_params);

  globals->system_http_network_session.reset(
      new net::HttpNetworkSession(system_params));
  globals->system_http_transaction_factory.reset(
      new net::HttpNetworkLayer(globals->system_http_network_session.get()));
  context->set_http_transaction_factory(
      globals->system_http_transaction_factory.get());

  return context;
}

net::URLRequestContext* IOThread::ConstructProxyScriptFetcherContext(
    IOThread::Globals* globals,
    net::NetLog* net_log) {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::ConstructProxyScriptFetcherContext1"));
  net::URLRequestContext* context = new net::URLRequestContext;
  context->set_net_log(net_log);
  context->set_host_resolver(globals->host_resolver.get());
  context->set_cert_verifier(globals->cert_verifier.get());
  context->set_transport_security_state(
      globals->transport_security_state.get());
  context->set_cert_transparency_verifier(
      globals->cert_transparency_verifier.get());
  context->set_ssl_config_service(globals->ssl_config_service.get());
  context->set_http_auth_handler_factory(
      globals->http_auth_handler_factory.get());
  context->set_proxy_service(globals->proxy_script_fetcher_proxy_service.get());

  context->set_job_factory(
      globals->proxy_script_fetcher_url_request_job_factory.get());

  context->set_cookie_store(globals->system_cookie_store.get());
  context->set_channel_id_service(
      globals->system_channel_id_service.get());
  context->set_network_delegate(globals->system_network_delegate.get());
  context->set_http_user_agent_settings(
      globals->http_user_agent_settings.get());
  context->set_http_server_properties(
      globals->http_server_properties->GetWeakPtr());

  net::HttpNetworkSession::Params session_params;
  InitializeNetworkSessionParamsFromGlobals(*globals, &session_params);
  net::URLRequestContextBuilder::SetHttpNetworkSessionComponents(
      context, &session_params);

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::ConstructProxyScriptFetcherContext2"));
  globals->proxy_script_fetcher_http_network_session.reset(
      new net::HttpNetworkSession(session_params));
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/466432
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "466432 IOThread::ConstructProxyScriptFetcherContext3"));
  globals->proxy_script_fetcher_http_transaction_factory.reset(
      new net::HttpNetworkLayer(
          globals->proxy_script_fetcher_http_network_session.get()));
  context->set_http_transaction_factory(
      globals->proxy_script_fetcher_http_transaction_factory.get());

  scoped_ptr<net::URLRequestJobFactoryImpl> job_factory(
      new net::URLRequestJobFactoryImpl());

  job_factory->SetProtocolHandler(
      url::kDataScheme, make_scoped_ptr(new net::DataProtocolHandler()));
  job_factory->SetProtocolHandler(
      url::kFileScheme,
      make_scoped_ptr(new net::FileProtocolHandler(
          content::BrowserThread::GetBlockingPool()
              ->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN))));
#if !defined(DISABLE_FTP_SUPPORT)
  globals->proxy_script_fetcher_ftp_transaction_factory.reset(
      new net::FtpNetworkLayer(globals->host_resolver.get()));
  job_factory->SetProtocolHandler(
      url::kFtpScheme,
      make_scoped_ptr(new net::FtpProtocolHandler(
          globals->proxy_script_fetcher_ftp_transaction_factory.get())));
#endif
  globals->proxy_script_fetcher_url_request_job_factory =
      std::move(job_factory);

  context->set_job_factory(
      globals->proxy_script_fetcher_url_request_job_factory.get());

  // TODO(rtenneti): We should probably use HttpServerPropertiesManager for the
  // system URLRequestContext too. There's no reason this should be tied to a
  // profile.
  return context;
}
