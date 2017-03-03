// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/history/history_api.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/history.h"
#include "chrome/common/pref_names.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace extensions {

using api::history::HistoryItem;
using api::history::VisitItem;

typedef std::vector<api::history::HistoryItem> HistoryItemList;
typedef std::vector<api::history::VisitItem> VisitItemList;

namespace AddUrl = api::history::AddUrl;
namespace DeleteUrl = api::history::DeleteUrl;
namespace DeleteRange = api::history::DeleteRange;
namespace GetVisits = api::history::GetVisits;
namespace OnVisited = api::history::OnVisited;
namespace OnVisitRemoved = api::history::OnVisitRemoved;
namespace Search = api::history::Search;

namespace {

const char kInvalidUrlError[] = "Url is invalid.";
const char kDeleteProhibitedError[] = "Browsing history is not allowed to be "
                                      "deleted.";

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

HistoryItem GetHistoryItem(const history::URLRow& row) {
  HistoryItem history_item;

  history_item.id = base::Int64ToString(row.id());
  history_item.url.reset(new std::string(row.url().spec()));
  history_item.title.reset(new std::string(base::UTF16ToUTF8(row.title())));
  history_item.last_visit_time.reset(
      new double(MilliSecondsFromTime(row.last_visit())));
  history_item.typed_count.reset(new int(row.typed_count()));
  history_item.visit_count.reset(new int(row.visit_count()));

  return history_item;
}

VisitItem GetVisitItem(const history::VisitRow& row) {
  VisitItem visit_item;

  visit_item.id = base::Int64ToString(row.url_id);
  visit_item.visit_id = base::Int64ToString(row.visit_id);
  visit_item.visit_time.reset(new double(MilliSecondsFromTime(row.visit_time)));
  visit_item.referring_visit_id = base::Int64ToString(row.referring_visit);

  api::history::TransitionType transition = api::history::TRANSITION_TYPE_LINK;
  switch (row.transition & ui::PAGE_TRANSITION_CORE_MASK) {
    case ui::PAGE_TRANSITION_LINK:
      transition = api::history::TRANSITION_TYPE_LINK;
      break;
    case ui::PAGE_TRANSITION_TYPED:
      transition = api::history::TRANSITION_TYPE_TYPED;
      break;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      transition = api::history::TRANSITION_TYPE_AUTO_BOOKMARK;
      break;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      transition = api::history::TRANSITION_TYPE_AUTO_SUBFRAME;
      break;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      transition = api::history::TRANSITION_TYPE_MANUAL_SUBFRAME;
      break;
    case ui::PAGE_TRANSITION_GENERATED:
      transition = api::history::TRANSITION_TYPE_GENERATED;
      break;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      transition = api::history::TRANSITION_TYPE_AUTO_TOPLEVEL;
      break;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      transition = api::history::TRANSITION_TYPE_FORM_SUBMIT;
      break;
    case ui::PAGE_TRANSITION_RELOAD:
      transition = api::history::TRANSITION_TYPE_RELOAD;
      break;
    case ui::PAGE_TRANSITION_KEYWORD:
      transition = api::history::TRANSITION_TYPE_KEYWORD;
      break;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      transition = api::history::TRANSITION_TYPE_KEYWORD_GENERATED;
      break;
    default:
      DCHECK(false);
  }

  visit_item.transition = transition;

  return visit_item;
}

}  // namespace

HistoryEventRouter::HistoryEventRouter(Profile* profile,
                                       history::HistoryService* history_service)
    : profile_(profile), history_service_observer_(this) {
  DCHECK(profile);
  history_service_observer_.Add(history_service);
}

HistoryEventRouter::~HistoryEventRouter() {
}

void HistoryEventRouter::OnURLVisited(history::HistoryService* history_service,
                                      ui::PageTransition transition,
                                      const history::URLRow& row,
                                      const history::RedirectList& redirects,
                                      base::Time visit_time) {
  std::unique_ptr<base::ListValue> args =
      OnVisited::Create(GetHistoryItem(row));
  DispatchEvent(profile_, events::HISTORY_ON_VISITED,
                api::history::OnVisited::kEventName, std::move(args));
}

void HistoryEventRouter::OnURLsDeleted(history::HistoryService* history_service,
                                       bool all_history,
                                       bool expired,
                                       const history::URLRows& deleted_rows,
                                       const std::set<GURL>& favicon_urls) {
  OnVisitRemoved::Removed removed;
  removed.all_history = all_history;

  std::vector<std::string>* urls = new std::vector<std::string>();
  for (const auto& row : deleted_rows)
    urls->push_back(row.url().spec());
  removed.urls.reset(urls);

  std::unique_ptr<base::ListValue> args = OnVisitRemoved::Create(removed);
  DispatchEvent(profile_, events::HISTORY_ON_VISIT_REMOVED,
                api::history::OnVisitRemoved::kEventName, std::move(args));
}

void HistoryEventRouter::DispatchEvent(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  if (profile && EventRouter::Get(profile)) {
    std::unique_ptr<Event> event(
        new Event(histogram_value, event_name, std::move(event_args)));
    event->restrict_to_browser_context = profile;
    EventRouter::Get(profile)->BroadcastEvent(std::move(event));
  }
}

HistoryAPI::HistoryAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, api::history::OnVisited::kEventName);
  event_router->RegisterObserver(this,
                                 api::history::OnVisitRemoved::kEventName);
}

HistoryAPI::~HistoryAPI() {
}

void HistoryAPI::Shutdown() {
  history_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<HistoryAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<HistoryAPI>* HistoryAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<HistoryAPI>::DeclareFactoryDependencies() {
  DependsOn(ActivityLog::GetFactoryInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

void HistoryAPI::OnListenerAdded(const EventListenerInfo& details) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  history_event_router_.reset(new HistoryEventRouter(
      profile, HistoryServiceFactory::GetForProfile(
                   profile, ServiceAccessType::EXPLICIT_ACCESS)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

bool HistoryFunction::ValidateUrl(const std::string& url_string, GURL* url) {
  GURL temp_url(url_string);
  if (!temp_url.is_valid()) {
    error_ = kInvalidUrlError;
    return false;
  }
  url->Swap(&temp_url);
  return true;
}

bool HistoryFunction::VerifyDeleteAllowed() {
  PrefService* prefs = GetProfile()->GetPrefs();
  if (!prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory)) {
    error_ = kDeleteProhibitedError;
    return false;
  }
  return true;
}

base::Time HistoryFunction::GetTime(double ms_from_epoch) {
  // The history service has seconds resolution, while javascript Date() has
  // milliseconds resolution.
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  // Time::FromDoubleT converts double time 0 to empty Time object. So we need
  // to do special handling here.
  return (seconds_from_epoch == 0) ?
      base::Time::UnixEpoch() : base::Time::FromDoubleT(seconds_from_epoch);
}

HistoryFunctionWithCallback::HistoryFunctionWithCallback() {
}

HistoryFunctionWithCallback::~HistoryFunctionWithCallback() {
}

bool HistoryFunctionWithCallback::RunAsync() {
  AddRef();  // Balanced in SendAysncRepose() and below.
  bool retval = RunAsyncImpl();
  if (false == retval)
    Release();
  return retval;
}

void HistoryFunctionWithCallback::SendAsyncResponse() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&HistoryFunctionWithCallback::SendResponseToCallback, this));
}

void HistoryFunctionWithCallback::SendResponseToCallback() {
  SendResponse(true);
  Release();  // Balanced in RunAsync().
}

bool HistoryGetVisitsFunction::RunAsyncImpl() {
  std::unique_ptr<GetVisits::Params> params(GetVisits::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryURL(url,
               true,  // Retrieve full history of a URL.
               base::Bind(&HistoryGetVisitsFunction::QueryComplete,
                          base::Unretained(this)),
               &task_tracker_);
  return true;
}

void HistoryGetVisitsFunction::QueryComplete(
    bool success,
    const history::URLRow& url_row,
    const history::VisitVector& visits) {
  VisitItemList visit_item_vec;
  if (success && !visits.empty()) {
    for (const history::VisitRow& visit : visits)
      visit_item_vec.push_back(GetVisitItem(visit));
  }

  results_ = GetVisits::Results::Create(visit_item_vec);
  SendAsyncResponse();
}

bool HistorySearchFunction::RunAsyncImpl() {
  std::unique_ptr<Search::Params> params(Search::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::string16 search_text = base::UTF8ToUTF16(params->query.text);

  history::QueryOptions options;
  options.SetRecentDayRange(1);
  options.max_count = 100;

  if (params->query.start_time.get())
    options.begin_time = GetTime(*params->query.start_time);
  if (params->query.end_time.get())
    options.end_time = GetTime(*params->query.end_time);
  if (params->query.max_results.get())
    options.max_count = *params->query.max_results;

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text,
                   options,
                   base::Bind(&HistorySearchFunction::SearchComplete,
                              base::Unretained(this)),
                   &task_tracker_);

  return true;
}

void HistorySearchFunction::SearchComplete(history::QueryResults* results) {
  HistoryItemList history_item_vec;
  if (results && !results->empty()) {
    for (const history::URLResult* item : *results)
      history_item_vec.push_back(GetHistoryItem(*item));
  }
  results_ = Search::Results::Create(history_item_vec);
  SendAsyncResponse();
}

bool HistoryAddUrlFunction::RunAsync() {
  std::unique_ptr<AddUrl::Params> params(AddUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  base::string16 title;
  if (params->details.title.get())
    title = base::UTF8ToUTF16(*params->details.title);

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->AddPage(url, base::Time::Now(), history::SOURCE_EXTENSION);

  if (!title.empty())
    hs->SetPageTitle(url, title);

  SendResponse(true);
  return true;
}

bool HistoryDeleteUrlFunction::RunAsync() {
  std::unique_ptr<DeleteUrl::Params> params(DeleteUrl::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!VerifyDeleteAllowed())
    return false;

  GURL url;
  if (!ValidateUrl(params->details.url, &url))
    return false;

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->DeleteURL(url);

  // Also clean out from the activity log. If the activity log testing flag is
  // set then don't clean so testers can see what potentially malicious
  // extensions have been trying to clean from their logs.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogTesting)) {
    ActivityLog* activity_log = ActivityLog::GetInstance(GetProfile());
    DCHECK(activity_log);
    activity_log->RemoveURL(url);
  }

  SendResponse(true);
  return true;
}

bool HistoryDeleteRangeFunction::RunAsyncImpl() {
  std::unique_ptr<DeleteRange::Params> params(
      DeleteRange::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (!VerifyDeleteAllowed())
    return false;

  base::Time start_time = GetTime(params->range.start_time);
  base::Time end_time = GetTime(params->range.end_time);

  std::set<GURL> restrict_urls;
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      start_time,
      end_time,
      base::Bind(&HistoryDeleteRangeFunction::DeleteComplete,
                 base::Unretained(this)),
      &task_tracker_);

  // Also clean from the activity log unless in testing mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogTesting)) {
    ActivityLog* activity_log = ActivityLog::GetInstance(GetProfile());
    DCHECK(activity_log);
    activity_log->RemoveURLs(restrict_urls);
  }

  return true;
}

void HistoryDeleteRangeFunction::DeleteComplete() {
  SendAsyncResponse();
}

bool HistoryDeleteAllFunction::RunAsyncImpl() {
  if (!VerifyDeleteAllowed())
    return false;

  std::set<GURL> restrict_urls;
  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      GetProfile(), ServiceAccessType::EXPLICIT_ACCESS);
  hs->ExpireHistoryBetween(
      restrict_urls,
      base::Time(),      // Unbounded beginning...
      base::Time(),      // ...and the end.
      base::Bind(&HistoryDeleteAllFunction::DeleteComplete,
                 base::Unretained(this)),
      &task_tracker_);

  // Also clean from the activity log unless in testing mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogTesting)) {
    ActivityLog* activity_log = ActivityLog::GetInstance(GetProfile());
    DCHECK(activity_log);
    activity_log->RemoveURLs(restrict_urls);
  }

  return true;
}

void HistoryDeleteAllFunction::DeleteComplete() {
  SendAsyncResponse();
}

}  // namespace extensions
