// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"

#include <sys/socket.h>
#include <wayland-server.h>

#include <cstdlib>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "ui/ozone/platform/wayland/test/test_gtk_primary_selection.h"
#include "ui/ozone/platform/wayland/test/test_zwp_primary_selection.h"

namespace wl {

namespace {

void handle_client_destroyed(struct wl_listener* listener, void* data) {
  TestServerListener* destroy_listener =
      wl_container_of(listener, /*sample=*/destroy_listener,
                      /*member=*/listener);
  DCHECK(destroy_listener);
  destroy_listener->test_server->OnClientDestroyed(
      static_cast<struct wl_client*>(data));
}

}  // namespace

void DisplayDeleter::operator()(wl_display* display) {
  wl_display_destroy(display);
}

TestWaylandServerThread::TestWaylandServerThread()
    : Thread("test_wayland_server"),
      client_destroy_listener_(this),
      compositor_v4_(4),
      compositor_v3_(3),
      controller_(FROM_HERE) {
  DETACH_FROM_THREAD(thread_checker_);
}

TestWaylandServerThread::~TestWaylandServerThread() {
  // Stop watching the descriptor here to guarantee that no new events
  // will come during or after the destruction of the display. This must be
  // done on the correct thread to avoid data races.
  auto stop_controller_on_server_thread =
      [](wl::TestWaylandServerThread* server) {
        server->controller_.StopWatchingFileDescriptor();
      };
  RunAndWait(
      base::BindLambdaForTesting(std::move(stop_controller_on_server_thread)));

  Stop();

  if (protocol_logger_)
    wl_protocol_logger_destroy(protocol_logger_);
  protocol_logger_ = nullptr;

  // Check if the client has been destroyed after the thread is stopped. This
  // most probably will happen if the real client has closed its fd resulting
  // in a closed socket. The server's event loop will then see that and destroy
  // the client automatically. This may or may not happen - depends on whether
  // the events will be processed after the real client closes its end or not.
  if (client_)
    wl_client_destroy(client_);
  client_ = nullptr;
}

bool TestWaylandServerThread::Start(const ServerConfig& config) {
  display_.reset(wl_display_create());
  if (!display_)
    return false;
  event_loop_ = wl_display_get_event_loop(display_.get());

  int fd[2];
  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd) < 0)
    return false;
  base::ScopedFD server_fd(fd[0]);
  base::ScopedFD client_fd(fd[1]);

  if (wl_display_init_shm(display_.get()) < 0)
    return false;
  if (config.compositor_version == CompositorVersion::kV3) {
    if (!compositor_v3_.Initialize(display_.get()))
      return false;
  } else {
    if (!compositor_v4_.Initialize(display_.get()))
      return false;
  }
  if (!sub_compositor_.Initialize(display_.get()))
    return false;
  if (!viewporter_.Initialize(display_.get()))
    return false;
  if (!alpha_compositing_.Initialize(display_.get()))
    return false;

  if (!output_.Initialize(display_.get()))
    return false;
  SetupOutputs();

  if (!data_device_manager_.Initialize(display_.get()))
    return false;
  if (!SetupPrimarySelectionManager(config.primary_selection_protocol))
    return false;

  if (!seat_.Initialize(display_.get()))
    return false;

  if (!xdg_shell_.Initialize(display_.get()))
    return false;

  if (config.enable_aura_shell == EnableAuraShellProtocol::kEnabled) {
    if (!zxdg_output_manager_.Initialize(display_.get()))
      return false;

    output_.set_aura_shell_enabled();
    if (!zaura_shell_.Initialize(display_.get()))
      return false;
  }

  if (!zcr_stylus_.Initialize(display_.get()))
    return false;
  if (!zcr_text_input_extension_v1_.Initialize(display_.get()))
    return false;
  if (!zwp_text_input_manager_v1_.Initialize(display_.get()))
    return false;
  if (!SetupExplicitSynchronizationProtocol(
          config.use_explicit_synchronization))
    return false;
  if (!zwp_linux_dmabuf_v1_.Initialize(display_.get()))
    return false;
  if (!overlay_prioritizer_.Initialize(display_.get()))
    return false;
  if (!wp_pointer_gestures_.Initialize(display_.get()))
    return false;
  if (!zcr_color_manager_v1_.Initialize(display_.get())) {
    return false;
  }
  if (!xdg_activation_v1_.Initialize(display_.get())) {
    return false;
  }

  client_ = wl_client_create(display_.get(), server_fd.release());
  if (!client_)
    return false;

  client_destroy_listener_.listener.notify = handle_client_destroyed;
  wl_client_add_destroy_listener(client_, &client_destroy_listener_.listener);

  protocol_logger_ = wl_display_add_protocol_logger(
      display_.get(), TestWaylandServerThread::ProtocolLogger, this);

  base::Thread::Options options;
  options.message_pump_factory = base::BindRepeating(
      &TestWaylandServerThread::CreateMessagePump, base::Unretained(this));
  if (!base::Thread::StartWithOptions(std::move(options)))
    return false;

  setenv("WAYLAND_SOCKET", base::NumberToString(client_fd.release()).c_str(),
         1);

  return true;
}

void TestWaylandServerThread::RunAndWait(
    base::OnceCallback<void(TestWaylandServerThread*)> callback) {
  base::OnceClosure closure =
      base::BindOnce(std::move(callback), base::Unretained(this));
  RunAndWait(std::move(closure));
}

void TestWaylandServerThread::RunAndWait(base::OnceClosure closure) {
  // Allow nestable tasks for dnd tests.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&TestWaylandServerThread::DoRun, base::Unretained(this),
                     std::move(closure)),
      run_loop.QuitClosure());
  run_loop.Run();
}

MockWpPresentation* TestWaylandServerThread::EnsureAndGetWpPresentation() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (wp_presentation_.resource())
    return &wp_presentation_;
  if (wp_presentation_.Initialize(display_.get()))
    return &wp_presentation_;
  return nullptr;
}

TestSurfaceAugmenter* TestWaylandServerThread::EnsureSurfaceAugmenter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (surface_augmenter_.Initialize(display_.get()))
    return &surface_augmenter_;
  return nullptr;
}

void TestWaylandServerThread::OnClientDestroyed(wl_client* client) {
  if (!client_)
    return;

  DCHECK_EQ(client_, client);
  client_ = nullptr;
}

uint32_t TestWaylandServerThread::GetNextSerial() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return wl_display_next_serial(display_.get());
}

uint32_t TestWaylandServerThread::GetNextTime() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  static uint32_t timestamp = 0;
  return ++timestamp;
}

// By default, just make sure primary screen has bounds set. Otherwise delegates
// it, making it possible to emulate different scenarios, such as, multi-screen,
// lazy configuration, arbitrary ordering of the outputs metadata sending, etc.
void TestWaylandServerThread::SetupOutputs() {
  if (output_delegate_) {
    output_delegate_->SetupOutputs(&output_);
    return;
  }
  if (output_.GetRect().IsEmpty())
    output_.SetRect(gfx::Rect{0, 0, 800, 600});
}

bool TestWaylandServerThread::SetupPrimarySelectionManager(
    PrimarySelectionProtocol protocol) {
  switch (protocol) {
    case PrimarySelectionProtocol::kNone:
      return true;
    case PrimarySelectionProtocol::kZwp:
      primary_selection_device_manager_.reset(CreateTestSelectionManagerZwp());
      break;
    case PrimarySelectionProtocol::kGtk:
      primary_selection_device_manager_.reset(CreateTestSelectionManagerGtk());
      break;
  }
  return primary_selection_device_manager_->Initialize(display_.get());
}

bool TestWaylandServerThread::SetupExplicitSynchronizationProtocol(
    ShouldUseExplicitSynchronizationProtocol usage) {
  switch (usage) {
    case ShouldUseExplicitSynchronizationProtocol::kNone:
      return true;
    case ShouldUseExplicitSynchronizationProtocol::kUse:
      return zwp_linux_explicit_synchronization_v1_.Initialize(display_.get());
  }
  NOTREACHED();
  return false;
}

std::unique_ptr<base::MessagePump>
TestWaylandServerThread::CreateMessagePump() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto pump = std::make_unique<base::MessagePumpLibevent>();
  pump->WatchFileDescriptor(wl_event_loop_get_fd(event_loop_), true,
                            base::MessagePumpLibevent::WATCH_READ, &controller_,
                            this);
  return std::move(pump);
}

void TestWaylandServerThread::DoRun(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(closure).Run();
  wl_display_flush_clients(display_.get());
}

void TestWaylandServerThread::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  wl_event_loop_dispatch(event_loop_, 0);
  if (display_)
    wl_display_flush_clients(display_.get());
}

void TestWaylandServerThread::OnFileCanWriteWithoutBlocking(int fd) {}

// static
void TestWaylandServerThread::ProtocolLogger(
    void* user_data,
    enum wl_protocol_logger_type direction,
    const struct wl_protocol_logger_message* message) {
  auto* test_server = static_cast<TestWaylandServerThread*>(user_data);
  DCHECK(test_server);
  // All the protocol calls must be made on the correct thread.
  DCHECK_CALLED_ON_VALID_THREAD(test_server->thread_checker_);
}

}  // namespace wl
