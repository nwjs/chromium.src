// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_INPUT_INJECTOR_WAYLAND_H_
#define REMOTING_HOST_LINUX_INPUT_INJECTOR_WAYLAND_H_

#include <gtk/gtk.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "remoting/base/constants.h"
#include "remoting/host/clipboard.h"
#include "remoting/host/input_injector.h"
#include "remoting/host/input_injector_constants_linux.h"
#include "remoting/host/input_injector_metadata.h"
#include "remoting/host/linux/clipboard_wayland.h"
#include "remoting/host/linux/remote_desktop_portal_injector.h"
#include "remoting/host/linux/wayland_manager.h"
#include "remoting/proto/internal.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "remoting/host/chromeos/point_transformer.h"
#endif

namespace remoting {

// A class to generate events on wayland.
class InputInjectorWayland : public InputInjector {
 public:
  explicit InputInjectorWayland(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  InputInjectorWayland(const InputInjectorWayland&) = delete;
  InputInjectorWayland& operator=(const InputInjectorWayland&) = delete;
  ~InputInjectorWayland() override;

  // Clipboard stub interface.
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // InputStub interface.
  void InjectKeyEvent(const protocol::KeyEvent& event) override;
  void InjectTextEvent(const protocol::TextEvent& event) override;
  void InjectMouseEvent(const protocol::MouseEvent& event) override;
  void InjectTouchEvent(const protocol::TouchEvent& event) override;
  void SetMetadata(InputInjectorMetadata metadata) override;

  // InputInjector interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;

 private:
  // The actual implementation resides in InputInjectorWayland::Core class.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    explicit Core(scoped_refptr<base::SingleThreadTaskRunner> task_runner);
    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;
    // Mirrors the ClipboardStub interface.
    void InjectClipboardEvent(const protocol::ClipboardEvent& event);
    // Mirrors the InputStub interface.
    void InjectKeyEvent(const protocol::KeyEvent& event);
    void InjectTextEvent(const protocol::TextEvent& event);
    void InjectMouseEvent(const protocol::MouseEvent& event);
    void SetRemoteDesktopSessionDetails(
        const webrtc::xdg_portal::SessionDetails& session_details);
    void SetClipboardSessionDetails(
        const webrtc::xdg_portal::SessionDetails& session_details);
    // Mirrors the InputInjector interface.
    void Start(std::unique_ptr<protocol::ClipboardStub> client_clipboard);
    // Sets a keyboard capability ready callback on the global
    // WaylandManager class.
    void SetKeyboardCapabilityCallback();

    void Shutdown();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();
    void SeatAcquiredKeyboardCapability();
    void InjectFakeKeyEvent();
    bool IsReady();
    void MaybeFlushPendingEvents();
    void InjectScrollWheelClicks(int button, int count);
    void InjectMouseButton(unsigned int code, bool pressed);
    void InjectMouseScroll(unsigned int axis, int steps);
    void MovePointerTo(int x, int y);
    void MovePointerBy(int delta_x, int delta_y);
    void InjectKeyPress(unsigned int code, bool pressed, bool is_code = true);

    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
    std::set<int> pressed_keys_;
    absl::optional<webrtc::DesktopVector> latest_mouse_position_;
    float wheel_ticks_x_ = 0;
    float wheel_ticks_y_ = 0;
    base::TimeTicks latest_tick_y_event_;

    // The direction of the last scroll event that resulted in at least one
    // "tick" being injected.
    ScrollDirection latest_tick_y_direction_ = ScrollDirection::NONE;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    PointTransformer point_transformer_;
#endif
    ClipboardWayland clipboard_;
    xdg_portal::RemoteDesktopPortalInjector remotedesktop_portal_;

    // If input is injected before complete initialization then some portal
    // APIs can crash. This flag is marked to track initialization,
    // and all inputs before the initialization is complete are added to
    // |pending_tasks| queue and injected upon initialization.
    bool remote_desktop_initialized_ = false;

    base::queue<base::OnceClosure> pending_remote_desktop_tasks_;

    // Similar to remote_desktop_initialized_, we keep the last clipboard event
    // but separated so that the remote desktop isn't blocked waiting for the
    // clipboard.
    bool clipboard_initialized_ = false;
    absl::optional<protocol::ClipboardEvent> pending_clipboard_event_;

    // Keeps track of whether or not the associated seat has keyboard
    // capability.
    bool seat_has_keyboard_capability_ = false;
  };
  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_INPUT_INJECTOR_WAYLAND_H_
