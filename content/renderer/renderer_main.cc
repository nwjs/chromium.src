// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/debug/leak_annotations.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pending_task.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/hi_res_timer_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/scheduler/renderer/renderer_scheduler.h"
#include "components/startup_metric_utils/common/startup_metric_messages.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "ui/base/ui_base_switches.h"


#include "base/native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/node/src/node_webkit.h"

#if defined(OS_ANDROID)
#include "base/android/library_loader/library_loader_hooks.h"
#endif  // OS_ANDROID

#if defined(OS_MACOSX)
#include <Carbon/Carbon.h>
#include <signal.h>
#include <unistd.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/message_pump_mac.h"
#include "base/message_loop/message_pumpuv_mac.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "base/mac/bundle_locations.h"
#endif  // OS_MACOSX

#if defined(ENABLE_PLUGINS)
#include "content/renderer/pepper/pepper_plugin_registry.h"
#endif

#if defined(ENABLE_WEBRTC)
#include "third_party/libjingle/overrides/init_webrtc.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/client_native_pixmap_factory.h"
#endif

#if defined(MOJO_SHELL_CLIENT)
#include "content/common/mojo/mojo_shell_connection_impl.h"
#endif

extern VoidHookFn g_msg_pump_ctor_fn;
extern VoidHookFn g_msg_pump_dtor_fn;
extern VoidHookFn g_msg_pump_sched_work_fn, g_msg_pump_nest_leave_fn, g_msg_pump_need_work_fn;
extern VoidHookFn g_msg_pump_did_work_fn, g_msg_pump_pre_loop_fn, g_msg_pump_nest_enter_fn;
extern VoidIntHookFn g_msg_pump_delay_work_fn;
extern VoidHookFn g_msg_pump_clean_ctx_fn;
extern GetPointerFn g_uv_default_loop_fn;
extern NodeStartFn g_node_start_fn;
extern UVRunFn g_uv_run_fn;
extern SetUVRunFn g_set_uv_run_fn;

extern CallTickCallbackFn g_call_tick_callback_fn;
extern SetupNWNodeFn g_setup_nwnode_fn;
extern IsNodeInitializedFn g_is_node_initialized_fn;
extern SetNWTickCallbackFn g_set_nw_tick_callback_fn;
extern StartNWInstanceFn g_start_nw_instance_fn;
extern GetNodeContextFn g_get_node_context_fn;
extern SetNodeContextFn g_set_node_context_fn;
extern GetNodeEnvFn g_get_node_env_fn;
extern GetCurrentEnvironmentFn g_get_current_env_fn;
extern EmitExitFn g_emit_exit_fn;
extern RunAtExitFn g_run_at_exit_fn;
extern VoidHookFn g_promise_reject_callback_fn;

#if defined(OS_MACOSX)
extern VoidHookFn g_msg_pump_dtor_osx_fn, g_uv_sem_post_fn, g_uv_sem_wait_fn;
extern VoidPtr4Fn g_msg_pump_ctor_osx_fn;
extern IntVoidFn g_nw_uvrun_nowait_fn, g_uv_runloop_once_fn;
extern IntVoidFn g_uv_backend_timeout_fn;
extern IntVoidFn g_uv_backend_fd_fn;
#endif

namespace content {
namespace {
// This function provides some ways to test crash and assertion handling
// behavior of the renderer.
static void HandleRendererErrorTestParameters(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kWaitForDebugger))
    base::debug::WaitForDebugger(60, true);

  if (command_line.HasSwitch(switches::kRendererStartupDialog))
    ChildProcess::WaitForDebugger("Renderer");
}

#if defined(USE_OZONE)
base::LazyInstance<scoped_ptr<ui::ClientNativePixmapFactory>> g_pixmap_factory =
    LAZY_INSTANCE_INITIALIZER;
#endif

}  // namespace

// mainline routine for running as the Renderer process
int RendererMain(const MainFunctionParams& parameters) {
  // Don't use the TRACE_EVENT0 macro because the tracing infrastructure doesn't
  // expect synchronous events around the main loop of a thread.
  TRACE_EVENT_ASYNC_BEGIN0("startup", "RendererMain", 0);

  const base::TimeTicks renderer_main_entry_time = base::TimeTicks::Now();

  base::trace_event::TraceLog::GetInstance()->SetProcessName("Renderer");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventRendererProcessSortIndex);

  const base::CommandLine& parsed_command_line = parameters.command_line;

#if defined(MOJO_SHELL_CLIENT)
  if (parsed_command_line.HasSwitch(switches::kEnableMojoShellConnection))
    MojoShellConnectionImpl::Create();
#endif

  bool nwjs = parsed_command_line.HasSwitch(switches::kNWJS);

  struct SymbolDefinition {
    const char* name;
    VoidHookFn* fn;
  };
  const SymbolDefinition kSymbols[] = {
#if defined(OS_MACOSX)
    {"g_msg_pump_dtor_osx", &g_msg_pump_dtor_osx_fn },
    {"g_uv_sem_post", &g_uv_sem_post_fn },
    {"g_uv_sem_wait", &g_uv_sem_wait_fn },
#endif
    { "g_msg_pump_ctor", &g_msg_pump_ctor_fn },
    { "g_msg_pump_dtor", &g_msg_pump_dtor_fn },
    { "g_msg_pump_sched_work", &g_msg_pump_sched_work_fn },
    { "g_msg_pump_nest_leave", &g_msg_pump_nest_leave_fn },
    { "g_msg_pump_nest_enter", &g_msg_pump_nest_enter_fn },
    { "g_msg_pump_need_work", &g_msg_pump_need_work_fn },
    { "g_msg_pump_did_work", &g_msg_pump_did_work_fn },
    { "g_msg_pump_pre_loop", &g_msg_pump_pre_loop_fn },
    { "g_msg_pump_clean_ctx", &g_msg_pump_clean_ctx_fn },
    { "g_promise_reject_callback", &g_promise_reject_callback_fn}
  };
  if (nwjs) {
    base::NativeLibraryLoadError error;
#if defined(OS_MACOSX)
    base::FilePath node_dll_path = base::mac::FrameworkBundlePath().Append(base::FilePath::FromUTF16Unsafe(base::GetNativeLibraryName(base::UTF8ToUTF16("libnode"))));
#else
    base::FilePath node_dll_path = base::FilePath::FromUTF16Unsafe(base::GetNativeLibraryName(base::UTF8ToUTF16("node")));
#endif
    base::NativeLibrary node_dll = base::LoadNativeLibrary(node_dll_path, &error);
    if(!node_dll)
      LOG(FATAL) << "Failed to load node library (error: " << error.ToString() << ")";
    else {
      for (size_t i = 0; i < sizeof(kSymbols) / sizeof(kSymbols[0]); ++i) {
        *(kSymbols[i].fn) = (VoidHookFn)base::GetFunctionPointerFromNativeLibrary(node_dll, kSymbols[i].name);
        DCHECK(*kSymbols[i].fn) << "Unable to find symbol for "
                                << kSymbols[i].name;
      }
      g_msg_pump_delay_work_fn = (VoidIntHookFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_msg_pump_delay_work");
      g_node_start_fn = (NodeStartFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_node_start");
      g_uv_run_fn = (UVRunFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_uv_run");
      g_set_uv_run_fn = (SetUVRunFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_set_uv_run");
      g_uv_default_loop_fn = (GetPointerFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_uv_default_loop");

      g_call_tick_callback_fn = (CallTickCallbackFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_call_tick_callback");
      g_setup_nwnode_fn = (SetupNWNodeFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_setup_nwnode");
      g_is_node_initialized_fn = (IsNodeInitializedFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_is_node_initialized");
      g_set_nw_tick_callback_fn = (SetNWTickCallbackFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_set_nw_tick_callback");
      g_start_nw_instance_fn = (StartNWInstanceFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_start_nw_instance");
      g_get_node_context_fn = (GetNodeContextFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_get_node_context");
      g_set_node_context_fn = (SetNodeContextFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_set_node_context");
      g_get_node_env_fn = (GetNodeEnvFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_get_node_env");
      g_get_current_env_fn = (GetCurrentEnvironmentFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_get_current_env");
      g_emit_exit_fn = (EmitExitFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_emit_exit");
      g_run_at_exit_fn = (RunAtExitFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_run_at_exit");
#if defined(OS_MACOSX)
      g_msg_pump_ctor_osx_fn = (VoidPtr4Fn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_msg_pump_ctor_osx");
      g_nw_uvrun_nowait_fn = (IntVoidFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_nw_uvrun_nowait");
      g_uv_runloop_once_fn = (IntVoidFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_uv_runloop_once");
      g_uv_backend_timeout_fn = (IntVoidFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_uv_backend_timeout");
      g_uv_backend_fd_fn = (IntVoidFn)base::GetFunctionPointerFromNativeLibrary(node_dll, "g_uv_backend_fd");
#endif
    }
  }
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool* pool = parameters.autorelease_pool;
#endif  // OS_MACOSX

#if defined(OS_CHROMEOS)
  // As Zygote process starts up earlier than browser process gets its own
  // locale (at login time for Chrome OS), we have to set the ICU default
  // locale for renderer process here.
  // ICU locale will be used for fallback font selection etc.
  if (parsed_command_line.HasSwitch(switches::kLang)) {
    const std::string locale =
        parsed_command_line.GetSwitchValueASCII(switches::kLang);
    base::i18n::SetICUDefaultLocale(locale);
  }
#endif

  SkGraphics::Init();
#if defined(OS_ANDROID)
  const int kMB = 1024 * 1024;
  size_t font_cache_limit =
      base::SysInfo::IsLowEndDevice() ? kMB : 8 * kMB;
  SkGraphics::SetFontCacheLimit(font_cache_limit);
#endif

#if defined(USE_OZONE)
  g_pixmap_factory.Get() = ui::ClientNativePixmapFactory::Create();
  ui::ClientNativePixmapFactory::SetInstance(g_pixmap_factory.Get().get());
#endif

  // This function allows pausing execution using the --renderer-startup-dialog
  // flag allowing us to attach a debugger.
  // Do not move this function down since that would mean we can't easily debug
  // whatever occurs before it.
  HandleRendererErrorTestParameters(parsed_command_line);

  RendererMainPlatformDelegate platform(parameters);
#if defined(OS_MACOSX)
  // As long as scrollbars on Mac are painted with Cocoa, the message pump
  // needs to be backed by a Foundation-level loop to process NSTimers. See
  // http://crbug.com/306348#c24 for details.
  base::MessagePump* p;
  if (nwjs) {
    p = new base::MessagePumpUVNSRunLoop();
  } else
    p = new base::MessagePumpNSRunLoop();
  scoped_ptr<base::MessagePump> pump(p);
  scoped_ptr<base::MessageLoop> main_message_loop(
      new base::MessageLoop(std::move(pump)));
#else
  // The main message loop of the renderer services doesn't have IO or
  // UI tasks.
  base::MessageLoop* msg_loop;
  if (nwjs) {
    scoped_ptr<base::MessagePump> pump_uv(new base::MessagePumpUV());
    msg_loop = new base::MessageLoop(std::move(pump_uv));
  } else
    msg_loop = new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT);

  scoped_ptr<base::MessageLoop> main_message_loop(msg_loop);
#endif

  base::PlatformThread::SetName("CrRendererMain");

  bool no_sandbox = parsed_command_line.HasSwitch(switches::kNoSandbox);

  // Initialize histogram statistics gathering system.
  base::StatisticsRecorder::Initialize();

#if defined(OS_ANDROID)
  // If we have a pending chromium android linker histogram, record it.
  base::android::RecordChromiumAndroidLinkerRendererHistogram();
#endif

  // Initialize statistical testing infrastructure.  We set the entropy provider
  // to NULL to disallow the renderer process from creating its own one-time
  // randomized trials; they should be created in the browser process.
  base::FieldTrialList field_trial_list(NULL);
  // Ensure any field trials in browser are reflected into renderer.
  if (parsed_command_line.HasSwitch(switches::kForceFieldTrials)) {
    bool result = base::FieldTrialList::CreateTrialsFromString(
        parsed_command_line.GetSwitchValueASCII(switches::kForceFieldTrials),
        base::FieldTrialList::DONT_ACTIVATE_TRIALS,
        std::set<std::string>());
    DCHECK(result);
  }

  scoped_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      parsed_command_line.GetSwitchValueASCII(switches::kEnableFeatures),
      parsed_command_line.GetSwitchValueASCII(switches::kDisableFeatures));
  base::FeatureList::SetInstance(std::move(feature_list));

  scoped_ptr<scheduler::RendererScheduler> renderer_scheduler(
      scheduler::RendererScheduler::Create());

  // PlatformInitialize uses FieldTrials, so this must happen later.
  platform.PlatformInitialize();

#if defined(ENABLE_PLUGINS)
  // Load pepper plugins before engaging the sandbox.
  PepperPluginRegistry::GetInstance();
#endif
#if defined(ENABLE_WEBRTC)
  // Initialize WebRTC before engaging the sandbox.
  // NOTE: On linux, this call could already have been made from
  // zygote_main_linux.cc.  However, calling multiple times from the same thread
  // is OK.
  InitializeWebRtcModule();
#endif

  {
#if defined(OS_WIN) || defined(OS_MACOSX)
    // TODO(markus): Check if it is OK to unconditionally move this
    // instruction down.
    RenderProcessImpl render_process;
    RenderThreadImpl::Create(std::move(main_message_loop),
                             std::move(renderer_scheduler));
#endif
    bool run_loop = true;
    if (!no_sandbox)
      run_loop = platform.EnableSandbox();
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    RenderProcessImpl render_process;
    RenderThreadImpl::Create(std::move(main_message_loop),
                             std::move(renderer_scheduler));
#endif
    RenderThreadImpl::current()->Send(
        new StartupMetricHostMsg_RecordRendererMainEntryTime(
            renderer_main_entry_time));

    base::HighResolutionTimerManager hi_res_timer_manager;

    if (run_loop) {
#if defined(OS_MACOSX)
      if (pool)
        pool->Recycle();
#endif
      TRACE_EVENT_ASYNC_BEGIN0("toplevel", "RendererMain.START_MSG_LOOP", 0);
      base::MessageLoop::current()->Run();
      TRACE_EVENT_ASYNC_END0("toplevel", "RendererMain.START_MSG_LOOP", 0);
    }
#if defined(LEAK_SANITIZER)
    // Run leak detection before RenderProcessImpl goes out of scope. This helps
    // ignore shutdown-only leaks.
    __lsan_do_leak_check();
#endif
  }
  platform.PlatformUninitialize();
  TRACE_EVENT_ASYNC_END0("startup", "RendererMain", 0);
  return 0;
}

}  // namespace content
