// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_UV_H_
#define BASE_MESSAGE_PUMP_UV_H_
#pragma once

#include "base/message_loop/message_pump.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

#include <vector>

//#include "third_party/node/deps/uv/include/uv.h"

typedef struct uv_async_s uv_async_t;
namespace base {

class CONTENT_EXPORT MessagePumpUV : public MessagePump {
 public:
  MessagePumpUV();

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

 private:
  ~MessagePumpUV() override;

  // This flag is set to false when Run should return.
  bool keep_running_;

  // Nested loop level.
  int nesting_level_;

  // Handle to wake up loop.
  std::vector<void*> wakeup_events_;
  void* wakeup_event_;

  TimeTicks delayed_work_time_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpUV);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_UV_H_
