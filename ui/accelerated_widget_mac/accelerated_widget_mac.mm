// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accelerated_widget_mac/accelerated_widget_mac.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/geometry/dip_util.h"

namespace content {
  extern bool g_force_cpu_draw;
}

namespace ui {
namespace {

typedef std::map<gfx::AcceleratedWidget,AcceleratedWidgetMac*>
    WidgetToHelperMap;
base::LazyInstance<WidgetToHelperMap>::DestructorAtExit g_widget_to_helper_map;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratedWidgetMac

AcceleratedWidgetMac::AcceleratedWidgetMac() : view_(nullptr) {
  // Use a sequence number as the accelerated widget handle that we can use
  // to look up the internals structure.
  static uint64_t last_sequence_number = 0;
  native_widget_ = ++last_sequence_number;
  g_widget_to_helper_map.Pointer()->insert(
      std::make_pair(native_widget_, this));
}

AcceleratedWidgetMac::~AcceleratedWidgetMac() {
  DCHECK(!view_);
  g_widget_to_helper_map.Pointer()->erase(native_widget_);
}

void AcceleratedWidgetMac::SetNSView(AcceleratedWidgetMacNSView* view) {
  DCHECK(view && !view_);
  view_ = view;
}

void AcceleratedWidgetMac::ResetNSView() {
  last_ca_layer_params_valid_ = false;
  view_ = NULL;
}

const gfx::CALayerParams* AcceleratedWidgetMac::GetCALayerParams() const {
  if (!last_ca_layer_params_valid_)
    return nullptr;
  return &last_ca_layer_params_;
}

bool AcceleratedWidgetMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  if (!last_ca_layer_params_valid_)
    return false;
  gfx::Size last_swap_size_dip = gfx::ConvertSizeToDIP(
      last_ca_layer_params_.scale_factor, last_ca_layer_params_.pixel_size);
  return last_swap_size_dip == dip_size;
}

// static
AcceleratedWidgetMac* AcceleratedWidgetMac::Get(gfx::AcceleratedWidget widget) {
  WidgetToHelperMap::const_iterator found =
      g_widget_to_helper_map.Pointer()->find(widget);
  // This can end up being accessed after the underlying widget has been
  // destroyed, but while the ui::Compositor is still being destroyed.
  // Return NULL in these cases.
  if (found == g_widget_to_helper_map.Pointer()->end())
    return nullptr;
  return found->second;
}

// static
NSView* AcceleratedWidgetMac::GetNSView(gfx::AcceleratedWidget widget) {
  AcceleratedWidgetMac* widget_mac = Get(widget);
  if (!widget_mac || !widget_mac->view_)
    return nil;
  return widget_mac->view_->AcceleratedWidgetGetNSView();
}

void AcceleratedWidgetMac::SetSuspended(bool is_suspended) {
  is_suspended_ = is_suspended;
}

void AcceleratedWidgetMac::UpdateCALayerTree(
    const gfx::CALayerParams& ca_layer_params) {
  if (is_suspended_)
    return;

  last_ca_layer_params_valid_ = true;
  last_ca_layer_params_ = ca_layer_params;
  if (view_)
    view_->AcceleratedWidgetCALayerParamsUpdated();
}


#if 0
// this function is "copied" from AcceleratedWidgetMac::GotIOSurfaceFrame
void AcceleratedWidgetMac::GotSoftwareFrame(float scale_factor,
                                            SkCanvas* canvas) {
  TRACE_EVENT0("ui", "AcceleratedWidgetMac GotSoftwareFrame");
  assert(content::g_force_cpu_draw);
  if (!canvas || !view_) {
    TRACE_EVENT0("ui", "No associated NSView or No canvas");
    return;
  }

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  // Create (if needed) and update the IOSurface layer with new content.
  if (!io_surface_layer_) {
    io_surface_layer_.reset([[CALayer alloc] init]);
    [io_surface_layer_ setContentsGravity:kCAGravityTopLeft];
    [io_surface_layer_ setAnchorPoint:CGPointMake(0, 0)];
    [flipped_layer_ addSublayer:io_surface_layer_];
    if (content::g_force_cpu_draw)
      [io_surface_layer_.get() setBackgroundColor:[flipped_layer_.get() backgroundColor]];
  }

  // Set the software layer to draw the provided canvas.
  SkPixmap pixmap;
  canvas->peekPixels(&pixmap);
  const SkImageInfo& info = pixmap.info();
  const size_t row_bytes = pixmap.rowBytes();
  const void* pixels = pixmap.addr();
  gfx::Size pixel_size(info.width(), info.height());
  last_swap_size_dip_ = gfx::ConvertSizeToDIP(scale_factor, pixel_size);

  // Set the contents of the software CALayer to be a CGImage with the provided
  // pixel data. Make a copy of the data before backing the image with them,
  // because the same buffer will be reused for the next frame.
  base::ScopedCFTypeRef<CFDataRef> dataCopy(
      CFDataCreate(NULL,
                   static_cast<const UInt8 *>(pixels),
                   row_bytes * pixel_size.height()));
  base::ScopedCFTypeRef<CGDataProviderRef> dataProvider(
      CGDataProviderCreateWithCFData(dataCopy));
  base::ScopedCFTypeRef<CGImageRef> image(
      CGImageCreate(pixel_size.width(),
                    pixel_size.height(),
                    8,
                    32,
                    row_bytes,
                    base::mac::GetSystemColorSpace(),
                    kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
                    dataProvider,
                    NULL,
                    false,
                    kCGRenderingIntentDefault));

  id new_contents = (id)image.get();
  if (new_contents && new_contents == [io_surface_layer_ contents])
    [io_surface_layer_ setContentsChanged];
  else
    [io_surface_layer_ setContents:new_contents];
  [io_surface_layer_ setBounds:CGRectMake(0, 0, last_swap_size_dip_.width(),
                                          last_swap_size_dip_.height())];
  if ([io_surface_layer_ contentsScale] != scale_factor)
    [io_surface_layer_ setContentsScale:scale_factor];

  // Ensure that the content layer is removed.
  if (content_layer_) {
    [content_layer_ removeFromSuperlayer];
    content_layer_.reset();
  }
  
  if (content::g_force_cpu_draw) {
    // this is to tell parent window, that the window content has been updated
    [[view_->AcceleratedWidgetGetNSView() superview]setNeedsDisplay:YES];
  }
  view_->AcceleratedWidgetSwapCompleted();
}

void AcceleratedWidgetMacGotSoftwareFrame(
    gfx::AcceleratedWidget widget, float scale_factor, SkCanvas* canvas) {
  assert(content::g_force_cpu_draw);

  AcceleratedWidgetMac* accelerated_widget_mac =
  ui::AcceleratedWidgetMac::Get(widget);
  if (accelerated_widget_mac)
    accelerated_widget_mac->GotSoftwareFrame(scale_factor, canvas);
}
#endif

}  // namespace ui
