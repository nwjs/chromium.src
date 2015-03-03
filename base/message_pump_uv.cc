// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_uv.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "v8/include/v8.h"
#include "third_party/node/src/node.h"
#undef CHECK
#include "third_party/node/src/node_internals.h"

namespace base {

namespace {

void wakeup_callback(uv_async_t* handle) {
  // do nothing, just make libuv exit loop.
}

void idle_callback(uv_idle_t* handle) {
  // do nothing, just make libuv exit loop.
}

void timer_callback(uv_timer_t* timer) {
  // libuv would block unexpectedly with zero-timeout timer
  // this is a workaround of libuv bug #574:
  // https://github.com/joyent/libuv/issues/574
  uv_idle_start(static_cast<uv_idle_t*>(timer->data), idle_callback);
}

}  // namespace

MessagePumpUV::MessagePumpUV()
    : keep_running_(true),
      nesting_level_(0) {
  wakeup_event_ = new uv_async_t;
  uv_async_init(uv_default_loop(), wakeup_event_, wakeup_callback);
}

MessagePumpUV::~MessagePumpUV() {
  delete wakeup_event_;
  wakeup_event_ = NULL;
}

void MessagePumpUV::Run(Delegate* delegate) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  ++nesting_level_;
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  // Poll external loop in nested message loop, so node.js's events will be
  // paused in nested loop.
  uv_loop_t* loop = uv_default_loop();
  if (nesting_level_ > 1) {
    loop = uv_loop_new();

    wakeup_events_.push_back(wakeup_event_);
    wakeup_event_ = new uv_async_t;
    uv_async_init(loop, wakeup_event_, wakeup_callback);
  }

  // Create handles for the loop.
  uv_idle_t idle_handle;
  uv_idle_init(loop, &idle_handle);

  uv_timer_t delay_timer;
  delay_timer.data = &idle_handle;
  uv_timer_init(loop, &delay_timer);

  // Enter Loop
  for (;;) {
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work) {
      // call tick callback after done work in V8,
      // in the same way node upstream handle this in MakeCallBack,
      // or the tick callback is blocked in some cases
      if (node::g_env) {
        v8::HandleScope handleScope(isolate);
        uv_run(loop, UV_RUN_NOWAIT);
        node::CallTickCallback(node::g_env, v8::Undefined(isolate));
      }
      continue;
    }

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work) {
      if (node::g_env) {
        v8::HandleScope handleScope(isolate);
        uv_run(loop, UV_RUN_NOWAIT);
        node::CallTickCallback(node::g_env, v8::Undefined(isolate));
      }
      continue;
    }

    if (delayed_work_time_.is_null()) {
      uv_run(loop, UV_RUN_ONCE);
    } else {
      TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
      if (delay > TimeDelta()) {
        uv_timer_start(&delay_timer, timer_callback,
                       delay.InMilliseconds(), 0);
        uv_run(loop, UV_RUN_ONCE);
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

  if (nesting_level_ > 1) {
    uv_close((uv_handle_t*)wakeup_event_, NULL);
    // Delete external loop.
    uv_loop_close(loop);
    free(loop);

    // Restore previous async handle.
    delete wakeup_event_;
    wakeup_event_ = wakeup_events_.back();
    wakeup_events_.pop_back();
  }

  keep_running_ = true;
  --nesting_level_;
}

void MessagePumpUV::Quit() {
  keep_running_ = false;
}

void MessagePumpUV::ScheduleWork() {
  // Since this can be called on any thread, we need to ensure that our Run
  // loop wakes up.
#if defined(OS_WIN)
  uv_async_send_nw(wakeup_event_);
#else
  uv_async_send(wakeup_event_);
#endif
}

void MessagePumpUV::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base
