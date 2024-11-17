// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright Plask, (c) Dean McNamee <dean@gmail.com>, 2011.  BSD license


#import "base/message_loop/message_pump_mac.h"
#import "base/message_loop/message_pumpuv_mac.h"

#include <dlfcn.h>
#import <Foundation/Foundation.h>

#include <stdio.h>

#include <limits>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop/timer_slack.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "v8/include/v8.h"

#if !defined(OS_IOS)
#import <AppKit/AppKit.h>
#endif  // !defined(OS_IOS)

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

#include <vector>
#include "third_party/node-nw/src/node_webkit.h"
#include "content/nw/src/nw_content.h"

#define EVENTLOOP_DEBUG 0

#if EVENTLOOP_DEBUG
#define EVENTLOOP_DEBUG_C(x) x
#else
#define EVENTLOOP_DEBUG_C(x) do { } while(0)
#endif


//static bool g_should_quit = false;
static int g_kqueue_fd = 0;
static int g_main_thread_pipe_fd = 0;
static int g_kqueue_thread_pipe_fd = 0;
static bool g_quit_written = false;

#define NW_HOOK_MAP(type, sym, fn) extern type fn;
#include "content/nw/src/common/node_hooks.h"
#undef NW_HOOK_MAP

int kevent_hook(int kq, const struct kevent *changelist, int nchanges,
            struct kevent *eventlist, int nevents,
            const struct timespec *timeout) {
  int res;

  EVENTLOOP_DEBUG_C((printf("KQUEUE--- fd: %d changes: %d\n", kq, nchanges)));

#if 0 //EVENTLOOP_DEBUG
  for (int i = 0; i < nchanges; ++i) {
    dump_kevent(&changelist[i]);
  }
#endif

#if EVENTLOOP_BYPASS_CUSTOM
  int res = kevent(kq, changelist, nchanges, eventlist, nevents, timeout);
  printf("---> results: %d\n", res);
  for (int i = 0; i < res; ++i) {
    dump_kevent(&eventlist[i]);
  }
  return res;
#endif

  if (eventlist == NULL)  // Just updating the state.
    return kevent(kq, changelist, nchanges, eventlist, nevents, timeout);

  struct timespec zerotimeout;
  memset(&zerotimeout, 0, sizeof(zerotimeout));

  // Going for a poll.  A bit less optimial but we break it into two system
  // calls to make sure that the kqueue state is up to date.  We might as well
  // also peek since we basically get it for free w/ the same call.
  EVENTLOOP_DEBUG_C((printf("-- Updating kqueue state and peek\n")));
  res = kevent(kq, changelist, nchanges, eventlist, nevents, &zerotimeout);
  if (res != 0) return res;

  /*
  printf("kevent() blocking\n");
  res = kevent(kq, NULL, 0, eventlist, nevents, timeout);
  if (res != 0) return res;
  return res;
  */

  /*
  printf("Going for it...\n");
  res = kevent(kq, changelist, nchanges, eventlist, nevents, timeout);
  printf("<- %d\n", res);
  return res;
  */

  double ts = 9999;  // Timeout in seconds.  Default to some "future".
  if (timeout != NULL)
    ts = timeout->tv_sec + (timeout->tv_nsec / 1000000000.0);

  // NOTE(deanm): We only ever make a single pass, because we need to make
  // sure that any user code (which could update timers, etc) is reflected
  // and we have a proper timeout value.  Since user code can run in response
  // to [NSApp sendEvent] (mouse movement, keypress, etc, etc), we wind down
  // and go back through the uv loop again to make sure to update everything.

  EVENTLOOP_DEBUG_C((printf("-> Running NSApp iteration: timeout %f\n", ts)));

  // Have the helper thread start select()ing on the kqueue.
  write(g_main_thread_pipe_fd, "~", 1);

  [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:ts]];

  // Stop the helper thread if it hasn't already woken up (in which case it
  // would have already stopped itself).
  if (!g_quit_written)
    write(g_main_thread_pipe_fd, "!", 1);

  EVENTLOOP_DEBUG_C((printf("<- Finished NSApp iteration\n")));

  // Do the actual kqueue call now (ignore the timeout, don't block).
  return kevent(kq, NULL, 0, eventlist, nevents, &zerotimeout);
}

namespace base {

namespace {

// static
#if 0
const CFStringRef kMessageLoopExclusiveRunLoopMode =
    CFSTR("kMessageLoopExclusiveRunLoopMode");

void CFRunLoopAddSourceToAllModes(CFRunLoopRef rl, CFRunLoopSourceRef source) {
  CFRunLoopAddSource(rl, source, kCFRunLoopCommonModes);
  CFRunLoopAddSource(rl, source, kMessageLoopExclusiveRunLoopMode);
}

void CFRunLoopRemoveSourceFromAllModes(CFRunLoopRef rl,
                                       CFRunLoopSourceRef source) {
  CFRunLoopRemoveSource(rl, source, kCFRunLoopCommonModes);
  CFRunLoopRemoveSource(rl, source, kMessageLoopExclusiveRunLoopMode);
}
#endif

void NoOp(void* info) {
}

#if 0
void UvNoOp(void* handle) {
}
#endif

}  // namespace

// A scoper for autorelease pools created from message pump run loops.
// Avoids dirtying up the ScopedNSAutoreleasePool interface for the rare
// case where an autorelease pool needs to be passed in.
class MessagePumpScopedAutoreleasePool {
 public:
  explicit MessagePumpScopedAutoreleasePool(MessagePumpCFRunLoopBase* pump) :
      pool_(pump->CreateAutoreleasePool()) {
  }
   ~MessagePumpScopedAutoreleasePool() {
    [pool_ drain];
  }

 private:
  NSAutoreleasePool* pool_;
};

bool MessagePumpUVNSRunLoop::RunWork() {
  if (!delegate_) {
    // This point can be reached with a NULL delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_work_ = true;
    return false;
  }

  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);

  PopWorkItemScope();
  Delegate::NextWorkInfo next_work_info = delegate_->DoWork();
  PushWorkItemScope();
  
  if (next_work_info.is_immediate()) {
    CFRunLoopSourceSignal(work_source_);
    return true;
  } else {
    ScheduleDelayedWork(next_work_info);
    return false;
  }
}


void MessagePumpUVNSRunLoop::RunIdleWork() {
  if (!delegate_) {
    // This point can be reached with a NULL delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_idle_work_ = true;
    return;
  }

  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);

  // Call DoIdleWork once, and if something was done, arrange to come back here
  // again as long as the loop is still running.
  bool did_work = delegate_->DoIdleWork();
  if (did_work) {
    CFRunLoopSourceSignal(idle_work_source_);
  }

  return;
}

void MessagePumpUVNSRunLoop::PreWaitObserverHook() {
  // call tick callback before sleep in mach port
  // in the same way node upstream handle this in MakeCallBack,
  // or the tick callback is blocked in some cases
  nw::KickNextTick();
}

MessagePumpUVNSRunLoop::MessagePumpUVNSRunLoop()
    : MessagePumpCFRunLoopBase(0x1 /*kCommonModeMask*/), keep_running_(true) {
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.perform = NoOp;
  quit_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       0,     // priority
                                       &source_context);
  //InvokeForEnabledModes(&CFRunLoopAddSource, quit_source_);

  embed_closed_ = 0;
  nw_nesting_level_ = 0;
  int pipefds[2];
  if (pipe(pipefds) != 0) abort();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  int worker_support = command_line.HasSwitch("enable-node-worker") ? 1 : 0; //kEnableNodeWorker

  g_uv_init_nw_fn(worker_support);
  g_uv_default_loop_fn();

  g_kqueue_thread_pipe_fd = pipefds[0];
  g_main_thread_pipe_fd = pipefds[1];
  g_kqueue_fd = g_uv_backend_fd_fn();

  g_msg_pump_ctor_osx_fn(&ctx_, (void*)EmbedThreadRunner, (void*)kevent_hook, this, worker_support);
}

MessagePumpUVNSRunLoop::~MessagePumpUVNSRunLoop() {
  //InvokeForEnabledModes(&CFRunLoopRemoveSource, quit_source_);
  CFRelease(quit_source_);
  // Clear uv.
  embed_closed_ = 1;
  g_msg_pump_dtor_osx_fn(&ctx_);
}

void MessagePumpUVNSRunLoop::DoRun(Delegate* delegate) {

  nw_nesting_level_++;
  // Pause uv in nested loop.
  if (nw_nesting_level_ > 1) {
    pause_uv_ = true;
    while (keep_running_) {
      // NSRunLoop manages autorelease pools itself.
      [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                               beforeDate:[NSDate distantFuture]];
    }
  } else {
    while (keep_running_) {
      g_uv_runloop_once_fn();
    }
  }

  keep_running_ = true;
  // Resume uv.
  if (nw_nesting_level_ > 1) {
    pause_uv_ = false;
  }
  nw_nesting_level_--;
}

bool MessagePumpUVNSRunLoop::DoQuit() {
  keep_running_ = false;
  CFRunLoopSourceSignal(quit_source_);
  CFRunLoopWakeUp(run_loop());
  if (nw_nesting_level_ < 2 && !g_quit_written) { // NWJS#5020 
    g_quit_written = true;
    write(g_main_thread_pipe_fd, "q", 1);
  }
  return true;
}

void MessagePumpUVNSRunLoop::EmbedThreadRunner(void *arg) {
  bool check_kqueue = false;

  base::MessagePumpUVNSRunLoop* message_pump = static_cast<base::MessagePumpUVNSRunLoop*>(arg);

  NSAutoreleasePool* pool = [NSAutoreleasePool new];  // To avoid the warning.

  while (true) {
    int nfds = g_kqueue_thread_pipe_fd + 1;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(g_kqueue_thread_pipe_fd, &fds);
    if (check_kqueue) {
      FD_SET(g_kqueue_fd, &fds);
      if (g_kqueue_fd + 1 > nfds) nfds = g_kqueue_fd + 1;
    }

    EVENTLOOP_DEBUG_C((printf("Calling select: %d\n", check_kqueue)));
    int res = select(nfds, &fds, NULL, NULL, NULL);
    if (res <= 0) {
      if (res == -1 && errno == 4) //NWJS#6799
        continue;
      abort();  // TODO(deanm): Handle signals, etc.
    }

    if (FD_ISSET(g_kqueue_fd, &fds)) {
      EVENTLOOP_DEBUG_C((printf("postEvent\n")));
      message_pump->ScheduleWork();
      check_kqueue = false;
    }

    if (FD_ISSET(g_kqueue_thread_pipe_fd, &fds)) {
      char msg;
      ssize_t amt = read(g_kqueue_thread_pipe_fd, &msg, 1);
      if (amt != 1) abort();  // TODO(deanm): Handle errors.
      if (msg == 'q') {  // quit.
        EVENTLOOP_DEBUG_C((printf("quitting kqueue helper\n")));
        break;
      }
      check_kqueue = msg == '~';  // ~ - start, ! - stop.
    }
  }

  [pool drain];

}

}  // namespace base
