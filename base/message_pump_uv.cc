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

#include "third_party/libuv/include/uv.h"
#include "third_party/node/src/node.h"

namespace base {

static void async_cb(uv_async_t* handle, int status) {
}

MessagePumpUV::MessagePumpUV()
    : keep_running_(true),
      uv_loop_(uv_default_loop()),
      wake_up_event_(new uv_async_t),
      render_view_(NULL),
      render_view_observer_cb_(NULL)
{
  uv_async_init(uv_loop_, wake_up_event_, async_cb);
}

MessagePumpUV::~MessagePumpUV()
{
  delete wake_up_event_;
}

void MessagePumpUV::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";


  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringType node_args;
  node_args = command_line.GetSwitchValueNative("node-args"); //FIXME

  int argc = 2;
  char argv0[] = "node";
  char* argv[] = {argv0, (char*)node_args.c_str(), NULL};

  uv_timer_t timer0;
  uv_timer_init(uv_loop_, &timer0);

  node::Start(argc, argv);

  /*
  node::Start0(argc, argv);
  {
    v8::Locker locker;
    v8::HandleScope handle_scope;
    node::Start(argc, argv);

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
        uv_run_once(uv_loop_);
      } else {
        TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
        if (delay > TimeDelta()) {
          uv_timer_start(&timer0, (uv_timer_cb) &timer_cb,
                         delay.InMilliseconds(), 0);
          uv_run_once(uv_loop_);
        } else {
          // It looks like delayed_work_time_ indicates a time in the past, so we
          // need to call DoDelayedWork now.
          delayed_work_time_ = TimeTicks();
        }
      }
      // Since event_ is auto-reset, we don't need to do anything special here
      // other than service each delegate method.
    }
  }
  keep_running_ = true;
  */
}

void MessagePumpUV::Quit() {
  keep_running_ = false;
}

void MessagePumpUV::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
  uv_async_send(wake_up_event_);
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
