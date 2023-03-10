// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/delegated_ink_prediction_configuration.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace {

// FieldTrialParams for `DynamicSchedulerForDraw` and
// `kDynamicSchedulerForClients`.
const char kDynamicSchedulerPercentile[] = "percentile";

}  // namespace

namespace features {

BASE_FEATURE(kEnableOverlayPrioritization,
             "EnableOverlayPrioritization",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseMultipleOverlays,
             "UseMultipleOverlays",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
const char kMaxOverlaysParam[] = "max_overlays";

BASE_FEATURE(kDelegatedCompositing,
             "DelegatedCompositing",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kVideoDetectorIgnoreNonVideos,
             "VideoDetectorIgnoreNonVideos",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSimpleFrameRateThrottling,
             "SimpleFrameRateThrottling",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// When wide color gamut content from the web is encountered, promote our
// display to wide color gamut if supported.
BASE_FEATURE(kDynamicColorGamut,
             "DynamicColorGamut",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Submit CompositorFrame from SynchronousLayerTreeFrameSink directly to viz in
// WebView.
BASE_FEATURE(kVizFrameSubmissionForWebView,
             "VizFrameSubmissionForWebView",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether we should use the real buffers corresponding to overlay candidates in
// order to do a pageflip test rather than allocating test buffers.
BASE_FEATURE(kUseRealBuffersForPageFlipTest,
             "UseRealBuffersForPageFlipTest",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_FUCHSIA)
// Enables SkiaOutputDeviceBufferQueue instead of Vulkan swapchain on Fuchsia.
BASE_FEATURE(kUseSkiaOutputDeviceBufferQueue,
             "UseSkiaOutputDeviceBufferQueue",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// Whether we should log extra debug information to webrtc native log.
BASE_FEATURE(kWebRtcLogCapturePipeline,
             "WebRtcLogCapturePipeline",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Enables swap chains to call SetPresentDuration to request DWM/OS to reduce
// vsync.
BASE_FEATURE(kUseSetPresentDuration,
             "UseSetPresentDuration",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)

// Enables platform supported delegated ink trails instead of Skia backed
// delegated ink trails.
BASE_FEATURE(kUsePlatformDelegatedInk,
             "UsePlatformDelegatedInk",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used to debug Android WebView Vulkan composite. Composite to an intermediate
// buffer and draw the intermediate buffer to the secondary command buffer.
BASE_FEATURE(kWebViewVulkanIntermediateBuffer,
             "WebViewVulkanIntermediateBuffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Hardcoded as disabled for WebView to have a different default for
// UseSurfaceLayerForVideo from chrome.
BASE_FEATURE(kUseSurfaceLayerForVideoDefault,
             "UseSurfaceLayerForVideoDefault",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebViewNewInvalidateHeuristic,
             "WebViewNewInvalidateHeuristic",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Historically media on android hardcoded SRGB color space because of lack of
// color space support in surface control. This controls if we want to use real
// color space in DisplayCompositor.
BASE_FEATURE(kUseRealVideoColorSpaceForDisplay,
             "UseRealVideoColorSpaceForDisplay",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDrawPredictedInkPoint,
             "DrawPredictedInkPoint",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kDraw1Point12Ms[] = "1-pt-12ms";
const char kDraw2Points6Ms[] = "2-pt-6ms";
const char kDraw1Point6Ms[] = "1-pt-6ms";
const char kDraw2Points3Ms[] = "2-pt-3ms";
const char kPredictorKalman[] = "kalman";
const char kPredictorLinearResampling[] = "linear-resampling";
const char kPredictorLinear1[] = "linear-1";
const char kPredictorLinear2[] = "linear-2";
const char kPredictorLsq[] = "lsq";

// Used by Viz to parameterize adjustments to scheduler deadlines.
BASE_FEATURE(kDynamicSchedulerForDraw,
             "DynamicSchedulerForDraw",
             base::FEATURE_DISABLED_BY_DEFAULT);
// User to parameterize adjustments to clients' deadlines.
BASE_FEATURE(kDynamicSchedulerForClients,
             "DynamicSchedulerForClients",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kMacCAOverlayQuad,
             "MacCAOverlayQuads",
             base::FEATURE_ENABLED_BY_DEFAULT);
// The maximum supported overlay quad number on Mac CALayerOverlay.
// The default is set to -1. When MaxNum is < 0, the default in CALayerOverlay
// will be used instead.
const base::FeatureParam<int> kMacCAOverlayQuadMaxNum{
    &kMacCAOverlayQuad, "MacCAOverlayQuadMaxNum", -1};
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
BASE_FEATURE(kCanSkipRenderPassOverlay,
             "CanSkipRenderPassOverlay",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

// TODO(crbug.com/1357744): Solve the vulkan flakiness issue before enabling
// this on Linux.
BASE_FEATURE(kAllowUndamagedNonrootRenderPassToSkip,
             "AllowUndamagedNonrootRenderPassToSkip",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to:
// - Perform periodic inactive frame culling.
// - Cull *all* frames in case of critical memory pressure, rather than keeping
//   one.
BASE_FEATURE(kAggressiveFrameCulling,
             "AggressiveFrameCulling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, do not rely on surface garbage collection to happen
// periodically, but trigger it eagerly, to avoid missing calls.
BASE_FEATURE(kEagerSurfaceGarbageCollection,
             "EagerSurfaceGarbageCollection",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Only applies when a caller has requested a custom BeginFrame rate via the
// Throttle() API in frame_sink_manager.mojom. If enabled, parameters related
// to the BeginFrame rate are overridden in viz to reflect the throttled rate
// before being circulated in the system. The most notable are the interval and
// deadline in BeginFrameArgs. If disabled, these parameters reflect the default
// vsync rate (the behavior at the time this feature was created.)
BASE_FEATURE(kOverrideThrottledFrameRateParams,
             "OverrideThrottledFrameRateParams",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to gate calling SetPurgeable on OutputPresenter::Image from
// SkiaOutputDeviceBufferQueue.
BASE_FEATURE(kBufferQueueImageSetPurgeable,
             "BufferQueueImageSetPurgeable",
             base::FEATURE_DISABLED_BY_DEFAULT);

// On platforms using SkiaOutputDeviceBufferQueue, when this is true
// SkiaRenderer will allocate and maintain a buffer queue of images for the root
// render pass, instead of SkiaOutputDeviceBufferQueue itself.
BASE_FEATURE(kRendererAllocatesImages,
             "RendererAllocatesImages",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// On all platforms when attempting to evict a FrameTree, the active
// viz::Surface can be not included. This feature ensures that the we always add
// the active viz::Surface to the eviction list.
//
// Furthermore, by default on Android, when a client is being evicted, it only
// evicts itself. This differs from Destkop platforms which evict the entire
// FrameTree along with the topmost viz::Surface. When this feature is enabled,
// Android will begin also evicting the entire FrameTree.
BASE_FEATURE(kEvictSubtree, "EvictSubtree", base::FEATURE_DISABLED_BY_DEFAULT);

bool IsOverlayPrioritizationEnabled() {
  return base::FeatureList::IsEnabled(kEnableOverlayPrioritization);
}

bool IsDelegatedCompositingEnabled() {
  return base::FeatureList::IsEnabled(kDelegatedCompositing);
}

// If a synchronous IPC should used when destroying windows. This exists to test
// the impact of removing the sync IPC.
bool IsSyncWindowDestructionEnabled() {
  static BASE_FEATURE(kSyncWindowDestruction, "SyncWindowDestruction",
                      base::FEATURE_ENABLED_BY_DEFAULT);

  return base::FeatureList::IsEnabled(kSyncWindowDestruction);
}

bool IsSimpleFrameRateThrottlingEnabled() {
  return base::FeatureList::IsEnabled(kSimpleFrameRateThrottling);
}

#if BUILDFLAG(IS_ANDROID)
bool IsDynamicColorGamutEnabled() {
  if (viz::AlwaysUseWideColorGamut())
    return false;
  auto* build_info = base::android::BuildInfo::GetInstance();
  if (build_info->sdk_int() < base::android::SDK_VERSION_Q)
    return false;
  return base::FeatureList::IsEnabled(kDynamicColorGamut);
}
#endif

bool IsUsingVizFrameSubmissionForWebView() {
  return base::FeatureList::IsEnabled(kVizFrameSubmissionForWebView);
}

bool ShouldUseRealBuffersForPageFlipTest() {
  return base::FeatureList::IsEnabled(kUseRealBuffersForPageFlipTest);
}

bool ShouldWebRtcLogCapturePipeline() {
  return base::FeatureList::IsEnabled(kWebRtcLogCapturePipeline);
}

#if BUILDFLAG(IS_WIN)
bool ShouldUseSetPresentDuration() {
  return base::FeatureList::IsEnabled(kUseSetPresentDuration);
}
#endif  // BUILDFLAG(IS_WIN)

absl::optional<int> ShouldDrawPredictedInkPoints() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return absl::nullopt;

  std::string predicted_points = GetFieldTrialParamValueByFeature(
      kDrawPredictedInkPoint, "predicted_points");
  if (predicted_points == kDraw1Point12Ms)
    return viz::PredictionConfig::k1Point12Ms;
  else if (predicted_points == kDraw2Points6Ms)
    return viz::PredictionConfig::k2Points6Ms;
  else if (predicted_points == kDraw1Point6Ms)
    return viz::PredictionConfig::k1Point6Ms;
  else if (predicted_points == kDraw2Points3Ms)
    return viz::PredictionConfig::k2Points3Ms;

  NOTREACHED();
  return absl::nullopt;
}

std::string InkPredictor() {
  if (!base::FeatureList::IsEnabled(kDrawPredictedInkPoint))
    return "";

  return GetFieldTrialParamValueByFeature(kDrawPredictedInkPoint, "predictor");
}

bool ShouldUsePlatformDelegatedInk() {
  return base::FeatureList::IsEnabled(kUsePlatformDelegatedInk);
}

bool UseSurfaceLayerForVideo() {
#if BUILDFLAG(IS_ANDROID)
  // SurfaceLayer video should work fine with new heuristic.
  if (base::FeatureList::IsEnabled(kWebViewNewInvalidateHeuristic))
    return true;

  // Allow enabling UseSurfaceLayerForVideo if webview is using surface control.
  if (::features::IsAndroidSurfaceControlEnabled()) {
    return true;
  }
  return base::FeatureList::IsEnabled(kUseSurfaceLayerForVideoDefault);
#else
  return true;
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool UseRealVideoColorSpaceForDisplay() {
  // We need Android S for proper color space support in SurfaceControl.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SdkVersion::SDK_VERSION_S)
    return false;

  return base::FeatureList::IsEnabled(
      features::kUseRealVideoColorSpaceForDisplay);
}
#endif

// Used by Viz to determine if viz::DisplayScheduler should dynamically adjust
// its frame deadline. Returns the percentile of historic draw times to base the
// deadline on. Or absl::nullopt if the feature is disabled.
absl::optional<double> IsDynamicSchedulerEnabledForDraw() {
  if (!base::FeatureList::IsEnabled(kDynamicSchedulerForDraw))
    return absl::nullopt;
  double result = base::GetFieldTrialParamByFeatureAsDouble(
      kDynamicSchedulerForDraw, kDynamicSchedulerPercentile, -1.0);
  if (result < 0.0)
    return absl::nullopt;
  return result;
}

// Used by Viz to determine if the frame deadlines provided to CC should be
// dynamically adjusted. Returns the percentile of historic draw times to base
// the deadline on. Or absl::nullopt if the feature is disabled.
absl::optional<double> IsDynamicSchedulerEnabledForClients() {
  if (!base::FeatureList::IsEnabled(kDynamicSchedulerForClients))
    return absl::nullopt;
  double result = base::GetFieldTrialParamByFeatureAsDouble(
      kDynamicSchedulerForClients, kDynamicSchedulerPercentile, -1.0);
  if (result < 0.0)
    return absl::nullopt;
  return result;
}

int MaxOverlaysConsidered() {
  if (!IsOverlayPrioritizationEnabled()) {
    return 1;
  }

  if (!base::FeatureList::IsEnabled(kUseMultipleOverlays)) {
    return 1;
  }

  return base::GetFieldTrialParamByFeatureAsInt(kUseMultipleOverlays,
                                                kMaxOverlaysParam, 8);
}

bool ShouldVideoDetectorIgnoreNonVideoFrames() {
  return base::FeatureList::IsEnabled(kVideoDetectorIgnoreNonVideos);
}

bool ShouldOverrideThrottledFrameRateParams() {
  return base::FeatureList::IsEnabled(kOverrideThrottledFrameRateParams);
}

bool ShouldRendererAllocateImages() {
  return base::FeatureList::IsEnabled(kRendererAllocatesImages);
}

}  // namespace features
