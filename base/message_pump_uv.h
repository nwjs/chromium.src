// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_UV_H_
#define BASE_MESSAGE_PUMP_UV_H_
#pragma once

#include "base/message_pump.h"
#include "base/time.h"
#include "base/base_export.h"

struct uv_loop_s;
struct uv_async_s;

namespace base {

typedef void (*render_view_obs_cb_t)(void*);

class BASE_EXPORT MessagePumpUV : public MessagePump {
 public:
  MessagePumpUV();
  virtual ~MessagePumpUV();

  virtual void onRenderViewCreated(void* render_view);
  // MessagePump methods:
  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) OVERRIDE;

  void* render_view() { return render_view_; }
  void setCallback(render_view_obs_cb_t cb) { render_view_observer_cb_ = cb; }
 private:
  // This flag is set to false when Run should return.
  bool keep_running_;

  struct uv_loop_s* uv_loop_;

  struct uv_async_s* wake_up_event_;

  void* render_view_;
  render_view_obs_cb_t render_view_observer_cb_;
  // The time at which we should call DoDelayedWork.
  TimeTicks delayed_work_time_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpUV);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_UV_H_
