// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCROLL_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCROLL_DATA_H_

#include "third_party/blink/renderer/core/dom/element_rare_data_field.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/scroll/scroll_snapshot_client.h"
#include "third_party/blink/renderer/platform/graphics/compositor_element_id.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {

class Element;

// Created for each anchor-positioned element that uses anchor-scroll.
// Stores a snapshot of all the scroll containers of the anchor up to the
// containing block (exclusively) for use by layout, paint and compositing.
// The snapshot is updated once per frame update on top of animation frame to
// avoid layout cycling.
//
// === Validation of fallback position ===
//
// Each fallback position corresponds to a rectangular region such that when
// the anchor-scroll translation offset is within the region, the element's
// margin box translated by the offset doesn't overflow the containing block.
// Let's call it the fallback position's non-overflowing rect.
//
// Then the element should use the a fallback position if and only if:
// 1. The current translation offset is not in any previous fallback
//    position's non-overflowing rect, and
// 2. This is the last fallback position or the current translation offset is
//    in this fallback position's non-overflowing rect
//
// Whenever taking a snapshot, we also check if the above still holds for the
// current fallback position. If not, a layout invalidation is needed.
class AnchorScrollData : public GarbageCollected<AnchorScrollData>,
                         public ScrollSnapshotClient,
                         public ElementRareDataField {
 public:
  explicit AnchorScrollData(Element*);
  virtual ~AnchorScrollData();

  Element* OwnerElement() const { return owner_; }

  bool HasTranslation() const { return scroll_container_ids_.size(); }
  gfx::Vector2dF AccumulatedScrollOffset() const {
    return accumulated_scroll_offset_;
  }
  gfx::Vector2d AccumulatedScrollOrigin() const {
    return accumulated_scroll_origin_;
  }
  const Vector<CompositorElementId>& ScrollContainerIds() const {
    return scroll_container_ids_;
  }

  // Utility function that returns accumulated_scroll_offset_ rounded as a
  // PhysicalOffset.
  PhysicalOffset TranslationAsPhysicalOffset() const {
    return -PhysicalOffset::FromVector2dFFloor(accumulated_scroll_offset_);
  }

  // Returns whether |owner_| is still an anchor-positioned element using |this|
  // as its AnchroScrollData.
  bool IsActive() const;

  // For fallback position validation.
  void SetNonOverflowingRects(Vector<PhysicalRect>&& non_overflowing_rects) {
    non_overflowing_rects_ = std::move(non_overflowing_rects);
  }

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;
  bool ShouldScheduleNextService() override;

  void Trace(Visitor*) const override;

 private:
  enum class SnapshotDiff { kNone, kScrollersOrFallbackPosition, kOffsetOnly };
  // Takes an up-to-date snapshot, and compares it with the existing one.
  // If |update| is true, also rewrites the existing snapshot.
  SnapshotDiff TakeAndCompareSnapshot(bool update);
  bool IsFallbackPositionValid(
      const gfx::Vector2dF& accumulated_scroll_offset) const;

  void InvalidateLayout();
  void InvalidatePaint();

  // The anchor-positioned element.
  Member<Element> owner_;

  // Compositor element ids of the ancestor scroll containers of the anchor
  // element, up to the containing block of |owner_| (exclusively).
  Vector<CompositorElementId> scroll_container_ids_;

  // Sum of the scroll offsets of the above scroll containers. This is the
  // offset that the element should be translated in position-fallback choosing
  // and paint.
  gfx::Vector2dF accumulated_scroll_offset_;

  // Sum of the scroll origins of the above scroll containers. Used by
  // compositor to deal with writing modes.
  gfx::Vector2d accumulated_scroll_origin_;

  // TODO(crbug.com/1371217): Pass these rects to compositor, so that compositor
  // doesn't need to always trigger a main frame on every scroll, but only when
  // the element overflows the container. See also crbug.com/1381276.

  // See documentation of non-overflowing rects above.
  Vector<PhysicalRect> non_overflowing_rects_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_ANCHOR_SCROLL_DATA_H_
