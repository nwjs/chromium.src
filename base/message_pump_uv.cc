// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_uv.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "third_party/node/src/node.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

static uv_async_t g_wakeup_event;
static uv_idle_t idle_handle;
static bool timer_cb_called;

static void wakeup_callback(uv_async_t* handle, int status) {
  // do nothing
}

static void idle_null_cb(uv_idle_t* handle, int status) {
}

static void timer_callback(uv_timer_t* timer, int status) {
  // libuv would block unexpectedly with zero-timeout timer
  // this is a workaround of libuv bug #574:
  //  https://github.com/joyent/libuv/issues/574
  timer_cb_called = true;
  uv_idle_start(&idle_handle, idle_null_cb);
}

}

namespace base {

MessagePumpUV::MessagePumpUV()
    : keep_running_(true)
{
  uv_async_init(uv_default_loop(), &g_wakeup_event, wakeup_callback);
}

MessagePumpUV::~MessagePumpUV()
{
}

void MessagePumpUV::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  uv_timer_t delay_timer;
  uv_timer_init(uv_default_loop(), &delay_timer);
  uv_idle_init(uv_default_loop(), &idle_handle);
  // Enter Loop

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
        uv_idle_stop(&idle_handle);
        uv_timer_stop(&delay_timer);
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
  uv_async_send(&g_wakeup_event);
}

void MessagePumpUV::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base
