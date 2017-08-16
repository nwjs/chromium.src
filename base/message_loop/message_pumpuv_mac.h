// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The basis for all native run loops on the Mac is the CFRunLoop.  It can be
// used directly, it can be used as the driving force behind the similar
// Foundation NSRunLoop, and it can be used to implement higher-level event
// loops such as the NSApplication event loop.
//
// This file introduces a basic CFRunLoop-based implementation of the
// MessagePump interface called CFRunLoopBase.  CFRunLoopBase contains all
// of the machinery necessary to dispatch events to a delegate, but does not
// implement the specific run loop.  Concrete subclasses must provide their
// own DoRun and Quit implementations.
//
// A concrete subclass that just runs a CFRunLoop loop is provided in
// MessagePumpCFRunLoop.  For an NSRunLoop, the similar MessagePumpNSRunLoop
// is provided.
//
// For the application's event loop, an implementation based on AppKit's
// NSApplication event system is provided in MessagePumpNSApplication.
//
// Typically, MessagePumpNSApplication only makes sense on a Cocoa
// application's main thread.  If a CFRunLoop-based message pump is needed on
// any other thread, one of the other concrete subclasses is preferrable.
// MessagePumpMac::Create is defined, which returns a new NSApplication-based
// or NSRunLoop-based MessagePump subclass depending on which thread it is
// called on.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMPUV_MAC_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMPUV_MAC_H_

#include "base/message_loop/message_pump.h"

#include <CoreFoundation/CoreFoundation.h>
#include "v8.h"
#include "third_party/node-nw/src/node_webkit.h"

#include "base/memory/weak_ptr.h"
#include "base/message_loop/timer_slack.h"

#include "base/message_loop/message_pump_mac.h"

#if defined(__OBJC__)
#if defined(OS_IOS)
#import <Foundation/Foundation.h>
#else
#import <AppKit/AppKit.h>

#endif  // !defined(OS_IOS)
#endif  // defined(__OBJC__)

namespace base {

class RunLoop;
class TimeTicks;

class BASE_EXPORT MessagePumpUVNSRunLoop : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpUVNSRunLoop();
  ~MessagePumpUVNSRunLoop() override;

  void DoRun(Delegate* delegate) override;
  void Quit() override;

 protected:
  bool RunWork() override;
  bool RunIdleWork() override;
  void PreWaitObserverHook() override;

 private:
  // A source that doesn't do anything but provide something signalable
  // attached to the run loop.  This source will be signalled when Quit
  // is called, to cause the loop to wake up so that it can stop.
  CFRunLoopSourceRef quit_source_;

  // Thread to poll uv events.
  static void EmbedThreadRunner(void *arg);

  // False after Quit is called.
  bool keep_running_;
  // Flag to pause the libuv loop.
  bool pause_uv_;

  msg_pump_context_t ctx_;
  // Whether we're done.
  int embed_closed_;
  int nw_nesting_level_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpUVNSRunLoop);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMPUV_MAC_H_
