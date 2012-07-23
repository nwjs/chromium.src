// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_uv.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#include "third_party/node/src/node.h"

namespace base {

MessagePumpUV::MessagePumpUV()
    : keep_running_(true),
      render_view_(NULL),
      render_view_observer_cb_(NULL)
{
}

MessagePumpUV::~MessagePumpUV()
{
}

static void wakeup_callback(uv_async_t* handle, int status) {
  // Do nothing
}

static void timer_callback(uv_timer_t* timer, int status) {
  uv_timer_stop(timer);
}

void MessagePumpUV::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

//  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
//  CommandLine::StringType node_args;
//  node_args = command_line.GetSwitchValueNative("node-args"); //FIXME

//  int argc = 2;
//  char argv0[] = "node";
//  char* argv[] = {argv0, (char*)node_args.c_str(), NULL};

  uv_timer_t delay_timer;
  uv_timer_init(uv_default_loop(), &delay_timer);
  uv_async_init(uv_default_loop(), &wakeup_event_, wakeup_callback);

  for (;;) {
#if defined(OS_MACOSX)
    mac::ScopedNSAutoreleasePool autorelease_pool;
#endif

    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      uv_run_once(uv_default_loop());
    } else {
      TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
      if (delay > TimeDelta()) {
        uv_timer_start(&delay_timer, (uv_timer_cb)timer_callback,
                       delay.InMilliseconds(), 0);
        uv_run_once(uv_default_loop());
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = TimeTicks();
      }
    }
    // Since event_ is auto-reset, we don't need to do anything special here
    // other than service each delegate method.
  }

  keep_running_ = true;
}

void MessagePumpUV::Quit() {
  keep_running_ = false;
}

void MessagePumpUV::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  uv_async_send(&wakeup_event_);
}

void MessagePumpUV::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

void MessagePumpUV::onRenderViewCreated(void* render_view) {
  render_view_ = render_view;
  if (render_view_observer_cb_)
    render_view_observer_cb_(render_view);
}

bool print_js_stacktrace(int frameLimit) {
  if (!v8::Context::InContext())
    return false;
  v8::Handle<v8::Context> context = v8::Context::GetCurrent();
  if (context.IsEmpty())
    return false;
  v8::HandleScope scope;
  v8::Context::Scope contextScope(context);
  v8::Handle<v8::StackTrace> trace(v8::StackTrace::CurrentStackTrace(frameLimit));
  int frameCount = trace->GetFrameCount();
  if (trace.IsEmpty() || !frameCount)
    return false;
  for (int i = 0; i < frameCount; ++i) {
    v8::Handle<v8::StackFrame> frame = trace->GetFrame(i);
    v8::String::Utf8Value scriptName(frame->GetScriptName());
    v8::String::Utf8Value functionName(frame->GetFunctionName());
    fprintf(stderr, "%s:%d - %s\n", *scriptName, frame->GetLineNumber(), *functionName);
  }
  return true;
}

}  // namespace base
