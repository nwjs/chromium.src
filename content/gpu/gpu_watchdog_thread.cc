// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_watchdog_thread.h"

#include <errno.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace content {
namespace {
#if defined(OS_CHROMEOS)
const base::FilePath::CharType
    kTtyFilePath[] = FILE_PATH_LITERAL("/sys/class/tty/tty0/active");
#endif
#if defined(USE_X11)
const unsigned char text[20] = "check";
#endif
}  // namespace

GpuWatchdogThread::GpuWatchdogThread(int timeout)
    : base::Thread("Watchdog"),
      watched_message_loop_(base::MessageLoop::current()),
      timeout_(base::TimeDelta::FromMilliseconds(timeout)),
      armed_(false),
      task_observer_(this),
      use_thread_cpu_time_(true),
      responsive_acknowledge_count_(0),
#if defined(OS_WIN)
      watched_thread_handle_(0),
      arm_cpu_time_(),
#endif
      suspended_(false),
#if defined(USE_X11)
      display_(NULL),
      window_(0),
      atom_(None),
#endif
      weak_factory_(this) {
  DCHECK(timeout >= 0);

#if defined(OS_WIN)
  // GetCurrentThread returns a pseudo-handle that cannot be used by one thread
  // to identify another. DuplicateHandle creates a "real" handle that can be
  // used for this purpose.
  BOOL result = DuplicateHandle(GetCurrentProcess(),
                                GetCurrentThread(),
                                GetCurrentProcess(),
                                &watched_thread_handle_,
                                THREAD_QUERY_INFORMATION,
                                FALSE,
                                0);
  DCHECK(result);
#endif

#if defined(OS_CHROMEOS)
  tty_file_ = base::OpenFile(base::FilePath(kTtyFilePath), "r");
#endif
#if defined(USE_X11)
  SetupXServer();
#endif
  watched_message_loop_->AddTaskObserver(&task_observer_);
}

void GpuWatchdogThread::PostAcknowledge() {
  // Called on the monitored thread. Responds with OnAcknowledge. Cannot use
  // the method factory. Rely on reference counting instead.
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&GpuWatchdogThread::OnAcknowledge, this));
}

void GpuWatchdogThread::CheckArmed() {
  // Acknowledge the watchdog if it has armed itself. The watchdog will not
  // change its armed state until it is acknowledged.
  if (armed()) {
    PostAcknowledge();
  }
}

void GpuWatchdogThread::Init() {
  // Schedule the first check.
  OnCheck(false);
}

void GpuWatchdogThread::CleanUp() {
  weak_factory_.InvalidateWeakPtrs();
}

GpuWatchdogThread::GpuWatchdogTaskObserver::GpuWatchdogTaskObserver(
    GpuWatchdogThread* watchdog)
    : watchdog_(watchdog) {
}

GpuWatchdogThread::GpuWatchdogTaskObserver::~GpuWatchdogTaskObserver() {
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::WillProcessTask(
    const base::PendingTask& pending_task) {
  watchdog_->CheckArmed();
}

void GpuWatchdogThread::GpuWatchdogTaskObserver::DidProcessTask(
    const base::PendingTask& pending_task) {
}

GpuWatchdogThread::~GpuWatchdogThread() {
  // Verify that the thread was explicitly stopped. If the thread is stopped
  // implicitly by the destructor, CleanUp() will not be called.
  DCHECK(!weak_factory_.HasWeakPtrs());

#if defined(OS_WIN)
  CloseHandle(watched_thread_handle_);
#endif

  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  if (power_monitor)
    power_monitor->RemoveObserver(this);

#if defined(OS_CHROMEOS)
  if (tty_file_)
    fclose(tty_file_);
#endif

#if defined(USE_X11)
  XDestroyWindow(display_, window_);
  XCloseDisplay(display_);
#endif

  watched_message_loop_->RemoveTaskObserver(&task_observer_);
}

void GpuWatchdogThread::OnAcknowledge() {
  CHECK(base::PlatformThread::CurrentId() == GetThreadId());

  // The check has already been acknowledged and another has already been
  // scheduled by a previous call to OnAcknowledge. It is normal for a
  // watched thread to see armed_ being true multiple times before
  // the OnAcknowledge task is run on the watchdog thread.
  if (!armed_)
    return;

  // Revoke any pending hang termination.
  weak_factory_.InvalidateWeakPtrs();
  armed_ = false;

  if (suspended_) {
    responsive_acknowledge_count_ = 0;
    return;
  }

  base::Time current_time = base::Time::Now();

  // The watchdog waits until at least 6 consecutive checks have returned in
  // less than 50 ms before it will start ignoring the CPU time in determining
  // whether to timeout. This is a compromise to allow startups that are slow
  // due to disk contention to avoid timing out, but once the GPU process is
  // running smoothly the watchdog will be able to detect hangs that don't use
  // the CPU.
  if ((current_time - check_time_) < base::TimeDelta::FromMilliseconds(50))
    responsive_acknowledge_count_++;
  else
    responsive_acknowledge_count_ = 0;

  if (responsive_acknowledge_count_ >= 6)
    use_thread_cpu_time_ = false;

  // If it took a long time for the acknowledgement, assume the computer was
  // recently suspended.
  bool was_suspended = (current_time > suspension_timeout_);

  // The monitored thread has responded. Post a task to check it again.
  task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&GpuWatchdogThread::OnCheck,
                            weak_factory_.GetWeakPtr(), was_suspended),
      0.5 * timeout_);
}

void GpuWatchdogThread::OnCheck(bool after_suspend) {
  CHECK(base::PlatformThread::CurrentId() == GetThreadId());

  // Do not create any new termination tasks if one has already been created
  // or the system is suspended.
  if (armed_ || suspended_)
    return;

  // Must set armed before posting the task. This task might be the only task
  // that will activate the TaskObserver on the watched thread and it must not
  // miss the false -> true transition.
  armed_ = true;

#if defined(OS_WIN)
  arm_cpu_time_ = GetWatchedThreadTime();

  //QueryUnbiasedInterruptTime(&arm_interrupt_time_);
#endif

  check_time_ = base::Time::Now();
  check_timeticks_ = base::TimeTicks::Now();
  // Immediately after the computer is woken up from being suspended it might
  // be pretty sluggish, so allow some extra time before the next timeout.
  base::TimeDelta timeout = timeout_ * (after_suspend ? 3 : 1);
  suspension_timeout_ = check_time_ + timeout * 2;

  // Post a task to the monitored thread that does nothing but wake up the
  // TaskObserver. Any other tasks that are pending on the watched thread will
  // also wake up the observer. This simply ensures there is at least one.
  watched_message_loop_->task_runner()->PostTask(FROM_HERE,
                                                 base::Bind(&base::DoNothing));

  // Post a task to the watchdog thread to exit if the monitored thread does
  // not respond in time.
  task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang,
                 weak_factory_.GetWeakPtr()),
      timeout);
}

// Use the --disable-gpu-watchdog command line switch to disable this.
void GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang() {
  // Should not get here while the system is suspended.
  DCHECK(!suspended_);

#if defined(OS_WIN)
  // Defer termination until a certain amount of CPU time has elapsed on the
  // watched thread.
  base::TimeDelta time_since_arm = GetWatchedThreadTime() - arm_cpu_time_;
  if (use_thread_cpu_time_ && (time_since_arm < timeout_)) {
    message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &GpuWatchdogThread::DeliberatelyTerminateToRecoverFromHang,
            weak_factory_.GetWeakPtr()),
        timeout_ - time_since_arm);
    return;
  }
#endif

  // If the watchdog woke up significantly behind schedule, disarm and reset
  // the watchdog check. This is to prevent the watchdog thread from terminating
  // when a machine wakes up from sleep or hibernation, which would otherwise
  // appear to be a hang.
  if (base::Time::Now() > suspension_timeout_) {
    armed_ = false;
    OnCheck(true);
    return;
  }

#if defined(USE_X11)
  XWindowAttributes attributes;
  XGetWindowAttributes(display_, window_, &attributes);

  XSelectInput(display_, window_, PropertyChangeMask);
  SetupXChangeProp();

  XFlush(display_);

  // We wait for the property change event with a timeout. If it arrives we know
  // that X is responsive and is not the cause of the watchdog trigger, so we
  // should
  // terminate. If it times out, it may be due to X taking a long time, but
  // terminating won't help, so ignore the watchdog trigger.
  XEvent event_return;
  base::TimeTicks deadline = base::TimeTicks::Now() + timeout_;
  while (true) {
    base::TimeDelta delta = deadline - base::TimeTicks::Now();
    if (delta < base::TimeDelta()) {
      return;
    } else {
      while (XCheckWindowEvent(display_, window_, PropertyChangeMask,
                               &event_return)) {
        if (MatchXEventAtom(&event_return))
          break;
      }
      struct pollfd fds[1];
      fds[0].fd = XConnectionNumber(display_);
      fds[0].events = POLLIN;
      int status = poll(fds, 1, delta.InMilliseconds());
      if (status == -1) {
        if (errno == EINTR) {
          continue;
        } else {
          LOG(FATAL) << "Lost X connection, aborting.";
          break;
        }
      } else if (status == 0) {
        return;
      } else {
        continue;
      }
    }
  }
#endif

  // For minimal developer annoyance, don't keep terminating. You need to skip
  // the call to base::Process::Terminate below in a debugger for this to be
  // useful.
  static bool terminated = false;
  if (terminated)
    return;

#if defined(OS_WIN)
  if (IsDebuggerPresent())
    return;
#endif

#if defined(OS_CHROMEOS)
  // Don't crash if we're not on tty1. This avoids noise in the GPU process
  // crashes caused by people who use VT2 but still enable crash reporting.
  char tty_string[8] = {0};
  if (tty_file_ &&
      !fseek(tty_file_, 0, SEEK_SET) &&
      fread(tty_string, 1, 7, tty_file_)) {
    int tty_number = -1;
    int num_res = sscanf(tty_string, "tty%d", &tty_number);
    if (num_res == 1 && tty_number != 1)
      return;
  }
#endif

// Store variables so they're available in crash dumps to help determine the
// cause of any hang.
#if 0
  ULONGLONG fire_interrupt_time;
  QueryUnbiasedInterruptTime(&fire_interrupt_time);

  // This is the time since the watchdog was armed, in 100ns intervals,
  // ignoring time where the computer is suspended.
  ULONGLONG interrupt_delay = fire_interrupt_time - arm_interrupt_time_;

  base::debug::Alias(&interrupt_delay);
  base::debug::Alias(&time_since_arm);

  bool using_high_res_timer = base::Time::IsHighResolutionTimerInUse();
  base::debug::Alias(&using_high_res_timer);
#endif

  base::Time current_time = base::Time::Now();
  base::TimeTicks current_timeticks = base::TimeTicks::Now();
  base::debug::Alias(&current_time);
  base::debug::Alias(&current_timeticks);

  LOG(ERROR) << "The GPU process hung. Terminating after "
             << timeout_.InMilliseconds() << " ms.";

  // Deliberately crash the process to create a crash dump.
  *((volatile int*)0) = 0x1337;

  terminated = true;
}

#if defined(USE_X11)
void GpuWatchdogThread::SetupXServer() {
  display_ = XOpenDisplay(NULL);
  window_ = XCreateWindow(display_, DefaultRootWindow(display_), 0, 0, 1, 1, 0,
                          CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
  atom_ = XInternAtom(display_, "CHECK", False);
}

void GpuWatchdogThread::SetupXChangeProp() {
  XChangeProperty(display_, window_, atom_, XA_STRING, 8, PropModeReplace, text,
                  (arraysize(text) - 1));
}

bool GpuWatchdogThread::MatchXEventAtom(XEvent* event) {
  if (event->xproperty.window == window_ && event->type == PropertyNotify &&
      event->xproperty.atom == atom_)
    return true;

  return false;
}

#endif
void GpuWatchdogThread::AddPowerObserver() {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&GpuWatchdogThread::OnAddPowerObserver, this));
}

void GpuWatchdogThread::OnAddPowerObserver() {
  base::PowerMonitor* power_monitor = base::PowerMonitor::Get();
  DCHECK(power_monitor);
  power_monitor->AddObserver(this);
}

void GpuWatchdogThread::OnSuspend() {
  suspended_ = true;

  // When suspending force an acknowledgement to cancel any pending termination
  // tasks.
  OnAcknowledge();
}

void GpuWatchdogThread::OnResume() {
  suspended_ = false;

  // After resuming jump-start the watchdog again.
  armed_ = false;
  OnCheck(true);
}

#if defined(OS_WIN)
base::TimeDelta GpuWatchdogThread::GetWatchedThreadTime() {
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME user_time;
  FILETIME kernel_time;
  BOOL result = GetThreadTimes(watched_thread_handle_,
                               &creation_time,
                               &exit_time,
                               &kernel_time,
                               &user_time);
  DCHECK(result);

  ULARGE_INTEGER user_time64;
  user_time64.HighPart = user_time.dwHighDateTime;
  user_time64.LowPart = user_time.dwLowDateTime;

  ULARGE_INTEGER kernel_time64;
  kernel_time64.HighPart = kernel_time.dwHighDateTime;
  kernel_time64.LowPart = kernel_time.dwLowDateTime;

  // Time is reported in units of 100 nanoseconds. Kernel and user time are
  // summed to deal with to kinds of hangs. One is where the GPU process is
  // stuck in user level, never calling into the kernel and kernel time is
  // not increasing. The other is where either the kernel hangs and never
  // returns to user level or where user level code
  // calls into kernel level repeatedly, giving up its quanta before it is
  // tracked, for example a loop that repeatedly Sleeps.
  return base::TimeDelta::FromMilliseconds(static_cast<int64_t>(
      (user_time64.QuadPart + kernel_time64.QuadPart) / 10000));
}
#endif

}  // namespace content
