// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_uv.h"

#include "base/logging.h"
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "v8/include/v8.h"
#include "third_party/node/src/node.h"
#undef CHECK
#undef CHECK_EQ
#undef CHECK_GE
#undef CHECK_GT
#undef CHECK_NE
#undef CHECK_LT
#undef CHECK_LE
#undef CHECK_OP
#undef DISALLOW_COPY_AND_ASSIGN
#include "third_party/node/src/node_webkit.h"

VoidHookFn g_msg_pump_ctor_fn = nullptr;
VoidHookFn g_msg_pump_dtor_fn = nullptr;
VoidHookFn g_msg_pump_sched_work_fn = nullptr, g_msg_pump_nest_leave_fn = nullptr, g_msg_pump_need_work_fn = nullptr;
VoidHookFn g_msg_pump_did_work_fn = nullptr, g_msg_pump_pre_loop_fn = nullptr, g_msg_pump_nest_enter_fn = nullptr;
VoidIntHookFn g_msg_pump_delay_work_fn = nullptr;
VoidHookFn g_msg_pump_clean_ctx_fn = nullptr;
GetPointerFn g_uv_default_loop_fn = nullptr;

namespace base {

MessagePumpUV::MessagePumpUV()
    : keep_running_(true),
      nesting_level_(0) {
  // wakeup_event_ = new uv_async_t;
  // uv_async_init(uv_default_loop(), wakeup_event_, wakeup_callback);
  // node::g_nw_uv_run = uv_run;
  g_msg_pump_ctor_fn(&wakeup_event_);
}

MessagePumpUV::~MessagePumpUV() {
  // delete wakeup_event_;
  // wakeup_event_ = NULL;
  g_msg_pump_dtor_fn(&wakeup_event_);
}

void MessagePumpUV::Run(Delegate* delegate) {

  ++nesting_level_;
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  msg_pump_context_t ctx;
  memset(&ctx, 0, sizeof(msg_pump_context_t));

  // Poll external loop in nested message loop, so node.js's events will be
  // paused in nested loop.
  // uv_loop_t* loop = uv_default_loop();
  ctx.loop = g_uv_default_loop_fn();
  ctx.wakeup_event = wakeup_event_;
  ctx.wakeup_events = &wakeup_events_;

  if (nesting_level_ > 1) {
    g_msg_pump_nest_enter_fn(&ctx);
    wakeup_event_ = ctx.wakeup_event;
    // loop = uv_loop_new();

    // wakeup_events_.push_back(wakeup_event_);
    // wakeup_event_ = new uv_async_t;
    // uv_async_init(loop, wakeup_event_, wakeup_callback);
  }

  // // Create handles for the loop.
  // uv_idle_t idle_handle;
  // uv_idle_init(loop, &idle_handle);

  // uv_timer_t delay_timer;
  // delay_timer.data = &idle_handle;
  // uv_timer_init(loop, &delay_timer);

  g_msg_pump_pre_loop_fn(&ctx);
  // Enter Loop
  for (;;) {
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work) {
      // // call tick callback after done work in V8,
      // // in the same way node upstream handle this in MakeCallBack,
      // // or the tick callback is blocked in some cases
      // if (node::g_env) {
      //   v8::Isolate* isolate = node::g_env->isolate();
      //   v8::HandleScope handleScope(isolate);
      //   (*node::g_nw_uv_run)(loop, UV_RUN_NOWAIT);
      //   node::CallNWTickCallback(node::g_env, v8::Undefined(isolate));
      // }
      g_msg_pump_did_work_fn(&ctx);
      continue;
    }

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work) {
      g_msg_pump_did_work_fn(&ctx);
      continue;
    }

    if (delayed_work_time_.is_null()) {
      // (*node::g_nw_uv_run)(loop, UV_RUN_ONCE);
      g_msg_pump_need_work_fn(&ctx);
    } else {
      TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
      if (delay > TimeDelta()) {
        // uv_timer_start(&delay_timer, timer_callback,
        //                delay.InMilliseconds(), 0);
        // (*node::g_nw_uv_run)(loop, UV_RUN_ONCE);
        // uv_idle_stop(&idle_handle);
        // uv_timer_stop(&delay_timer);
        g_msg_pump_delay_work_fn(&ctx, delay.InMilliseconds());
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
    // uv_close((uv_handle_t*)wakeup_event_, NULL);
    // // Delete external loop.
    // uv_loop_close(loop);
    // free(loop);

    // // Restore previous async handle.
    // delete wakeup_event_;
    // wakeup_event_ = wakeup_events_.back();
    // wakeup_events_.pop_back();
    g_msg_pump_nest_leave_fn(&ctx);
    wakeup_event_ = ctx.wakeup_event;
  }

  keep_running_ = true;
  --nesting_level_;
  g_msg_pump_clean_ctx_fn(&ctx);
}

void MessagePumpUV::Quit() {
  keep_running_ = false;
}

void MessagePumpUV::ScheduleWork() {
//   // Since this can be called on any thread, we need to ensure that our Run
//   // loop wakes up.
// #if defined(OS_WIN)
//   uv_async_send_nw(wakeup_event_);
// #else
//   uv_async_send(wakeup_event_);
// #endif
  g_msg_pump_sched_work_fn(wakeup_event_);
}

void MessagePumpUV::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base
