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

namespace base {

namespace {

void wakeup_callback(uv_async_t* handle, int status) {
  // do nothing, just make libuv exit loop.
}

void idle_callback(uv_idle_t* handle, int status) {
  // do nothing, just make libuv exit loop.
}

void timer_callback(uv_timer_t* timer, int status) {
  // libuv would block unexpectedly with zero-timeout timer
  // this is a workaround of libuv bug #574:
  // https://github.com/joyent/libuv/issues/574
  static_cast<MessagePumpUV*>(timer->data)->WakeupInSameThread();
}

}  // namespace

MessagePumpUV::MessagePumpUV()
    : keep_running_(true)
{
  wakeup_event_.data = this;
  uv_async_init(uv_default_loop(), &wakeup_event_, wakeup_callback);

  idle_handle_.data = this;
  uv_idle_init(uv_default_loop(), &idle_handle_);

  delay_timer_.data = this;
  uv_timer_init(uv_default_loop(), &delay_timer_);
}

MessagePumpUV::~MessagePumpUV()
{
}

void MessagePumpUV::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

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
        uv_timer_start(&delay_timer_, timer_callback,
                       delay.InMilliseconds(), 0);
        uv_run_once(uv_default_loop());
        uv_idle_stop(&idle_handle_);
        uv_timer_stop(&delay_timer_);
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

void MessagePumpUV::WakeupInSameThread() {
  // Calling idle_start is cheaper than calling async_send.
  uv_idle_start(&idle_handle_, idle_callback);
}

}  // namespace base
