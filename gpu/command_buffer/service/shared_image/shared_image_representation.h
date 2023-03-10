// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gl/dc_layer_overlay_image.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "ui/gfx/mac/io_surface.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"

extern "C" typedef struct AHardwareBuffer AHardwareBuffer;
#endif

#if BUILDFLAG(IS_WIN)
#include <d3d11.h>
#include <wrl/client.h>
#endif

typedef unsigned int GLenum;
class GrBackendSurfaceMutableState;
class SkPromiseImageTexture;

namespace cc {
class PaintOpBuffer;
}  // namespace cc

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace media {
class VASurface;
}  // namespace media

namespace gpu {
class TextureBase;

namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

enum class RepresentationAccessMode {
  kNone,
  kRead,
  kWrite,
};

// A representation of a SharedImageBacking for use with a specific use case /
// api.
class GPU_GLES2_EXPORT SharedImageRepresentation {
 public:
  using AccessMode = RepresentationAccessMode;

  // Used by derived classes.
  enum class AllowUnclearedAccess { kYes, kNo };

  SharedImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker);
  virtual ~SharedImageRepresentation();

  viz::SharedImageFormat format() const { return backing_->format(); }
  const gfx::Size& size() const { return backing_->size(); }
  const gfx::ColorSpace& color_space() const { return backing_->color_space(); }
  GrSurfaceOrigin surface_origin() const { return backing_->surface_origin(); }
  SkAlphaType alpha_type() const { return backing_->alpha_type(); }
  uint32_t usage() const { return backing_->usage(); }
  const gpu::Mailbox& mailbox() const { return backing_->mailbox(); }
  MemoryTypeTracker* tracker() { return tracker_; }
  bool IsCleared() const { return backing_->IsCleared(); }
  void SetCleared() { backing_->SetCleared(); }
  gfx::Rect ClearedRect() const { return backing_->ClearedRect(); }
  void SetClearedRect(const gfx::Rect& cleared_rect) {
    backing_->SetClearedRect(cleared_rect);
  }

  // Indicates that the underlying graphics context has been lost, and the
  // backing should be treated as destroyed.
  void OnContextLost() {
    has_context_ = false;
    backing_->OnContextLost();
  }

  // Returns the number of image planes expected based on the backing format.
  size_t NumPlanesExpected() const;

 protected:
  SharedImageManager* manager() const { return manager_; }
  SharedImageBacking* backing() const { return backing_; }
  bool has_context() const { return has_context_; }

  // Helper class for derived classes' Scoped*Access objects. Has tracking to
  // ensure a Scoped*Access does not outlive the representation it's associated
  // with.
  template <typename RepresentationClass>
  class ScopedAccessBase {
   public:
    ScopedAccessBase(RepresentationClass* representation)
        : representation_(representation) {
      DCHECK(!representation_->has_scoped_access_);
      representation_->has_scoped_access_ = true;
    }

    ScopedAccessBase(const ScopedAccessBase&) = delete;
    ScopedAccessBase& operator=(const ScopedAccessBase&) = delete;

    ~ScopedAccessBase() {
      DCHECK(representation_->has_scoped_access_);
      representation_->has_scoped_access_ = false;
    }

    RepresentationClass* representation() { return representation_; }
    const RepresentationClass* representation() const {
      return representation_;
    }

   private:
    const raw_ptr<RepresentationClass> representation_;
  };

 private:
  const raw_ptr<SharedImageManager, DanglingUntriaged> manager_;
  raw_ptr<SharedImageBacking> backing_;
  const raw_ptr<MemoryTypeTracker> tracker_;
  bool has_context_ = true;
  bool has_scoped_access_ = false;
};

class SharedImageRepresentationFactoryRef : public SharedImageRepresentation {
 public:
  SharedImageRepresentationFactoryRef(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  ~SharedImageRepresentationFactoryRef() override;

  const Mailbox& mailbox() const { return backing()->mailbox(); }
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) {
    backing()->Update(std::move(in_fence));
    backing()->OnWriteSucceeded();
  }
  bool CopyToGpuMemoryBuffer() { return backing()->CopyToGpuMemoryBuffer(); }
  bool PresentSwapChain() { return backing()->PresentSwapChain(); }
  void RegisterImageFactory(SharedImageFactory* factory) {
    backing()->RegisterImageFactory(factory);
  }
};

class GPU_GLES2_EXPORT GLTextureImageRepresentationBase
    : public SharedImageRepresentation {
 public:
  static constexpr GLenum kReadAccessMode = 0x8AF6;

  class ScopedAccess
      : public ScopedAccessBase<GLTextureImageRepresentationBase> {
   public:
    ScopedAccess(base::PassKey<GLTextureImageRepresentationBase> pass_key,
                 GLTextureImageRepresentationBase* representation)
        : ScopedAccessBase(representation) {}
    ~ScopedAccess() {
      representation()->UpdateClearedStateOnEndAccess();
      representation()->EndAccess();
    }
  };

  GLTextureImageRepresentationBase(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      GLenum mode,
      AllowUnclearedAccess allow_uncleared);

  // Gets the texture associated with the `plane_index` for SharedImageFormat.
  virtual gpu::TextureBase* GetTextureBase(int plane_index) = 0;
  // Calls GetTextureBase with `plane_index` = 0 for single planar formats eg.
  // RGB.
  gpu::TextureBase* GetTextureBase();

 protected:
  friend class SkiaGLImageRepresentation;
  friend class DawnEGLImageRepresentation;
  friend class GLTextureGLCommonRepresentation;

  // Can be overridden to handle clear state tracking when GL access begins or
  // ends.
  virtual void UpdateClearedStateOnBeginAccess() {}
  virtual void UpdateClearedStateOnEndAccess() {}

  virtual bool BeginAccess(GLenum mode) = 0;
  virtual void EndAccess() = 0;

  virtual bool SupportsMultipleConcurrentReadAccess();
};

class GPU_GLES2_EXPORT GLTextureImageRepresentation
    : public GLTextureImageRepresentationBase {
 public:
  GLTextureImageRepresentation(SharedImageManager* manager,
                               SharedImageBacking* backing,
                               MemoryTypeTracker* tracker)
      : GLTextureImageRepresentationBase(manager, backing, tracker) {}

  // Gets the texture associated with the `plane_index` for SharedImageFormat.
  virtual gles2::Texture* GetTexture(int plane_index) = 0;
  // Calls GetTexture with `plane_index` = 0 for single planar formats eg. RGB.
  gles2::Texture* GetTexture();

  gpu::TextureBase* GetTextureBase(int plane_index) override;

 protected:
  friend class WrappedGLTextureCompoundImageRepresentation;

  void UpdateClearedStateOnBeginAccess() override;
  void UpdateClearedStateOnEndAccess() override;
};

class GPU_GLES2_EXPORT GLTexturePassthroughImageRepresentation
    : public GLTextureImageRepresentationBase {
 public:
  GLTexturePassthroughImageRepresentation(SharedImageManager* manager,
                                          SharedImageBacking* backing,
                                          MemoryTypeTracker* tracker)
      : GLTextureImageRepresentationBase(manager, backing, tracker) {}

  // Gets the passthrough texture associated with the `plane_index` for
  // SharedImageFormat.
  virtual const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) = 0;
  // Calls GetTexturePassthrough with `plane_index` = 0 for single planar
  // formats eg. RGB.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough();

  gpu::TextureBase* GetTextureBase(int plane_index) override;

 private:
  friend class WrappedGLTexturePassthroughCompoundImageRepresentation;
};

class GPU_GLES2_EXPORT SkiaImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<SkiaImageRepresentation> {
   public:
    ScopedWriteAccess(base::PassKey<SkiaImageRepresentation> pass_key,
                      SkiaImageRepresentation* representation,
                      std::vector<sk_sp<SkSurface>> surfaces,
                      std::unique_ptr<GrBackendSurfaceMutableState> end_state);
    ScopedWriteAccess(
        base::PassKey<SkiaImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures,
        std::unique_ptr<GrBackendSurfaceMutableState> end_state);
    ~ScopedWriteAccess();

    // NOTE: All references to the returned SkSurface(s) must be destroyed
    // before ScopedWriteAccess is destroyed.
    SkSurface* surface() const {
      DCHECK(representation()->format().is_single_plane());
      return surface(0);
    }
    SkSurface* surface(int plane_index) const {
      return surfaces_[plane_index].get();
    }

    SkPromiseImageTexture* promise_image_texture() const {
      DCHECK(representation()->format().is_single_plane());
      return promise_image_texture(0);
    }
    SkPromiseImageTexture* promise_image_texture(int plane_index) const {
      return promise_image_textures_[plane_index].get();
    }

    [[nodiscard]] std::unique_ptr<GrBackendSurfaceMutableState> TakeEndState();

   private:
    // A vector of surfaces and promise textures corresponding to the number of
    // planes in SharedImageFormat.
    std::vector<sk_sp<SkSurface>> surfaces_;
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures_;
    std::unique_ptr<GrBackendSurfaceMutableState> end_state_;
  };

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<SkiaImageRepresentation> {
   public:
    ScopedReadAccess(
        base::PassKey<SkiaImageRepresentation> pass_key,
        SkiaImageRepresentation* representation,
        std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures,
        std::unique_ptr<GrBackendSurfaceMutableState> end_state);
    ~ScopedReadAccess();

    SkPromiseImageTexture* promise_image_texture() const {
      DCHECK(representation()->format().is_single_plane());
      return promise_image_texture(0);
    }
    SkPromiseImageTexture* promise_image_texture(int plane_index) const {
      return promise_image_textures_[plane_index].get();
    }
    sk_sp<SkImage> CreateSkImage(
        GrDirectContext* context,
        SkImage::TextureReleaseProc texture_release_proc = nullptr,
        SkImage::ReleaseContext release_context = nullptr) const;
    [[nodiscard]] std::unique_ptr<GrBackendSurfaceMutableState> TakeEndState();

   private:
    // A vector of promise textures corresponding to the number of planes in
    // SharedImageFormat.
    std::vector<sk_sp<SkPromiseImageTexture>> promise_image_textures_;
    std::unique_ptr<GrBackendSurfaceMutableState> end_state_;
  };

  SkiaImageRepresentation(SharedImageManager* manager,
                          SharedImageBacking* backing,
                          MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  // Note: See BeginWriteAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true);

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true);

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      AllowUnclearedAccess allow_uncleared,
      bool use_sk_surface = true);

  // Note: See BeginReadAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  virtual bool SupportsMultipleConcurrentReadAccess();

 protected:
  friend class WrappedSkiaCompoundImageRepresentation;

  // Begin the write access.
  //
  // update_rect is a hint to the backend about the portion of the image that
  // will be drawn to. Callers shouldn't draw outside of this area, but aren't
  // required to overwrite every pixel inside it.
  //
  // The implementations should insert semaphores into begin_semaphores vector
  // which client will wait on before writing the backing. The ownership of
  // begin_semaphores is not passed to client. The implementations can also
  // optionally insert semaphores into end_semaphores. If using end_semaphores,
  // the client must submit them with drawing operations which use the backing.
  // The ownership of end_semaphores are not passed to client. And client must
  // submit the end_semaphores before calling EndWriteAccess().
  //
  // The backing can assign end_state, and the caller must reset backing's state
  // to the end_state before calling EndWriteAccess().
  // Returns an empty vector on failure.
  virtual std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) = 0;
  virtual std::vector<sk_sp<SkPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) = 0;
  virtual void EndWriteAccess() = 0;

  // Begin the read access. The implementations should insert semaphores into
  // begin_semaphores vector which client will wait on before reading the
  // backing. The ownership of begin_semaphores is not passed to client.
  // The implementations can also optionally insert semaphores into
  // end_semaphores. If using end_semaphores, the client must submit them with
  // drawing operations which use the backing. The ownership of end_semaphores
  // are not passed to client. And client must submit the end_semaphores before
  // calling EndReadAccess().
  // The backing can assign end_state, and the caller must reset backing's state
  // to the end_state before calling EndReadAccess().
  // Returns an empty vector on failure.
  virtual std::vector<sk_sp<SkPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) = 0;
  virtual void EndReadAccess() = 0;
};

class GPU_GLES2_EXPORT DawnImageRepresentation
    : public SharedImageRepresentation {
 public:
  static constexpr uint32_t kWriteUsage =
      WGPUTextureUsage_CopyDst | WGPUTextureUsage_RenderAttachment;

  DawnImageRepresentation(SharedImageManager* manager,
                          SharedImageBacking* backing,
                          MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedAccess
      : public ScopedAccessBase<DawnImageRepresentation> {
   public:
    ScopedAccess(base::PassKey<DawnImageRepresentation> pass_key,
                 DawnImageRepresentation* representation,
                 WGPUTexture texture);
    ~ScopedAccess();

    // Get the unowned texture handle. The caller should take a reference
    // if necessary by doing wgpu::Texture texture(access->texture());
    WGPUTexture texture() const { return texture_; }

   private:
    WGPUTexture texture_ = 0;
  };

  // Calls BeginAccess and returns a ScopedAccess object which will EndAccess
  // when it goes out of scope. The Representation must outlive the returned
  // ScopedAccess.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(
      WGPUTextureUsage usage,
      AllowUnclearedAccess allow_uncleared);

 private:
  friend class WrappedDawnCompoundImageRepresentation;

  // This can return null in case of a Dawn validation error, for example if
  // usage is invalid.
  virtual WGPUTexture BeginAccess(WGPUTextureUsage usage) = 0;
  virtual void EndAccess() = 0;
};

class GPU_GLES2_EXPORT OverlayImageRepresentation
    : public SharedImageRepresentation {
 public:
  OverlayImageRepresentation(SharedImageManager* manager,
                             SharedImageBacking* backing,
                             MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<OverlayImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<OverlayImageRepresentation> pass_key,
                     OverlayImageRepresentation* representation,
                     gfx::GpuFenceHandle acquire_fence);
    ~ScopedReadAccess();

#if BUILDFLAG(IS_ANDROID)
    AHardwareBuffer* GetAHardwareBuffer() {
      return representation()->GetAHardwareBuffer();
    }
    // Deprecated. All code should use GetAHardwareBuffer() above, this function
    // will be deleted when GLSurfaceEGLSurface control will be able to deliver
    // fences via EndAccess.
    std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
    GetAHardwareBufferFenceSync() {
      return representation()->GetAHardwareBufferFenceSync();
    }
#elif BUILDFLAG(IS_OZONE)
    scoped_refptr<gfx::NativePixmap> GetNativePixmap() {
      return representation()->GetNativePixmap();
    }
#elif BUILDFLAG(IS_WIN)
    absl::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage() {
      return representation()->GetDCLayerOverlayImage();
    }
#elif BUILDFLAG(IS_MAC)
    gfx::ScopedIOSurface GetIOSurface() const {
      return representation()->GetIOSurface();
    }
    bool IsInUseByWindowServer() const {
      return representation()->IsInUseByWindowServer();
    }
#endif

    gfx::GpuFenceHandle TakeAcquireFence() { return std::move(acquire_fence_); }
    void SetReleaseFence(gfx::GpuFenceHandle release_fence) {
      // Note: We overwrite previous fence. In case if window manager uses fence
      // for each frame we schedule overlay and the same image is scheduled for
      // multiple frames this will be updated after each frame. It's safe to
      // wait only for the last frame's fence.
      release_fence_ = std::move(release_fence);
    }

   private:
    gfx::GpuFenceHandle acquire_fence_;
    gfx::GpuFenceHandle release_fence_;
  };

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

 protected:
  friend class WrappedOverlayCompoundImageRepresentation;

  // Notifies the backing that an access will start. Returns false if there is a
  // conflict. Otherwise, returns true and:
  // - Set a gpu fence to |acquire_fence| that should be waited on before the
  // SharedImage is ready to be displayed. This fence is fired when the gpu
  // has finished writing.
  virtual bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) = 0;

  // |release_fence| is a fence that will be signaled when the image can be
  // safely re-used. Note, on some platforms window manager doesn't support
  // release fences and return image when it's already safe to re-use.
  // |release_fence| will be null in that case.
  virtual void EndReadAccess(gfx::GpuFenceHandle release_fence) = 0;

#if BUILDFLAG(IS_ANDROID)
  virtual AHardwareBuffer* GetAHardwareBuffer();
  virtual std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync();
#elif BUILDFLAG(IS_OZONE)
  scoped_refptr<gfx::NativePixmap> GetNativePixmap();
#elif BUILDFLAG(IS_WIN)
  virtual absl::optional<gl::DCLayerOverlayImage> GetDCLayerOverlayImage();
#elif BUILDFLAG(IS_MAC)
  virtual gfx::ScopedIOSurface GetIOSurface() const;
  // Return true if the macOS WindowServer is currently using the underlying
  // storage for the image.
  virtual bool IsInUseByWindowServer() const;
#endif
};

#if BUILDFLAG(IS_ANDROID)
class GPU_GLES2_EXPORT LegacyOverlayImageRepresentation
    : public SharedImageRepresentation {
 public:
  LegacyOverlayImageRepresentation(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  // Renders shared image to SurfaceView/Dialog overlay. Should only be called
  // if the image already promoted to overlay.
  virtual void RenderToOverlay() = 0;

  // Notifies legacy overlay system about overlay promotion.
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
};
#endif

class GPU_GLES2_EXPORT MemoryImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<MemoryImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<MemoryImageRepresentation> pass_key,
                     MemoryImageRepresentation* representation,
                     SkPixmap pixmap);
    ~ScopedReadAccess();

    SkPixmap pixmap() { return pixmap_; }

   private:
    SkPixmap pixmap_;
  };

  MemoryImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

 protected:
  virtual SkPixmap BeginReadAccess() = 0;
};

// An interface that allows a SharedImageBacking to hold a reference to VA-API
// surface without depending on //media/gpu/vaapi targets.
class VaapiDependencies {
 public:
  virtual ~VaapiDependencies() = default;
  virtual const media::VASurface* GetVaSurface() const = 0;
  virtual bool SyncSurface() = 0;
};

// Interface that allows a SharedImageBacking to create VaapiDependencies from a
// NativePixmap without depending on //media/gpu/vaapi targets.
class VaapiDependenciesFactory {
 public:
  virtual ~VaapiDependenciesFactory() = default;
  // Returns a VaapiDependencies or nullptr on failure.
  virtual std::unique_ptr<VaapiDependencies> CreateVaapiDependencies(
      scoped_refptr<gfx::NativePixmap> pixmap) = 0;
};

// Representation of a SharedImageBacking as a VA-API surface.
// This representation is currently only supported by OzoneImageBacking.
//
// Synchronized access is currently not required in this representation because:
//
// For reads:
// We will be using this for the destination of decoding work, so no read access
// synchronization is needed from the point of view of the VA-API.
//
// For writes:
// Because of the design of the current video pipeline, we don't start the
// decoding work until we're sure that the destination buffer is not being used
// by the rest of the pipeline. However, we still need to keep track of write
// accesses so that other representations can synchronize with the decoder.
class GPU_GLES2_EXPORT VaapiImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<VaapiImageRepresentation> {
   public:
    ScopedWriteAccess(base::PassKey<VaapiImageRepresentation> pass_key,
                      VaapiImageRepresentation* representation);

    ~ScopedWriteAccess();

    const media::VASurface* va_surface();
  };
  VaapiImageRepresentation(SharedImageManager* manager,
                           SharedImageBacking* backing,
                           MemoryTypeTracker* tracker,
                           VaapiDependencies* vaapi_dependency);
  ~VaapiImageRepresentation() override;

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess();

 private:
  friend class WrappedVaapiRepresentation;

  raw_ptr<VaapiDependencies> vaapi_deps_;
  virtual void EndAccess() = 0;
  virtual void BeginAccess() = 0;
};

// Representation of a SharedImageBacking for raster work.
// This representation is used for raster work and compositor. The raster work
// will be converted to a cc::PaintOpBuffer and stored in the
// SharedImageBacking. And then the the compositor will access the stored
// cc::PaintOpBuffer and execute paint ops in it.
class GPU_GLES2_EXPORT RasterImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedReadAccess
      : public ScopedAccessBase<RasterImageRepresentation> {
   public:
    ScopedReadAccess(base::PassKey<RasterImageRepresentation> pass_key,
                     RasterImageRepresentation* representation,
                     const cc::PaintOpBuffer* paint_op_buffer,
                     const absl::optional<SkColor4f>& clear_color);
    ~ScopedReadAccess();

    const cc::PaintOpBuffer* paint_op_buffer() const {
      return paint_op_buffer_;
    }
    const absl::optional<SkColor4f>& clear_color() const {
      return clear_color_;
    }

   private:
    const raw_ptr<const cc::PaintOpBuffer> paint_op_buffer_;
    absl::optional<SkColor4f> clear_color_;
  };

  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<RasterImageRepresentation> {
   public:
    ScopedWriteAccess(base::PassKey<RasterImageRepresentation> pass_key,
                      RasterImageRepresentation* representation,
                      cc::PaintOpBuffer* paint_op_buffer);
    ~ScopedWriteAccess();

    cc::PaintOpBuffer* paint_op_buffer() { return paint_op_buffer_; }
    // An optional callback which will be called when the all paint ops in the
    // |paint_op_buffer_| are released.
    void set_callback(base::OnceClosure callback) {
      DCHECK(!callback_);
      DCHECK(callback);
      callback_ = std::move(callback);
    }

   private:
    const raw_ptr<cc::PaintOpBuffer> paint_op_buffer_;
    base::OnceClosure callback_;
  };

  RasterImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess();

  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      scoped_refptr<SharedContextState> context_state,
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor4f>& clear_color,
      bool visible);

 protected:
  virtual cc::PaintOpBuffer* BeginReadAccess(
      absl::optional<SkColor4f>& clear_color) = 0;
  virtual void EndReadAccess() = 0;
  virtual cc::PaintOpBuffer* BeginWriteAccess(
      scoped_refptr<SharedContextState> context_state,
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const absl::optional<SkColor4f>& clear_color,
      bool visible) = 0;
  virtual void EndWriteAccess(base::OnceClosure callback) = 0;
};

class GPU_GLES2_EXPORT VideoDecodeImageRepresentation
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess
      : public ScopedAccessBase<VideoDecodeImageRepresentation> {
   public:
    ScopedWriteAccess(base::PassKey<VideoDecodeImageRepresentation> pass_key,
                      VideoDecodeImageRepresentation* representation);
    ~ScopedWriteAccess();

#if BUILDFLAG(IS_WIN)
    Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const {
      return representation()->GetD3D11Texture();
    }
#endif  // BUILDFLAG(IS_WIN)
  };

  VideoDecodeImageRepresentation(SharedImageManager* manager,
                                 SharedImageBacking* backing,
                                 MemoryTypeTracker* tracker);
  ~VideoDecodeImageRepresentation() override;

  virtual std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess();

 protected:
#if BUILDFLAG(IS_WIN)
  virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetD3D11Texture() const = 0;
#endif  // BUILDFLAG(IS_WIN)
  virtual bool BeginWriteAccess() = 0;
  virtual void EndWriteAccess() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_REPRESENTATION_H_
