// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SWAP_CHAIN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SWAP_CHAIN_H_

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class GPUDevice;
class GPUTexture;
class XRCompositionLayer;

class XRGPUSwapChain : public GarbageCollected<XRGPUSwapChain> {
 public:
  virtual ~XRGPUSwapChain() = default;

  virtual GPUTexture* GetCurrentTexture() = 0;
  virtual void OnFrameStart();
  virtual void OnFrameEnd();

  virtual const wgpu::TextureDescriptor& descriptor() const = 0;

  virtual void SetLayer(XRCompositionLayer* layer) { layer_ = layer; }
  XRCompositionLayer* layer() { return layer_.Get(); }

  virtual void Trace(Visitor* visitor) const;

 private:
  Member<XRCompositionLayer> layer_;
};

// A swap chain backed by Mailboxes
class XRGPUMailboxSwapChain : public XRGPUSwapChain {
 public:
  XRGPUMailboxSwapChain(GPUDevice* device, const wgpu::TextureDescriptor& desc);
  ~XRGPUMailboxSwapChain() override = default;

  GPUTexture* GetCurrentTexture() override;

  void OnFrameEnd() override;

  const wgpu::TextureDescriptor& descriptor() const override {
    return descriptor_;
  }

  void Trace(Visitor* visitor) const override;

 private:
  Member<GPUDevice> device_;
  Member<GPUTexture> texture_;
  wgpu::TextureDescriptor descriptor_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_GPU_SWAP_CHAIN_H_
