// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_layout_algorithm.h"

#include "third_party/blink/renderer/core/layout/ng/ng_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_disable_side_effects_scope.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_layout_part.h"
#include "third_party/blink/renderer/core/layout/ng/ng_relative_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_space_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NGGridLayoutAlgorithm::NGGridLayoutAlgorithm(
    const NGLayoutAlgorithmParams& params)
    : NGLayoutAlgorithm(params) {
  DCHECK(params.space.IsNewFormattingContext());

  const auto& node = Node();
  const auto& style = Style();
  const auto& constraint_space = ConstraintSpace();

  // At various stages of the algorithm we need to know the grid available-size.
  // If it's initially indefinite, we need to know the min/max sizes as well.
  // Initialize all these to the same value.
  grid_available_size_ = grid_min_available_size_ = grid_max_available_size_ =
      ChildAvailableSize();

  // If our inline-size is indefinite, compute the min/max inline-sizes.
  if (grid_available_size_.inline_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().InlineSum();

    const MinMaxSizes sizes = ComputeMinMaxInlineSizes(
        constraint_space, node, container_builder_.BorderPadding(),
        [&border_scrollbar_padding](MinMaxSizesType) -> MinMaxSizesResult {
          // If we've reached here we are inside the |ComputeMinMaxSizes| pass,
          // and also have something like "min-width: min-content". This is
          // cyclic. Just return the border/scrollbar/padding as our
          // "intrinsic" size.
          return MinMaxSizesResult(
              {border_scrollbar_padding, border_scrollbar_padding},
              /* depends_on_block_constraints */ false);
        });

    grid_min_available_size_.inline_size =
        (sizes.min_size - border_scrollbar_padding).ClampNegativeToZero();
    grid_max_available_size_.inline_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding).ClampNegativeToZero();
  }

  // And similar for the min/max block-sizes.
  if (grid_available_size_.block_size == kIndefiniteSize) {
    const LayoutUnit border_scrollbar_padding =
        BorderScrollbarPadding().BlockSum();
    const MinMaxSizes sizes = ComputeMinMaxBlockSizes(
        constraint_space, style, container_builder_.BorderPadding());

    grid_min_available_size_.block_size =
        (sizes.min_size - border_scrollbar_padding).ClampNegativeToZero();
    grid_max_available_size_.block_size =
        (sizes.max_size == LayoutUnit::Max())
            ? sizes.max_size
            : (sizes.max_size - border_scrollbar_padding).ClampNegativeToZero();

    // If block-size containment applies compute the block-size ignoring
    // children (just based on the row definitions).
    if (node.ShouldApplyBlockSizeContainment()) {
      contain_intrinsic_block_size_ =
          ComputeIntrinsicBlockSizeIgnoringChildren();

      // Resolve the block-size, and set the available sizes.
      const LayoutUnit block_size = ComputeBlockSizeForFragment(
          constraint_space, style, BorderPadding(),
          *contain_intrinsic_block_size_, container_builder_.InlineSize());

      grid_available_size_.block_size = grid_min_available_size_.block_size =
          grid_max_available_size_.block_size =
              (block_size - border_scrollbar_padding).ClampNegativeToZero();
    }
  }
}

namespace {

void CacheGridItemsProperties(
    const NGGridLayoutTrackCollection& track_collection,
    GridItems* grid_items,
    const Vector<GridItemIndices>* range_indices = nullptr,
    const Vector<GridArea>* resolved_positions = nullptr) {
  DCHECK(grid_items);

  GridItemDataPtrVector grid_items_spanning_multiple_ranges;
  const auto track_direction = track_collection.Direction();

  for (wtf_size_t index = 0; index < grid_items->Size(); ++index) {
    auto& grid_item = grid_items->At(index);

    // If positions range indices were provided, assign them before querying
    // `RangeIndices` below.
    if (resolved_positions && resolved_positions->size())
      grid_item.resolved_position = resolved_positions->at(index);

    if (range_indices && range_indices->size()) {
      if (track_direction == kForColumns)
        grid_item.column_range_indices = range_indices->at(index);
      else
        grid_item.row_range_indices = range_indices->at(index);
    }

    const auto& item_range_indices = grid_item.RangeIndices(track_direction);
    auto& track_span_properties = (track_direction == kForColumns)
                                      ? grid_item.column_span_properties
                                      : grid_item.row_span_properties;

    grid_item.ComputeSetIndices(track_collection);
    track_span_properties.Reset();

    // If a grid item spans only one range, then we can just cache the track
    // span properties directly. On the contrary, if a grid item spans multiple
    // tracks, it is added to |grid_items_spanning_multiple_ranges| as we need
    // to do more work to cache its track span properties.
    //
    // TODO(layout-dev): Investigate applying this concept to spans > 1.
    if (item_range_indices.begin == item_range_indices.end) {
      track_span_properties =
          track_collection.RangeProperties(item_range_indices.begin);
    } else {
      grid_items_spanning_multiple_ranges.emplace_back(&grid_item);
    }
  }

  if (grid_items_spanning_multiple_ranges.empty())
    return;

  auto CompareGridItemsByStartLine =
      [track_direction](GridItemData* lhs, GridItemData* rhs) -> bool {
    return lhs->StartLine(track_direction) < rhs->StartLine(track_direction);
  };
  std::sort(grid_items_spanning_multiple_ranges.begin(),
            grid_items_spanning_multiple_ranges.end(),
            CompareGridItemsByStartLine);

  auto CacheGridItemsSpanningMultipleRangesProperty =
      [&](TrackSpanProperties::PropertyId property) {
        // At this point we have the remaining grid items sorted by start line
        // in the respective direction; this is important since we'll process
        // both, the ranges in the track collection and the grid items,
        // incrementally.
        wtf_size_t current_range_index = 0;
        const wtf_size_t range_count = track_collection.RangeCount();

        for (auto* grid_item : grid_items_spanning_multiple_ranges) {
          // We want to find the first range in the collection that:
          //   - Spans tracks located AFTER the start line of the current grid
          //   item; this can be done by checking that the last track number of
          //   the current range is NOT less than the current grid item's start
          //   line. Furthermore, since grid items are sorted by start line, if
          //   at any point a range is located BEFORE the current grid item's
          //   start line, the same range will also be located BEFORE any
          //   subsequent item's start line.
          //   - Contains a track that fulfills the specified property.
          while (current_range_index < range_count &&
                 (track_collection.RangeEndLine(current_range_index) <=
                      grid_item->StartLine(track_direction) ||
                  !track_collection.RangeProperties(current_range_index)
                       .HasProperty(property))) {
            ++current_range_index;
          }

          // Since we discarded every range in the track collection, any
          // following grid item cannot fulfill the property.
          if (current_range_index == range_count)
            break;

          // Notice that, from the way we build the ranges of a track collection
          // (see |NGGridRangeBuilder::EnsureTrackCoverage|), any given range
          // must either be completely contained or excluded from a grid item's
          // span. Thus, if the current range's last track is also located
          // BEFORE the item's end line, then this range, including a track that
          // fulfills the specified property, is completely contained within
          // this item's boundaries. Otherwise, this and every subsequent range
          // are excluded from the grid item's span, meaning that such item
          // cannot satisfy the property we are looking for.
          if (track_collection.RangeEndLine(current_range_index) <=
              grid_item->EndLine(track_direction)) {
            grid_item->SetTrackSpanProperty(property, track_direction);
          }
        }
      };

  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasFlexibleTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasIntrinsicTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasAutoMinimumTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasFixedMinimumTrack);
  CacheGridItemsSpanningMultipleRangesProperty(
      TrackSpanProperties::kHasFixedMaximumTrack);
}

bool HasBlockSizeDependentGridItem(const GridItems& grid_items) {
  for (const auto& grid_item : grid_items) {
    if (grid_item.is_sizing_dependent_on_block_size)
      return true;
  }
  return false;
}

}  // namespace

const NGLayoutResult* NGGridLayoutAlgorithm::Layout() {
  const auto* result = LayoutInternal();
  if (result->Status() == NGLayoutResult::kDisableFragmentation) {
    DCHECK(ConstraintSpace().HasBlockFragmentation());
    return RelayoutWithoutFragmentation<NGGridLayoutAlgorithm>();
  }
  return result;
}

const NGLayoutResult* NGGridLayoutAlgorithm::LayoutInternal() {
  PaintLayerScrollableArea::DelayScrollOffsetClampScope delay_clamp_scope;

  const auto& node = Node();
  HeapVector<Member<LayoutBox>> oof_children;

  // Don't re-accumulate out-of-flow children if we're resuming layout, since
  // that data is stored on the break token.
  auto grid_sizing_tree = IsBreakInside(BreakToken())
                              ? BuildGridSizingTree()
                              : BuildGridSizingTree(&oof_children);

  LayoutUnit intrinsic_block_size;
  auto& grid_items = grid_sizing_tree[0].grid_items;
  auto& layout_data = grid_sizing_tree[0].layout_data;

  if (IsBreakInside(BreakToken())) {
    // TODO(layout-dev): When we support variable inline-size fragments we'll
    // need to re-run |ComputeGridGeometry| for the different inline-size.
    // When doing this, we'll need to make sure that we don't recalculate the
    // automatic repetitions (this depends on available size), as this might
    // change the grid structure significantly (e.g. pull a child up into the
    // first row).
    const auto* grid_data = To<NGGridBreakTokenData>(BreakToken()->TokenData());
    intrinsic_block_size = grid_data->intrinsic_block_size;
    layout_data = grid_data->layout_data;

    // Update `grid_items` with resolved positions and range indices stored
    // on the break token, as these are dependent on the `layout_data` above.
    //
    // TODO(kschmi): If these don't change between fragmentainers, we can store
    // them (and Columns/Rows) on `NGGridBreakTokenData` and avoid recomputing.
    CacheGridItemsProperties(layout_data.Columns(), &grid_items,
                             &grid_data->column_range_indices,
                             &grid_data->resolved_positions);
    CacheGridItemsProperties(layout_data.Rows(), &grid_items,
                             &grid_data->row_range_indices);
  } else {
    ComputeGridGeometry(&grid_sizing_tree, &intrinsic_block_size);
  }

  // Subgridded items must be placed by their parent.
  grid_items.RemoveSubgriddedItems();

  Vector<EBreakBetween> row_break_between;
  LayoutUnit consumed_grid_block_size;
  Vector<GridItemPlacementData> grid_items_placement_data;
  Vector<LayoutUnit> row_offset_adjustments;
  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    // Either retrieve all items offsets, or generate them using the
    // non-fragmented |PlaceGridItems| pass.
    if (IsBreakInside(BreakToken())) {
      const auto* grid_data =
          To<NGGridBreakTokenData>(BreakToken()->TokenData());

      consumed_grid_block_size = grid_data->consumed_grid_block_size;
      grid_items_placement_data = grid_data->grid_items_placement_data;
      row_offset_adjustments = grid_data->row_offset_adjustments;
      row_break_between = grid_data->row_break_between;
      oof_children = grid_data->oof_children;
    } else {
      row_offset_adjustments =
          Vector<LayoutUnit>(layout_data.Rows().GetSetCount() + 1);
      PlaceGridItems(grid_items, layout_data, &row_break_between,
                     &grid_items_placement_data);
    }

    PlaceGridItemsForFragmentation(
        grid_items, row_break_between, &layout_data, &grid_items_placement_data,
        &row_offset_adjustments, &intrinsic_block_size,
        &consumed_grid_block_size);
  } else {
    PlaceGridItems(grid_items, layout_data, &row_break_between);
  }

  const auto& border_padding = BorderPadding();
  const auto& constraint_space = ConstraintSpace();
  const auto block_size = ComputeBlockSizeForFragment(
      constraint_space, Style(), border_padding, intrinsic_block_size,
      container_builder_.InlineSize());

  // For scrollable overflow purposes grid is unique in that the "inflow-bounds"
  // are the size of the grid, and *not* where the inflow grid-items are placed.
  // Explicitly set the inflow-bounds to the grid size.
  if (node.IsScrollContainer()) {
    LogicalOffset offset = {layout_data.Columns().GetSetOffset(0),
                            layout_data.Rows().GetSetOffset(0)};

    LogicalSize size = {layout_data.Columns().ComputeSetSpanSize(),
                        layout_data.Rows().ComputeSetSpanSize()};

    container_builder_.SetInflowBounds(LogicalRect(offset, size));
  }
  container_builder_.SetMayHaveDescendantAboveBlockStart(false);

  // Grid is slightly different to other layout modes in that the contents of
  // the grid won't change if the initial block-size changes definiteness (for
  // example). We can safely mark ourselves as not having any children
  // dependent on the block constraints.
  container_builder_.SetHasDescendantThatDependsOnPercentageBlockSize(false);

  if (constraint_space.HasKnownFragmentainerBlockSize()) {
    // |FinishFragmentation| uses |NGBoxFragmentBuilder::IntrinsicBlockSize| to
    // determine the final size of this fragment. We don't have an accurate
    // "per-fragment" intrinsic block-size so just set it to the trailing
    // border-padding.
    container_builder_.SetIntrinsicBlockSize(border_padding.block_end);
  } else {
    container_builder_.SetIntrinsicBlockSize(intrinsic_block_size);
  }
  container_builder_.SetFragmentsTotalBlockSize(block_size);

  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    auto status = FinishFragmentation(
        node, constraint_space, border_padding.block_end,
        FragmentainerSpaceLeft(constraint_space), &container_builder_);
    if (status == NGBreakStatus::kDisableFragmentation)
      return container_builder_.Abort(NGLayoutResult::kDisableFragmentation);
    DCHECK_EQ(status, NGBreakStatus::kContinue);
  } else {
#if DCHECK_IS_ON()
    // If we're not participating in a fragmentation context, no block
    // fragmentation related fields should have been set.
    container_builder_.CheckNoBlockFragmentation();
#endif
  }

  // Set our break-before/break-after.
  if (constraint_space.ShouldPropagateChildBreakValues()) {
    container_builder_.SetInitialBreakBefore(row_break_between.front());
    container_builder_.SetPreviousBreakAfter(row_break_between.back());
  }

  if (!oof_children.empty())
    PlaceOutOfFlowItems(layout_data, block_size, oof_children);

  if (ConstraintSpace().HasBlockFragmentation()) {
    Vector<GridItemIndices> column_range_indices;
    Vector<GridItemIndices> row_range_indices;
    Vector<GridArea> resolved_positions;
    for (auto& grid_item : grid_items) {
      column_range_indices.push_back(grid_item.column_range_indices);
      row_range_indices.push_back(grid_item.row_range_indices);
      resolved_positions.push_back(grid_item.resolved_position);
    }
    container_builder_.SetBreakTokenData(
        MakeGarbageCollected<NGGridBreakTokenData>(
            container_builder_.GetBreakTokenData(), layout_data,
            intrinsic_block_size, consumed_grid_block_size,
            std::move(column_range_indices), std::move(row_range_indices),
            std::move(resolved_positions), grid_items_placement_data,
            row_offset_adjustments, row_break_between, oof_children));
  }

  // Copy grid layout data for use in computed style and devtools.
  container_builder_.TransferGridLayoutData(
      std::make_unique<NGGridLayoutData>(layout_data));

  NGOutOfFlowLayoutPart(node, constraint_space, &container_builder_).Run();
  return container_builder_.ToBoxFragment();
}

MinMaxSizesResult NGGridLayoutAlgorithm::ComputeMinMaxSizes(
    const MinMaxSizesFloatInput&) {
  const auto& node = Node();
  const LayoutUnit override_intrinsic_inline_size =
      node.OverrideIntrinsicContentInlineSize();

  if (override_intrinsic_inline_size != kIndefiniteSize) {
    const LayoutUnit size =
        BorderScrollbarPadding().InlineSum() + override_intrinsic_inline_size;
    return {{size, size}, /* depends_on_block_constraints */ false};
  }

  // If we have inline size containment ignore all children.
  auto grid_sizing_tree = node.ShouldApplyInlineSizeContainment()
                              ? BuildGridSizingTreeIgnoringChildren()
                              : BuildGridSizingTree();

  auto& grid_items = grid_sizing_tree[0].grid_items;
  auto& layout_data = grid_sizing_tree[0].layout_data;

  InitializeTrackCollections(&grid_sizing_tree);
  CacheGridItemsProperties(layout_data.Columns(), &grid_items);
  CacheGridItemsProperties(layout_data.Rows(), &grid_items);

  bool depends_on_block_constraints = false;
  auto ComputeTotalColumnSize =
      [&](SizingConstraint sizing_constraint) -> LayoutUnit {
    ComputeUsedTrackSizes(layout_data, sizing_constraint, &grid_items,
                          &layout_data.Rows(),
                          /* needs_additional_pass */ nullptr,
                          /* only_initialize_track_sizes */ true);

    bool needs_additional_pass = false;
    ComputeUsedTrackSizes(layout_data, sizing_constraint, &grid_items,
                          &layout_data.Columns(), &needs_additional_pass);

    if (needs_additional_pass || HasBlockSizeDependentGridItem(grid_items)) {
      // If we need to calculate the row geometry, we have a dependency on our
      // block constraints.
      depends_on_block_constraints = true;

      if (layout_data.Columns().IsForSizing() &&
          layout_data.Rows().IsForSizing()) {
        ComputeUsedTrackSizes(layout_data, sizing_constraint, &grid_items,
                              &layout_data.Rows(), &needs_additional_pass);

        if (needs_additional_pass) {
          ComputeUsedTrackSizes(layout_data, sizing_constraint, &grid_items,
                                &layout_data.Columns());
        }
      }
    }
    return layout_data.Columns().ComputeSetSpanSize();
  };

  MinMaxSizes sizes{ComputeTotalColumnSize(SizingConstraint::kMinContent),
                    ComputeTotalColumnSize(SizingConstraint::kMaxContent)};
  sizes += BorderScrollbarPadding().InlineSum();

  // TODO(crbug.com/1272533): This should be |depends_on_block_constraints|
  // (rather than false). However we need more cache slots to handle the
  // performance degredation we currently experience. See bug for more details.
  return MinMaxSizesResult(sizes, /* depends_on_block_constraints */ false);
}

namespace {

GridArea SubgriddedAreaInParent(const GridItemData& subgrid_data) {
  DCHECK(subgrid_data.IsSubgrid());

  auto subgridded_area_in_parent = subgrid_data.resolved_position;

  if (!subgrid_data.is_parallel_with_root_grid) {
    std::swap(subgridded_area_in_parent.columns,
              subgridded_area_in_parent.rows);
  }

  if (!subgrid_data.has_subgridded_columns) {
    subgridded_area_in_parent.columns = GridSpan::IndefiniteGridSpan();
  }
  if (!subgrid_data.has_subgridded_rows) {
    subgridded_area_in_parent.rows = GridSpan::IndefiniteGridSpan();
  }
  return subgridded_area_in_parent;
}

}  // namespace

wtf_size_t NGGridLayoutAlgorithm::BuildGridSizingSubtree(
    NGGridSizingTree* sizing_tree,
    HeapVector<Member<LayoutBox>>* oof_children,
    const NGGridLineResolver* parent_line_resolver,
    const NGGridSizingData* parent_sizing_data,
    const GridItemData* subgrid_data,
    bool must_ignore_children) const {
  DCHECK(sizing_tree);

  const auto& node = Node();
  const auto& style = node.Style();
  wtf_size_t column_auto_repetitions = kNotFound;
  wtf_size_t row_auto_repetitions = kNotFound;

  // TODO(ethavar): Compute automatic repetitions for subgridded axes as
  // described in https://drafts.csswg.org/css-grid-2/#auto-repeat.
  if (!parent_sizing_data) {
    column_auto_repetitions = ComputeAutomaticRepetitions(kForColumns);
    row_auto_repetitions = ComputeAutomaticRepetitions(kForRows);
  }

  // Initialize this grid's placement data.
  // TODO(kschmi): Remove placement data from `NGGridPlacement`.
  auto placement_data =
      parent_line_resolver
          ? NGGridPlacementData(style, *parent_line_resolver,
                                SubgriddedAreaInParent(*subgrid_data))
          : NGGridPlacementData(style, column_auto_repetitions,
                                row_auto_repetitions);
  bool has_nested_subgrid = false;
  auto& sizing_data = sizing_tree->CreateSizingData();

  if (!must_ignore_children) {
    // Construct grid items that are not subgridded.
    sizing_data.grid_items = node.ConstructGridItems(
        placement_data, oof_children, &has_nested_subgrid);

    placement_data.column_start_offset =
        node.CachedPlacementData().column_start_offset;
    placement_data.row_start_offset =
        node.CachedPlacementData().row_start_offset;
  }

  auto BuildSizingCollection = [&](GridTrackSizingDirection track_direction) {
    NGGridRangeBuilder range_builder(style, placement_data, track_direction);

    bool must_create_baselines = false;
    for (auto& grid_item : sizing_data.grid_items) {
      must_create_baselines |=
          grid_item.IsBaselineSpecifiedForDirection(track_direction);

      auto& range_indices = grid_item.RangeIndices(track_direction);
      range_builder.EnsureTrackCoverage(grid_item.StartLine(track_direction),
                                        grid_item.SpanSize(track_direction),
                                        &range_indices.begin,
                                        &range_indices.end);
    }
    sizing_data.layout_data.SetTrackCollection(
        std::make_unique<NGGridSizingTrackCollection>(
            range_builder.FinalizeRanges(), must_create_baselines,
            track_direction));
  };

  const bool must_build_sizing_column_collection =
      !subgrid_data || !subgrid_data->has_subgridded_columns;
  const bool must_build_sizing_row_collection =
      !subgrid_data || !subgrid_data->has_subgridded_rows;

  if (must_build_sizing_column_collection)
    BuildSizingCollection(kForColumns);
  if (must_build_sizing_row_collection)
    BuildSizingCollection(kForRows);

  if (!has_nested_subgrid)
    return sizing_data.subtree_size;

  NGSubgridSizingData opt_subgrid_sizing_data;
  if (subgrid_data && parent_sizing_data) {
    opt_subgrid_sizing_data =
        NGGridItemSizingData(*subgrid_data, parent_sizing_data->layout_data);
  }

  InitializeTrackCollection(kForColumns, opt_subgrid_sizing_data, &sizing_data,
                            /* force_sets_geometry_caching */ true);
  InitializeTrackCollection(kForRows, opt_subgrid_sizing_data, &sizing_data,
                            /* force_sets_geometry_caching */ true);

  // |AppendSubgriddedItems| rely on the cached placement data of a subgrid to
  // construct its grid items, so we need to build their subtrees beforehand.
  for (auto& grid_item : sizing_data.grid_items) {
    if (!grid_item.IsSubgrid())
      continue;

    grid_item.ComputeSetIndices(sizing_data.layout_data.Columns());
    grid_item.ComputeSetIndices(sizing_data.layout_data.Rows());

    LogicalRect unused_containing_grid_area;
    const auto space = CreateConstraintSpaceForLayout(
        grid_item, sizing_data.layout_data, &unused_containing_grid_area);

    const auto fragment_geometry = CalculateInitialFragmentGeometry(
        space, grid_item.node, /* break_token */ nullptr,
        /* is_intrinsic */ !space.IsFixedInlineSize());

    auto subgrid_algorithm =
        NGGridLayoutAlgorithm({grid_item.node, fragment_geometry, space});

    sizing_data.subtree_size += subgrid_algorithm.BuildGridSizingSubtree(
        sizing_tree, /* oof_children */ nullptr, &placement_data.line_resolver,
        &sizing_data, &grid_item);
  }

  node.AppendSubgriddedItems(&sizing_data.grid_items);

  // We need to recreate the track builder collections to ensure track coverage
  // for subgridded items; it would be ideal to have them accounted for already,
  // but we might need the track collections to compute a subgrid's automatic
  // repetitions, so we do this process twice to avoid a cyclic dependency.
  if (must_build_sizing_column_collection)
    BuildSizingCollection(kForColumns);
  if (must_build_sizing_row_collection)
    BuildSizingCollection(kForRows);

  return sizing_data.subtree_size;
}

NGGridSizingTree NGGridLayoutAlgorithm::BuildGridSizingTree(
    HeapVector<Member<LayoutBox>>* oof_children) const {
  NGGridSizingTree sizing_tree;

  const auto& constraint_space = ConstraintSpace();
  const auto* subgridded_columns = constraint_space.SubgriddedColumns();
  const auto* subgridded_rows = constraint_space.SubgriddedRows();

  // For subgrids, we build only the direct children and rely on the subgridded
  // tracks from the constraint space to build layout data. This isn't ideal for
  // the grid sizing tree approach, but we do it to keep passing subgrid tests.
  //
  // TODO(ethavar): Remove all of this redundant code.
  if (subgridded_columns || subgridded_rows) {
    const auto& node = Node();
    const auto& container_style = Style();
    auto& sizing_data = sizing_tree.CreateSizingData();

    bool has_nested_subgrid;
    sizing_data.grid_items = node.ConstructGridItems(
        node.CachedPlacementData(), oof_children, &has_nested_subgrid);
    if (has_nested_subgrid)
      node.AppendSubgriddedItems(&sizing_data.grid_items);

    auto BuildSizingCollection = [&](GridTrackSizingDirection track_direction) {
      NGGridRangeBuilder range_builder(
          container_style, node.CachedPlacementData(), track_direction);

      bool must_create_baselines = false;
      for (auto& grid_item : sizing_data.grid_items) {
        must_create_baselines |=
            grid_item.IsBaselineSpecifiedForDirection(track_direction);

        auto& range_indices = grid_item.RangeIndices(track_direction);
        range_builder.EnsureTrackCoverage(grid_item.StartLine(track_direction),
                                          grid_item.SpanSize(track_direction),
                                          &range_indices.begin,
                                          &range_indices.end);
      }
      sizing_data.layout_data.SetTrackCollection(
          std::make_unique<NGGridSizingTrackCollection>(
              range_builder.FinalizeRanges(), must_create_baselines,
              track_direction));
    };

    if (subgridded_columns) {
      sizing_data.layout_data.SetTrackCollection(
          std::make_unique<NGGridLayoutTrackCollection>(
              *subgridded_columns, BorderScrollbarPadding(),
              ComputeMarginsForSelf(constraint_space, container_style)));
    } else {
      BuildSizingCollection(kForColumns);
    }

    if (subgridded_rows) {
      sizing_data.layout_data.SetTrackCollection(
          std::make_unique<NGGridLayoutTrackCollection>(
              *subgridded_rows, BorderScrollbarPadding(),
              ComputeMarginsForSelf(constraint_space, container_style)));
    } else {
      BuildSizingCollection(kForRows);
    }
    return sizing_tree;
  }

  BuildGridSizingSubtree(&sizing_tree, oof_children);
  return sizing_tree;
}

NGGridSizingTree NGGridLayoutAlgorithm::BuildGridSizingTreeIgnoringChildren()
    const {
  NGGridSizingTree sizing_tree;
  BuildGridSizingSubtree(&sizing_tree, /* oof_children */ nullptr,
                         /* parent_line_resolver */ nullptr,
                         /* parent_sizing_data */ nullptr,
                         /* subgrid_data */ nullptr,
                         /* must_ignore_children */ true);
  return sizing_tree;
}

LayoutUnit NGGridLayoutAlgorithm::Baseline(
    const NGGridLayoutData& layout_data,
    const GridItemData& grid_item,
    GridTrackSizingDirection track_direction) const {
  // "If a box spans multiple shared alignment contexts, then it participates
  //  in first/last baseline alignment within its start-most/end-most shared
  //  alignment context along that axis"
  // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
  if (track_direction == kForColumns) {
    return (grid_item.column_baseline_group == BaselineGroup::kMajor)
               ? layout_data.Columns().MajorBaseline(
                     grid_item.column_set_indices.begin)
               : layout_data.Columns().MinorBaseline(
                     grid_item.column_set_indices.end - 1);
  } else {
    return (grid_item.row_baseline_group == BaselineGroup::kMajor)
               ? layout_data.Rows().MajorBaseline(
                     grid_item.row_set_indices.begin)
               : layout_data.Rows().MinorBaseline(
                     grid_item.row_set_indices.end - 1);
  }
}

namespace {

struct FirstSetGeometry {
  LayoutUnit start_offset;
  LayoutUnit gutter_size;
};

FirstSetGeometry ComputeFirstSetGeometry(
    const NGGridSizingTrackCollection& track_collection,
    const ComputedStyle& container_style,
    LayoutUnit available_size,
    LayoutUnit start_border_scrollbar_padding) {
  const bool is_for_columns = track_collection.Direction() == kForColumns;

  const auto& content_alignment = is_for_columns
                                      ? container_style.JustifyContent()
                                      : container_style.AlignContent();
  const auto overflow = content_alignment.Overflow();

  // Determining the free-space is typically unnecessary, i.e. if there is
  // default alignment. Only compute this on-demand.
  auto FreeSpace = [&]() -> LayoutUnit {
    LayoutUnit free_space = available_size - track_collection.TotalTrackSize();

    // If overflow is 'safe', make sure we don't overflow the 'start' edge
    // (potentially causing some data loss as the overflow is unreachable).
    return (overflow == OverflowAlignment::kSafe)
               ? free_space.ClampNegativeToZero()
               : free_space;
  };

  // The default alignment, perform adjustments on top of this.
  FirstSetGeometry geometry{start_border_scrollbar_padding,
                            track_collection.GutterSize()};

  // If we have an indefinite |available_size| we can't perform any alignment,
  // just return the default alignment.
  if (available_size == kIndefiniteSize)
    return geometry;

  // TODO(ikilpatrick): 'space-between', 'space-around', and 'space-evenly' all
  // divide by the free-space, and may have a non-zero modulo. Investigate if
  // this should be distributed between the tracks.
  switch (content_alignment.Distribution()) {
    case ContentDistributionType::kSpaceBetween: {
      // Default behavior for 'space-between' is to start align content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (track_count < 2 || free_space < LayoutUnit())
        return geometry;

      geometry.gutter_size += free_space / (track_count - 1);
      return geometry;
    }
    case ContentDistributionType::kSpaceAround: {
      // Default behavior for 'space-around' is to center content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (track_count < 1 || free_space < LayoutUnit()) {
        geometry.start_offset += free_space / 2;
        return geometry;
      }

      LayoutUnit track_space = free_space / track_count;
      geometry.start_offset += track_space / 2;
      geometry.gutter_size += track_space;
      return geometry;
    }
    case ContentDistributionType::kSpaceEvenly: {
      // Default behavior for 'space-evenly' is to center content.
      const wtf_size_t track_count = track_collection.NonCollapsedTrackCount();
      const LayoutUnit free_space = FreeSpace();
      if (free_space < LayoutUnit()) {
        geometry.start_offset += free_space / 2;
        return geometry;
      }

      LayoutUnit track_space = free_space / (track_count + 1);
      geometry.start_offset += track_space;
      geometry.gutter_size += track_space;
      return geometry;
    }
    case ContentDistributionType::kStretch:
    case ContentDistributionType::kDefault:
      break;
  }

  switch (content_alignment.GetPosition()) {
    case ContentPosition::kLeft: {
      DCHECK(is_for_columns);
      if (IsLtr(container_style.Direction()))
        return geometry;

      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kRight: {
      DCHECK(is_for_columns);
      if (IsRtl(container_style.Direction()))
        return geometry;

      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kCenter: {
      geometry.start_offset += FreeSpace() / 2;
      return geometry;
    }
    case ContentPosition::kEnd:
    case ContentPosition::kFlexEnd: {
      geometry.start_offset += FreeSpace();
      return geometry;
    }
    case ContentPosition::kStart:
    case ContentPosition::kFlexStart:
    case ContentPosition::kNormal:
    case ContentPosition::kBaseline:
    case ContentPosition::kLastBaseline:
      return geometry;
  }
}

}  // namespace

void NGGridLayoutAlgorithm::ComputeGridGeometry(
    NGGridSizingTree* grid_sizing_tree,
    LayoutUnit* intrinsic_block_size) {
  DCHECK(grid_sizing_tree && intrinsic_block_size);

  const auto& node = Node();
  const auto& container_style = Style();
  const auto& constraint_space = ConstraintSpace();
  const auto& border_scrollbar_padding = BorderScrollbarPadding();

  DCHECK_NE(grid_available_size_.inline_size, kIndefiniteSize);

  auto& root_sizing_data = (*grid_sizing_tree)[0];
  auto& grid_items = root_sizing_data.grid_items;
  auto& layout_data = root_sizing_data.layout_data;

  InitializeTrackCollections(grid_sizing_tree);
  CacheGridItemsProperties(layout_data.Columns(), &grid_items);
  CacheGridItemsProperties(layout_data.Rows(), &grid_items);

  ComputeUsedTrackSizes(layout_data, SizingConstraint::kLayout, &grid_items,
                        &layout_data.Rows(),
                        /* needs_additional_pass */ nullptr,
                        /* only_initialize_track_sizes */ true);

  bool needs_additional_pass = false;
  ComputeUsedTrackSizes(layout_data, SizingConstraint::kLayout, &grid_items,
                        &layout_data.Columns(), &needs_additional_pass);
  ComputeUsedTrackSizes(layout_data, SizingConstraint::kLayout, &grid_items,
                        &layout_data.Rows(), &needs_additional_pass);

  if (contain_intrinsic_block_size_) {
    *intrinsic_block_size = *contain_intrinsic_block_size_;
  } else {
    *intrinsic_block_size = layout_data.Rows().ComputeSetSpanSize() +
                            border_scrollbar_padding.BlockSum();

    // TODO(layout-dev): This isn't great but matches legacy. Ideally this
    // would only apply when we have only flexible track(s).
    if (grid_items.IsEmpty() && node.HasLineIfEmpty()) {
      *intrinsic_block_size = std::max(
          *intrinsic_block_size, border_scrollbar_padding.BlockSum() +
                                     node.EmptyLineBlockSize(BreakToken()));
    }

    *intrinsic_block_size = ClampIntrinsicBlockSize(
        constraint_space, node, BreakToken(), border_scrollbar_padding,
        *intrinsic_block_size);
  }

  if (layout_data.Rows().IsForSizing() &&
      grid_available_size_.block_size == kIndefiniteSize) {
    const auto block_size = ComputeBlockSizeForFragment(
        constraint_space, container_style, BorderPadding(),
        *intrinsic_block_size, container_builder_.InlineSize());

    DCHECK_NE(block_size, kIndefiniteSize);

    grid_available_size_.block_size = grid_min_available_size_.block_size =
        grid_max_available_size_.block_size =
            (block_size - border_scrollbar_padding.BlockSum())
                .ClampNegativeToZero();

    // If we have any rows, gaps which will resolve differently if we have a
    // definite |grid_available_size_| re-compute the grid using the
    // |block_size| calculated above.
    needs_additional_pass |= (container_style.RowGap() &&
                              container_style.RowGap()->IsPercentOrCalc()) ||
                             layout_data.Rows().IsDependentOnAvailableSize();

    // If we are a flex-item, we may have our initial block-size forced to be
    // indefinite, however grid layout always re-computes the grid using the
    // final "used" block-size.
    // We can detect this case by checking if computing our block-size (with an
    // indefinite intrinsic size) is definite.
    //
    // TODO(layout-dev): A small optimization here would be to do this only if
    // we have 'auto' tracks which fill the remaining available space.
    if (constraint_space.IsInitialBlockSizeIndefinite()) {
      needs_additional_pass |=
          ComputeBlockSizeForFragment(
              constraint_space, container_style, BorderPadding(),
              /* intrinsic_block_size */ kIndefiniteSize,
              container_builder_.InlineSize()) != kIndefiniteSize;
    }

    // After resolving the block-size, if we don't need to rerun the track
    // sizing algorithm, simply apply any content alignment to its rows.
    if (!needs_additional_pass &&
        container_style.AlignContent() !=
            ComputedStyleInitialValues::InitialAlignContent()) {
      auto& track_collection = layout_data.SizingCollection(kForRows);

      // Re-compute the row geometry now that we resolved the available block
      // size. "align-content: space-evenly", etc, require the resolved size.
      auto first_set_geometry = ComputeFirstSetGeometry(
          track_collection, container_style, grid_available_size_.block_size,
          border_scrollbar_padding.block_start);

      track_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                            first_set_geometry.gutter_size);
    }
  }

  if (needs_additional_pass) {
    InitializeTrackCollections(grid_sizing_tree);
    CacheGridItemsProperties(layout_data.Columns(), &grid_items);
    CacheGridItemsProperties(layout_data.Rows(), &grid_items);

    ComputeUsedTrackSizes(layout_data, SizingConstraint::kLayout, &grid_items,
                          &layout_data.Columns());
    ComputeUsedTrackSizes(layout_data, SizingConstraint::kLayout, &grid_items,
                          &layout_data.Rows());
  }

  // Calculate final alignment baselines for grid item layout.
  if (layout_data.Columns().IsForSizing() &&
      layout_data.Columns().HasBaselines()) {
    CalculateAlignmentBaselines(layout_data, SizingConstraint::kLayout,
                                &grid_items,
                                &layout_data.SizingCollection(kForColumns));
  }

  if (layout_data.Rows().IsForSizing() && layout_data.Rows().HasBaselines()) {
    CalculateAlignmentBaselines(layout_data, SizingConstraint::kLayout,
                                &grid_items,
                                &layout_data.SizingCollection(kForRows));
  }
}

LayoutUnit NGGridLayoutAlgorithm::ComputeIntrinsicBlockSizeIgnoringChildren()
    const {
  const auto& node = Node();
  const LayoutUnit override_intrinsic_block_size =
      node.OverrideIntrinsicContentBlockSize();
  DCHECK(node.ShouldApplyBlockSizeContainment());

  // First check 'contain-intrinsic-size'.
  if (override_intrinsic_block_size != kIndefiniteSize)
    return BorderScrollbarPadding().BlockSum() + override_intrinsic_block_size;

  auto grid_sizing_tree = BuildGridSizingTreeIgnoringChildren();
  auto& grid_items = grid_sizing_tree[0].grid_items;
  auto& layout_data = grid_sizing_tree[0].layout_data;

  InitializeTrackCollections(&grid_sizing_tree);
  ComputeUsedTrackSizes(layout_data, SizingConstraint::kLayout, &grid_items,
                        &layout_data.Rows());

  return layout_data.Rows().ComputeSetSpanSize() +
         BorderScrollbarPadding().BlockSum();
}

namespace {

const NGLayoutResult* LayoutGridItemForMeasure(
    const GridItemData& grid_item,
    const NGConstraintSpace& constraint_space,
    SizingConstraint sizing_constraint) {
  const auto& node = grid_item.node;

  // Disable side effects during MinMax computation to avoid potential "MinMax
  // after layout" crashes. This is not necessary during the layout pass, and
  // would have a negative impact on performance if used there.
  //
  // TODO(ikilpatrick): For subgrid, ideally we don't want to disable side
  // effects as it may impact performance significantly; this issue can be
  // avoided by introducing additional cache slots (see crbug.com/1272533).
  absl::optional<NGDisableSideEffectsScope> disable_side_effects;
  if (!node.GetLayoutBox()->NeedsLayout() &&
      (sizing_constraint != SizingConstraint::kLayout ||
       grid_item.is_subgridded_to_parent_grid)) {
    disable_side_effects.emplace();
  }
  return node.Layout(constraint_space);
}

}  // namespace

LayoutUnit NGGridLayoutAlgorithm::GetLogicalBaseline(
    const NGBoxFragment& baseline_fragment,
    const bool is_last_baseline) const {
  const auto font_baseline = Style().GetFontBaseline();
  return is_last_baseline
             ? baseline_fragment.BlockSize() -
                   baseline_fragment.LastBaselineOrSynthesize(font_baseline)
             : baseline_fragment.FirstBaselineOrSynthesize(font_baseline);
}

LayoutUnit NGGridLayoutAlgorithm::GetSynthesizedLogicalBaseline(
    const LayoutUnit block_size,
    const bool is_flipped_lines,
    const bool is_last_baseline) const {
  const auto font_baseline = Style().GetFontBaseline();
  const auto synthesized_baseline = NGBoxFragment::SynthesizedBaseline(
      font_baseline, is_flipped_lines, block_size);
  return is_last_baseline ? block_size - synthesized_baseline
                          : synthesized_baseline;
}

LayoutUnit NGGridLayoutAlgorithm::ContributionSizeForGridItem(
    const NGGridLayoutData& layout_data,
    GridItemContributionType contribution_type,
    GridTrackSizingDirection track_direction,
    SizingConstraint sizing_constraint,
    GridItemData* grid_item) const {
  DCHECK(grid_item);

  // From https://drafts.csswg.org/css-grid-2/#subgrid-size-contribution:
  //   The subgrid itself [...] acts as if it was completely empty for track
  //   sizing purposes in the subgridded dimension.
  if (!grid_item->IsConsideredForSizing(track_direction))
    return LayoutUnit();

  const auto& node = grid_item->node;
  const auto& item_style = node.Style();

  const bool is_for_columns = track_direction == kForColumns;
  const bool is_parallel_with_track_direction =
      is_for_columns == grid_item->is_parallel_with_root_grid;

  // TODO(ikilpatrick): We'll need to record if any child used an indefinite
  // size for its contribution, such that we can then do the 2nd pass on the
  // track-sizing algorithm.
  const auto space =
      CreateConstraintSpaceForMeasure(*grid_item, layout_data, track_direction);
  const auto margins = ComputeMarginsFor(space, item_style, ConstraintSpace());

  LayoutUnit baseline_shim;
  auto CalculateBaselineShim = [&](const LayoutUnit baseline) -> void {
    const LayoutUnit track_baseline =
        Baseline(layout_data, *grid_item, track_direction);
    if (track_baseline == LayoutUnit::Min())
      return;

    // Determine the delta between the baselines.
    baseline_shim = track_baseline - baseline;

    // Subtract out the start margin so it doesn't get added a second time at
    // the end of |NGGridLayoutAlgorithm::ContributionSizeForGridItem|.
    baseline_shim -=
        ComputeMarginsFor(space, item_style,
                          grid_item->BaselineWritingDirection(track_direction))
            .block_start;
  };

  auto MinOrMaxContentSize = [&](bool is_min_size) -> LayoutUnit {
    const auto result = ComputeMinAndMaxContentContributionForSelf(node, space);

    // The min/max contribution may depend on the block-size of the grid-area:
    // <div style="display: inline-grid; grid-template-columns: auto auto;">
    //   <div style="height: 100%">
    //     <img style="height: 50%;" />
    //   </div>
    //   <div>
    //     <div style="height: 100px;"></div>
    //   </div>
    // </div>
    // Mark ourselves as requiring an additional pass to re-resolve the column
    // tracks for this case.
    if (grid_item->is_parallel_with_root_grid &&
        result.depends_on_block_constraints) {
      grid_item->is_sizing_dependent_on_block_size = true;
    }

    const LayoutUnit content_size =
        is_min_size ? result.sizes.min_size : result.sizes.max_size;

    if (grid_item->IsBaselineAlignedForDirection(track_direction)) {
      CalculateBaselineShim(GetSynthesizedLogicalBaseline(
          content_size,
          grid_item->BaselineWritingDirection(track_direction).IsFlippedLines(),
          grid_item->IsLastBaselineSpecifiedForDirection(track_direction)));
    }
    return content_size + baseline_shim;
  };

  auto MinContentSize = [&]() -> LayoutUnit {
    return MinOrMaxContentSize(/* is_min_size */ true);
  };
  auto MaxContentSize = [&]() -> LayoutUnit {
    return MinOrMaxContentSize(/* is_min_size */ false);
  };

  // This function will determine the correct block-size of a grid-item.
  // TODO(ikilpatrick): This should try and skip layout when possible. Notes:
  //  - We'll need to do a full layout for tables.
  //  - We'll need special logic for replaced elements.
  //  - We'll need to respect the aspect-ratio when appropriate.
  auto BlockContributionSize = [&]() -> LayoutUnit {
    DCHECK(!is_parallel_with_track_direction);

    // TODO(ikilpatrick): This check is potentially too broad, i.e. a fixed
    // inline size with no %-padding doesn't need the additional pass.
    if (is_for_columns)
      grid_item->is_sizing_dependent_on_block_size = true;

    const NGLayoutResult* result = nullptr;
    if (space.AvailableSize().inline_size == kIndefiniteSize) {
      // The only case where we will have an indefinite block size is for the
      // first column resolution step; after that we will always have the used
      // sizes of the previous step for the orthogonal direction.
      DCHECK(is_for_columns);

      // If we are orthogonal grid-item, resolving against an indefinite size,
      // set our inline-size to our max content-contribution size.
      const auto fallback_space = CreateConstraintSpaceForMeasure(
          *grid_item, layout_data, track_direction,
          /* opt_fixed_block_size */ MaxContentSize());

      result = LayoutGridItemForMeasure(*grid_item, fallback_space,
                                        sizing_constraint);
    } else {
      result = LayoutGridItemForMeasure(*grid_item, space, sizing_constraint);
    }

    NGBoxFragment baseline_fragment(
        grid_item->BaselineWritingDirection(track_direction),
        To<NGPhysicalBoxFragment>(result->PhysicalFragment()));

    if (grid_item->IsBaselineAlignedForDirection(track_direction)) {
      CalculateBaselineShim(GetLogicalBaseline(
          baseline_fragment,
          grid_item->IsLastBaselineSpecifiedForDirection(track_direction)));
    }

    return baseline_fragment.BlockSize() + baseline_shim;
  };

  const LayoutUnit margin_sum =
      is_for_columns ? margins.InlineSum() : margins.BlockSum();

  LayoutUnit contribution;
  switch (contribution_type) {
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForIntrinsicMaximums:
      contribution = is_parallel_with_track_direction ? MinContentSize()
                                                      : BlockContributionSize();
      break;
    case GridItemContributionType::kForIntrinsicMinimums: {
      // TODO(ikilpatrick): All of the below is incorrect for replaced elements.
      const Length& main_length = is_parallel_with_track_direction
                                      ? item_style.LogicalWidth()
                                      : item_style.LogicalHeight();
      const Length& min_length = is_parallel_with_track_direction
                                     ? item_style.LogicalMinWidth()
                                     : item_style.LogicalMinHeight();

      // We could be clever is and make this an if-stmt, but each type has
      // subtle consequences. This forces us in the future when we add a new
      // length type to consider what the best thing is for grid.
      switch (main_length.GetType()) {
        case Length::kAuto:
        case Length::kFitContent:
        case Length::kFillAvailable:
        case Length::kPercent:
        case Length::kCalculated: {
          const NGBoxStrut border_padding =
              ComputeBorders(space, node) + ComputePadding(space, item_style);

          // All of the above lengths are considered 'auto' if we are querying a
          // minimum contribution. They all require definite track sizes to
          // determine their final size.
          //
          // From https://drafts.csswg.org/css-grid/#min-size-auto:
          //   To provide a more reasonable default minimum size for grid items,
          //   the used value of its automatic minimum size in a given axis is
          //   the content-based minimum size if all of the following are true:
          //     - it is not a scroll container
          //     - it spans at least one track in that axis whose min track
          //     sizing function is 'auto'
          //     - if it spans more than one track in that axis, none of those
          //     tracks are flexible
          //   Otherwise, the automatic minimum size is zero, as usual.
          //
          // Start by resolving the cases where |min_length| is non-auto or its
          // automatic minimum size should be zero.
          if (!min_length.IsAuto() || item_style.IsScrollContainer() ||
              !grid_item->IsSpanningAutoMinimumTrack(track_direction) ||
              (grid_item->IsSpanningFlexibleTrack(track_direction) &&
               grid_item->SpanSize(track_direction) > 1)) {
            // TODO(ikilpatrick): This block needs to respect the aspect-ratio,
            // and apply the transferred min/max sizes when appropriate. We do
            // this sometimes elsewhere so should unify and simplify this code.
            if (is_parallel_with_track_direction) {
              auto MinMaxSizesFunc =
                  [&](MinMaxSizesType type) -> MinMaxSizesResult {
                return node.ComputeMinMaxSizes(item_style.GetWritingMode(),
                                               type, space);
              };

              contribution = ResolveMinInlineLength(
                  space, item_style, border_padding, MinMaxSizesFunc,
                  item_style.LogicalMinWidth());
            } else {
              contribution =
                  ResolveMinBlockLength(space, item_style, border_padding,
                                        item_style.LogicalMinHeight());
            }
            break;
          }

          // Resolve the content-based minimum size.
          contribution = is_parallel_with_track_direction
                             ? MinContentSize()
                             : BlockContributionSize();

          const auto& set_indices = grid_item->SetIndices(track_direction);
          const auto& track_collection =
              is_for_columns ? layout_data.Columns() : layout_data.Rows();

          auto spanned_tracks_definite_max_size =
              track_collection.ComputeSetSpanSize(set_indices.begin,
                                                  set_indices.end);

          if (spanned_tracks_definite_max_size != kIndefiniteSize) {
            // Further clamp the minimum size to less than or equal to the
            // stretch fit into the grid area’s maximum size in that dimension,
            // as represented by the sum of those grid tracks’ max track sizing
            // functions plus any intervening fixed gutters.
            const auto border_padding_sum = is_parallel_with_track_direction
                                                ? border_padding.InlineSum()
                                                : border_padding.BlockSum();
            DCHECK_GE(contribution, baseline_shim + border_padding_sum);

            // The stretch fit into a given size is that size, minus the box’s
            // computed margins, border, and padding in the given dimension,
            // flooring at zero so that the inner size is not negative.
            spanned_tracks_definite_max_size =
                (spanned_tracks_definite_max_size - baseline_shim - margin_sum -
                 border_padding_sum)
                    .ClampNegativeToZero();

            // Add the baseline shim, border, and padding (margins will be added
            // later) back to the contribution, since we don't want the outer
            // size of the minimum size to overflow its grid area; these are
            // already accounted for in the current value of |contribution|.
            contribution =
                std::min(contribution, spanned_tracks_definite_max_size +
                                           baseline_shim + border_padding_sum);
          }
          break;
        }
        case Length::kMinContent:
        case Length::kMaxContent:
        case Length::kFixed: {
          // All of the above lengths are "definite" (non-auto), and don't need
          // the special min-size treatment above. (They will all end up being
          // the specified size).
          if (is_parallel_with_track_direction) {
            contribution = main_length.IsMaxContent() ? MaxContentSize()
                                                      : MinContentSize();
          } else {
            contribution = BlockContributionSize();
          }
          break;
        }
        case Length::kMinIntrinsic:
        case Length::kDeviceWidth:
        case Length::kDeviceHeight:
        case Length::kExtendToZoom:
        case Length::kContent:
        case Length::kNone:
          NOTREACHED();
          break;
      }
      break;
    }
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForMaxContentMaximums:
      contribution = is_parallel_with_track_direction ? MaxContentSize()
                                                      : BlockContributionSize();
      break;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED() << "|kForFreeSpace| should only be used to distribute extra "
                      "space in maximize tracks and stretch auto tracks steps.";
      break;
  }
  return (contribution + margin_sum).ClampNegativeToZero();
}

// https://drafts.csswg.org/css-grid-2/#auto-repeat
wtf_size_t NGGridLayoutAlgorithm::ComputeAutomaticRepetitions(
    GridTrackSizingDirection track_direction) const {
  const bool is_for_columns = track_direction == kForColumns;
  const auto& track_list = is_for_columns
                               ? Style().GridTemplateColumns().TrackList()
                               : Style().GridTemplateRows().TrackList();

  if (!track_list.HasAutoRepeater())
    return 0;

  LayoutUnit available_size = is_for_columns ? grid_available_size_.inline_size
                                             : grid_available_size_.block_size;
  LayoutUnit max_available_size = available_size;

  if (available_size == kIndefiniteSize) {
    max_available_size = is_for_columns ? grid_max_available_size_.inline_size
                                        : grid_max_available_size_.block_size;
    available_size = is_for_columns ? grid_min_available_size_.inline_size
                                    : grid_min_available_size_.block_size;
  }

  LayoutUnit auto_repeater_size;
  LayoutUnit non_auto_specified_size;
  const LayoutUnit gutter_size = GutterSize(track_direction);

  for (wtf_size_t repeater_index = 0;
       repeater_index < track_list.RepeaterCount(); ++repeater_index) {
    const auto repeat_type = track_list.RepeatType(repeater_index);
    const bool is_auto_repeater =
        repeat_type == NGGridTrackRepeater::kAutoFill ||
        repeat_type == NGGridTrackRepeater::kAutoFit;

    LayoutUnit repeater_size;
    const wtf_size_t repeater_track_count =
        track_list.RepeatSize(repeater_index);

    for (wtf_size_t i = 0; i < repeater_track_count; ++i) {
      const auto& track_size = track_list.RepeatTrackSize(repeater_index, i);

      absl::optional<LayoutUnit> fixed_min_track_breadth;
      if (track_size.HasFixedMinTrackBreadth()) {
        fixed_min_track_breadth = MinimumValueForLength(
            track_size.MinTrackBreadth().length(), available_size);
      }

      absl::optional<LayoutUnit> fixed_max_track_breadth;
      if (track_size.HasFixedMaxTrackBreadth()) {
        fixed_max_track_breadth = MinimumValueForLength(
            track_size.MaxTrackBreadth().length(), available_size);
      }

      LayoutUnit track_contribution;
      if (fixed_max_track_breadth && fixed_min_track_breadth) {
        track_contribution =
            std::max(*fixed_max_track_breadth, *fixed_min_track_breadth);
      } else if (fixed_max_track_breadth) {
        track_contribution = *fixed_max_track_breadth;
      } else if (fixed_min_track_breadth) {
        track_contribution = *fixed_min_track_breadth;
      }

      // For the purpose of finding the number of auto-repeated tracks in a
      // standalone axis, the UA must floor the track size to a UA-specified
      // value to avoid division by zero. It is suggested that this floor be
      // 1px.
      if (is_auto_repeater)
        track_contribution = std::max(LayoutUnit(1), track_contribution);

      repeater_size += track_contribution + gutter_size;
    }

    if (!is_auto_repeater) {
      non_auto_specified_size +=
          repeater_size * track_list.RepeatCount(repeater_index, 0);
    } else {
      DCHECK_EQ(0, auto_repeater_size);
      auto_repeater_size = repeater_size;
    }
  }

  DCHECK_GT(auto_repeater_size, 0);

  // We can compute the number of repetitions by satisfying the expression
  // below. Notice that we subtract an extra |gutter_size| since it was included
  // in the contribution for the last set in the collection.
  //   available_size =
  //       (repetitions * auto_repeater_size) +
  //       non_auto_specified_size - gutter_size
  //
  // Solving for repetitions we have:
  //   repetitions =
  //       available_size - (non_auto_specified_size - gutter_size) /
  //       auto_repeater_size
  non_auto_specified_size -= gutter_size;

  // First we want to allow as many repetitions as possible, up to the max
  // available-size. Only do this if we have a definite max-size.
  // If a definite available-size was provided, |max_available_size| will be
  // set to that value.
  if (max_available_size != LayoutUnit::Max()) {
    // Use floor to ensure that the auto repeater sizes goes under the max
    // available-size.
    const int count = FloorToInt(
        (max_available_size - non_auto_specified_size) / auto_repeater_size);
    return (count <= 0) ? 1u : count;
  }

  // Next, consider the min available-size, which was already used to floor
  // |available_size|. Use ceil to ensure that the auto repeater size goes
  // above this min available-size.
  const int count = CeilToInt((available_size - non_auto_specified_size) /
                              auto_repeater_size);
  return (count <= 0) ? 1u : count;
}

void NGGridLayoutAlgorithm::CalculateAlignmentBaselines(
    const NGGridLayoutData& layout_data,
    SizingConstraint sizing_constraint,
    GridItems* grid_items,
    NGGridSizingTrackCollection* track_collection,
    bool* needs_additional_pass) const {
  DCHECK(grid_items && track_collection);
  const auto track_direction = track_collection->Direction();

  track_collection->ResetBaselines();

  for (auto& grid_item : *grid_items) {
    if (!grid_item.IsBaselineSpecifiedForDirection(track_direction))
      continue;

    LogicalRect unused_grid_area;
    const auto space = CreateConstraintSpaceForLayout(grid_item, layout_data,
                                                      &unused_grid_area);

    // We cannot apply some of the baseline alignment rules for synthesized
    // baselines until layout has been performed. However, layout cannot
    // be performed in certain scenarios. So force an additional pass in
    // these cases and skip layout for now.
    const auto& item_style = grid_item.node.Style();
    if (InlineLengthUnresolvable(space, item_style.LogicalWidth()) ||
        InlineLengthUnresolvable(space, item_style.LogicalMinWidth()) ||
        InlineLengthUnresolvable(space, item_style.LogicalMaxWidth())) {
      if (needs_additional_pass)
        *needs_additional_pass = true;
      continue;
    }

    const auto* result =
        LayoutGridItemForMeasure(grid_item, space, sizing_constraint);

    const auto baseline_writing_direction =
        grid_item.BaselineWritingDirection(track_direction);
    const NGBoxFragment baseline_fragment(
        baseline_writing_direction,
        To<NGPhysicalBoxFragment>(result->PhysicalFragment()));

    const bool has_synthesized_baseline =
        !baseline_fragment.FirstBaseline().has_value();
    grid_item.SetAlignmentFallback(track_direction, has_synthesized_baseline);

    if (!grid_item.IsBaselineAlignedForDirection(track_direction))
      continue;

    const auto margins =
        ComputeMarginsFor(space, item_style, baseline_writing_direction);
    const bool is_last_baseline =
        grid_item.IsLastBaselineSpecifiedForDirection(track_direction);
    const LayoutUnit baseline =
        (is_last_baseline ? margins.block_end : margins.block_start) +
        GetLogicalBaseline(baseline_fragment, is_last_baseline);

    // "If a box spans multiple shared alignment contexts, then it participates
    //  in first/last baseline alignment within its start-most/end-most shared
    //  alignment context along that axis"
    // https://www.w3.org/TR/css-align-3/#baseline-sharing-group
    if (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor) {
      track_collection->SetMajorBaseline(
          grid_item.SetIndices(track_direction).begin, baseline);
    } else {
      track_collection->SetMinorBaseline(
          grid_item.SetIndices(track_direction).end - 1, baseline);
    }
  }
}

void NGGridLayoutAlgorithm::InitializeTrackCollection(
    GridTrackSizingDirection track_direction,
    NGSubgridSizingData opt_subgrid_sizing_data,
    NGGridSizingData* sizing_data,
    bool force_sets_geometry_caching) const {
  auto& layout_data = sizing_data->layout_data;

  if (layout_data.HasSubgriddedAxis(track_direction)) {
    // TODO(ethavar): We need to remove this and let the DCHECK catch when we
    // don't have subgrid sizing data, but it keeps the subgrid tests passing.
    if (!opt_subgrid_sizing_data) {
      return;
    }

    // If we don't have a sizing collection for this axis, then we're in a
    // subgrid that must inherit the track collection of its parent grid.
    DCHECK(opt_subgrid_sizing_data);
    layout_data.SetTrackCollection(
        opt_subgrid_sizing_data->CreateSubgridCollection(track_direction));
    return;
  }

  const LayoutUnit available_size = (track_direction == kForColumns)
                                        ? grid_available_size_.inline_size
                                        : grid_available_size_.block_size;
  auto& track_collection = layout_data.SizingCollection(track_direction);
  track_collection.BuildSets(Style(), available_size);

  // Caching the definite sets geometry is useful to correctly determine the
  // available space for subgrids. However, we may not want to set this cache if
  // the grid has a cache already or if it doesn't have nested subgrids.
  if (!force_sets_geometry_caching &&
      (track_collection.HasCachedSetsGeometry() ||
       sizing_data->subtree_size == 1)) {
    return;
  }

  track_collection.InitializeSets(available_size);
  track_collection.SetGutterSize(GutterSize(track_direction));
  track_collection.CacheDefiniteSetsGeometry(available_size);
}

void NGGridLayoutAlgorithm::InitializeTrackCollections(
    NGGridSizingTree* sizing_tree,
    wtf_size_t current_grid_index,
    NGSubgridSizingData opt_subgrid_sizing_data) const {
  DCHECK(sizing_tree && current_grid_index < sizing_tree->Size());

  auto& sizing_data = (*sizing_tree)[current_grid_index];
  InitializeTrackCollection(kForColumns, opt_subgrid_sizing_data, &sizing_data);
  InitializeTrackCollection(kForRows, opt_subgrid_sizing_data, &sizing_data);
}

namespace {

struct BlockSizeDependentGridItem {
  GridItemIndices row_set_indices;
  LayoutUnit cached_block_size;
};

Vector<BlockSizeDependentGridItem> BlockSizeDependentGridItems(
    const GridItems& grid_items,
    const NGGridSizingTrackCollection& track_collection) {
  DCHECK_EQ(track_collection.Direction(), kForRows);

  Vector<BlockSizeDependentGridItem> dependent_items;
  dependent_items.ReserveInitialCapacity(grid_items.Size());

  for (const auto& grid_item : grid_items) {
    if (!grid_item.is_sizing_dependent_on_block_size)
      continue;

    const auto& set_indices = grid_item.SetIndices(kForRows);
    BlockSizeDependentGridItem dependent_item = {
        set_indices, track_collection.ComputeSetSpanSize(set_indices.begin,
                                                         set_indices.end)};
    dependent_items.emplace_back(std::move(dependent_item));
  }
  return dependent_items;
}

bool MayChangeBlockSizeDependentGridItemContributions(
    const Vector<BlockSizeDependentGridItem>& dependent_items,
    const NGGridSizingTrackCollection& track_collection) {
  DCHECK_EQ(track_collection.Direction(), kForRows);

  for (const auto& grid_item : dependent_items) {
    const LayoutUnit block_size = track_collection.ComputeSetSpanSize(
        grid_item.row_set_indices.begin, grid_item.row_set_indices.end);

    DCHECK_NE(block_size, kIndefiniteSize);
    if (block_size != grid_item.cached_block_size)
      return true;
  }
  return false;
}

}  // namespace

// https://drafts.csswg.org/css-grid-2/#algo-track-sizing
void NGGridLayoutAlgorithm::ComputeUsedTrackSizes(
    const NGGridLayoutData& layout_data,
    SizingConstraint sizing_constraint,
    GridItems* grid_items,
    NGGridLayoutTrackCollection* track_collection,
    bool* needs_additional_pass,
    bool only_initialize_track_sizes) const {
  DCHECK(grid_items && track_collection);

  // The track collection is not being sized by this grid container.
  if (!track_collection->IsForSizing())
    return;

  auto& sizing_collection = To<NGGridSizingTrackCollection>(*track_collection);
  const auto track_direction = sizing_collection.Direction();
  const bool is_for_columns = track_direction == kForColumns;

  LayoutUnit available_size = is_for_columns ? grid_available_size_.inline_size
                                             : grid_available_size_.block_size;
  LayoutUnit start_border_scrollbar_padding =
      is_for_columns ? BorderScrollbarPadding().inline_start
                     : BorderScrollbarPadding().block_start;

  sizing_collection.InitializeSets(available_size);
  sizing_collection.SetGutterSize(GutterSize(track_direction));

  // If all of our tracks have a definite size upfront, we can use the current
  // set sizes as the used track sizes (applying alignment, if present).
  if (sizing_collection.IsSpanningOnlyDefiniteTracks()) {
    auto first_set_geometry =
        ComputeFirstSetGeometry(sizing_collection, Style(), available_size,
                                start_border_scrollbar_padding);
    sizing_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                           first_set_geometry.gutter_size);
    return;
  }

  sizing_collection.CacheInitializedSetsGeometry(
      start_border_scrollbar_padding);

  if (only_initialize_track_sizes)
    return;

  // Cache baselines, as these contributions can influence track sizing.
  if (sizing_collection.HasBaselines()) {
    CalculateAlignmentBaselines(layout_data, sizing_constraint, grid_items,
                                &sizing_collection, needs_additional_pass);
  }

  // 2. Resolve intrinsic track sizing functions to absolute lengths.
  if (sizing_collection.HasIntrinsicTrack()) {
    ResolveIntrinsicTrackSizes(layout_data, sizing_constraint,
                               &sizing_collection, grid_items);
  }

  // If any track still has an infinite growth limit (i.e. it had no items
  // placed in it), set its growth limit to its base size before maximizing.
  sizing_collection.SetIndefiniteGrowthLimitsToBaseSize();

  // 3. If the free space is positive, distribute it equally to the base sizes
  // of all tracks, freezing tracks as they reach their growth limits (and
  // continuing to grow the unfrozen tracks as needed).
  MaximizeTracks(sizing_constraint, &sizing_collection);

  // 4. This step sizes flexible tracks using the largest value it can assign to
  // an 'fr' without exceeding the available space.
  if (sizing_collection.HasFlexibleTrack()) {
    ExpandFlexibleTracks(layout_data, sizing_constraint, &sizing_collection,
                         grid_items);
  }

  // 5. Stretch tracks with an 'auto' max track sizing function.
  StretchAutoTracks(sizing_constraint, &sizing_collection);

  // After computing row sizes, if we're still trying to determine whether we
  // need to perform and additional pass, check if there is a grid item whose
  // contributions relied on the available block size and may be changed.
  const bool needs_to_check_block_size_dependent_grid_items =
      !is_for_columns && needs_additional_pass && !(*needs_additional_pass);

  Vector<BlockSizeDependentGridItem> block_size_dependent_grid_items;
  if (needs_to_check_block_size_dependent_grid_items) {
    block_size_dependent_grid_items =
        BlockSizeDependentGridItems(*grid_items, sizing_collection);
  }

  auto first_set_geometry =
      ComputeFirstSetGeometry(sizing_collection, Style(), available_size,
                              start_border_scrollbar_padding);

  sizing_collection.FinalizeSetsGeometry(first_set_geometry.start_offset,
                                         first_set_geometry.gutter_size);

  if (needs_to_check_block_size_dependent_grid_items) {
    *needs_additional_pass = MayChangeBlockSizeDependentGridItemContributions(
        block_size_dependent_grid_items, sizing_collection);
  }
}

// Helpers for the track sizing algorithm.
namespace {

using ClampedDouble = base::ClampedNumeric<double>;
using SetIterator = NGGridSizingTrackCollection::SetIterator;

const double kDoubleEpsilon = std::numeric_limits<float>::epsilon();

SetIterator GetSetIteratorForItem(
    const GridItemData& grid_item,
    NGGridSizingTrackCollection& track_collection) {
  const auto& set_indices = grid_item.SetIndices(track_collection.Direction());
  return track_collection.GetSetIterator(set_indices.begin, set_indices.end);
}

LayoutUnit DefiniteGrowthLimit(const NGGridSet& set) {
  LayoutUnit growth_limit = set.GrowthLimit();
  // For infinite growth limits, substitute the track’s base size.
  return (growth_limit == kIndefiniteSize) ? set.BaseSize() : growth_limit;
}

// Returns the corresponding size to be increased by accommodating a grid item's
// contribution; for intrinsic min track sizing functions, return the base size.
// For intrinsic max track sizing functions, return the growth limit.
LayoutUnit AffectedSizeForContribution(
    const NGGridSet& set,
    GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      return set.BaseSize();
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      return DefiniteGrowthLimit(set);
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
      return LayoutUnit();
  }
}

void GrowAffectedSizeByPlannedIncrease(
    NGGridSet& set,
    GridItemContributionType contribution_type) {
  LayoutUnit planned_increase = set.planned_increase;
  set.is_infinitely_growable = false;

  // Only grow sets that accommodated a grid item.
  if (planned_increase == kIndefiniteSize)
    return;

  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
      set.IncreaseBaseSize(set.BaseSize() + planned_increase);
      return;
    case GridItemContributionType::kForIntrinsicMaximums:
      // Mark any tracks whose growth limit changed from infinite to finite in
      // this step as infinitely growable for the next step.
      set.is_infinitely_growable = set.GrowthLimit() == kIndefiniteSize;
      set.IncreaseGrowthLimit(DefiniteGrowthLimit(set) + planned_increase);
      return;
    case GridItemContributionType::kForMaxContentMaximums:
      set.IncreaseGrowthLimit(DefiniteGrowthLimit(set) + planned_increase);
      return;
    case GridItemContributionType::kForFreeSpace:
      NOTREACHED();
      return;
  }
}

// Returns true if a set should increase its used size according to the steps in
// https://drafts.csswg.org/css-grid-2/#algo-spanning-items; false otherwise.
bool IsContributionAppliedToSet(const NGGridSet& set,
                                GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
      return set.track_size.HasIntrinsicMinTrackBreadth();
    case GridItemContributionType::kForContentBasedMinimums:
      return set.track_size.HasMinOrMaxContentMinTrackBreadth();
    case GridItemContributionType::kForMaxContentMinimums:
      // TODO(ethavar): Check if the grid container is being sized under a
      // 'max-content' constraint to consider 'auto' min track sizing functions,
      // see https://drafts.csswg.org/css-grid-2/#track-size-max-content-min.
      return set.track_size.HasMaxContentMinTrackBreadth();
    case GridItemContributionType::kForIntrinsicMaximums:
      return set.track_size.HasIntrinsicMaxTrackBreadth();
    case GridItemContributionType::kForMaxContentMaximums:
      return set.track_size.HasMaxContentOrAutoMaxTrackBreadth();
    case GridItemContributionType::kForFreeSpace:
      return true;
  }
}

// https://drafts.csswg.org/css-grid-2/#extra-space
// Returns true if a set's used size should be consider to grow beyond its limit
// (see the "Distribute space beyond limits" section); otherwise, false.
// Note that we will deliberately return false in cases where we don't have a
// collection of tracks different than "all affected tracks".
bool ShouldUsedSizeGrowBeyondLimit(const NGGridSet& set,
                                   GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
      return set.track_size.HasIntrinsicMaxTrackBreadth();
    case GridItemContributionType::kForMaxContentMinimums:
      return set.track_size.HasMaxContentOrAutoMaxTrackBreadth();
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
    case GridItemContributionType::kForFreeSpace:
      return false;
  }
}

bool IsDistributionForGrowthLimits(GridItemContributionType contribution_type) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums:
    case GridItemContributionType::kForFreeSpace:
      return false;
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums:
      return true;
  }
}

enum class InfinitelyGrowableBehavior { kEnforce, kIgnore };

// We define growth potential = limit - affected size; for base sizes, the limit
// is its growth limit. For growth limits, the limit is infinity if it is marked
// as "infinitely growable", and equal to the growth limit otherwise.
LayoutUnit GrowthPotentialForSet(
    const NGGridSet& set,
    GridItemContributionType contribution_type,
    InfinitelyGrowableBehavior infinitely_growable_behavior =
        InfinitelyGrowableBehavior::kEnforce) {
  switch (contribution_type) {
    case GridItemContributionType::kForIntrinsicMinimums:
    case GridItemContributionType::kForContentBasedMinimums:
    case GridItemContributionType::kForMaxContentMinimums: {
      LayoutUnit growth_limit = set.GrowthLimit();
      if (growth_limit == kIndefiniteSize)
        return kIndefiniteSize;

      LayoutUnit increased_base_size =
          set.BaseSize() + set.item_incurred_increase;
      DCHECK_LE(increased_base_size, growth_limit);
      return growth_limit - increased_base_size;
    }
    case GridItemContributionType::kForIntrinsicMaximums:
    case GridItemContributionType::kForMaxContentMaximums: {
      if (infinitely_growable_behavior ==
              InfinitelyGrowableBehavior::kEnforce &&
          set.GrowthLimit() != kIndefiniteSize && !set.is_infinitely_growable) {
        // For growth limits, the potential is infinite if its value is infinite
        // too or if the set is marked as infinitely growable; otherwise, zero.
        return LayoutUnit();
      }

      DCHECK(set.fit_content_limit >= 0 ||
             set.fit_content_limit == kIndefiniteSize);

      // The max track sizing function of a 'fit-content' track is treated as
      // 'max-content' until it reaches the limit specified as the 'fit-content'
      // argument, after which it is treated as having a fixed sizing function
      // of that argument (with a growth potential of zero).
      if (set.fit_content_limit != kIndefiniteSize) {
        LayoutUnit growth_potential = set.fit_content_limit -
                                      DefiniteGrowthLimit(set) -
                                      set.item_incurred_increase;
        return growth_potential.ClampNegativeToZero();
      }
      // Otherwise, this set has infinite growth potential.
      return kIndefiniteSize;
    }
    case GridItemContributionType::kForFreeSpace: {
      LayoutUnit growth_limit = set.GrowthLimit();
      DCHECK_NE(growth_limit, kIndefiniteSize);
      return growth_limit - set.BaseSize();
    }
  }
}

template <typename T>
bool AreEqual(T a, T b) {
  return a == b;
}

template <>
bool AreEqual<double>(double a, double b) {
  return std::abs(a - b) < kDoubleEpsilon;
}

// Follow the definitions from https://drafts.csswg.org/css-grid-2/#extra-space;
// notice that this method replaces the notion of "tracks" with "sets".
template <bool is_equal_distribution>
void DistributeExtraSpaceToSets(LayoutUnit extra_space,
                                double flex_factor_sum,
                                GridItemContributionType contribution_type,
                                GridSetPtrVector* sets_to_grow,
                                GridSetPtrVector* sets_to_grow_beyond_limit) {
  DCHECK(extra_space && sets_to_grow);

  if (extra_space == kIndefiniteSize) {
    // Infinite extra space should only happen when distributing free space at
    // the maximize tracks step; in such case, we can simplify this method by
    // "filling" every track base size up to their growth limit.
    DCHECK_EQ(contribution_type, GridItemContributionType::kForFreeSpace);
    for (auto* set : *sets_to_grow) {
      set->item_incurred_increase =
          GrowthPotentialForSet(*set, contribution_type);
    }
    return;
  }

  DCHECK_GT(extra_space, 0);
#if DCHECK_IS_ON()
  if (IsDistributionForGrowthLimits(contribution_type))
    DCHECK_EQ(sets_to_grow, sets_to_grow_beyond_limit);
#endif

  wtf_size_t growable_track_count = 0;
  for (auto* set : *sets_to_grow) {
    set->item_incurred_increase = LayoutUnit();

    // From the first note in https://drafts.csswg.org/css-grid-2/#extra-space:
    //   If the affected size was a growth limit and the track is not marked
    //   "infinitely growable", then each item-incurred increase will be zero.
    //
    // When distributing space to growth limits, we need to increase each track
    // up to its 'fit-content' limit. However, because of the note above, first
    // we should only grow tracks marked as "infinitely growable" up to limits
    // and then grow all affected tracks beyond limits.
    //
    // We can correctly resolve every scenario by doing a single sort of
    // |sets_to_grow|, purposely ignoring the "infinitely growable" flag, then
    // filtering out sets that won't take a share of the extra space at each
    // step; for base sizes this is not required, but if there are no tracks
    // with growth potential > 0, we can optimize by not sorting the sets.
    if (GrowthPotentialForSet(*set, contribution_type))
      growable_track_count += set->track_count;
  }

  using ShareRatioType = typename std::conditional<is_equal_distribution,
                                                   wtf_size_t, double>::type;
  DCHECK(is_equal_distribution ||
         !AreEqual<ShareRatioType>(flex_factor_sum, 0));
  ShareRatioType share_ratio_sum =
      is_equal_distribution ? growable_track_count : flex_factor_sum;
  const bool is_flex_factor_sum_overflowing_limits =
      share_ratio_sum >= std::numeric_limits<wtf_size_t>::max();

  // We will sort the tracks by growth potential in non-decreasing order to
  // distribute space up to limits; notice that if we start distributing space
  // equally among all tracks we will eventually reach the limit of a track or
  // run out of space to distribute. If the former scenario happens, it should
  // be easy to see that the group of tracks that will reach its limit first
  // will be that with the least growth potential. Otherwise, if tracks in such
  // group does not reach their limit, every upcoming track with greater growth
  // potential must be able to increase its size by the same amount.
  if (growable_track_count ||
      IsDistributionForGrowthLimits(contribution_type)) {
    auto CompareSetsByGrowthPotential =
        [contribution_type](const NGGridSet* lhs, const NGGridSet* rhs) {
          auto growth_potential_lhs = GrowthPotentialForSet(
              *lhs, contribution_type, InfinitelyGrowableBehavior::kIgnore);
          auto growth_potential_rhs = GrowthPotentialForSet(
              *rhs, contribution_type, InfinitelyGrowableBehavior::kIgnore);

          if (growth_potential_lhs == kIndefiniteSize ||
              growth_potential_rhs == kIndefiniteSize) {
            // At this point we know that there is at least one set with
            // infinite growth potential; if |a| has a definite value, then |b|
            // must have infinite growth potential, and thus, |a| < |b|.
            return growth_potential_lhs != kIndefiniteSize;
          }
          // Straightforward comparison of definite growth potentials.
          return growth_potential_lhs < growth_potential_rhs;
        };

    // Only sort for equal distributions; since the growth potential of any
    // flexible set is infinite, they don't require comparing.
    if (AreEqual<double>(flex_factor_sum, 0)) {
      DCHECK(is_equal_distribution);
      std::sort(sets_to_grow->begin(), sets_to_grow->end(),
                CompareSetsByGrowthPotential);
    }
  }

  auto ExtraSpaceShare = [&](const NGGridSet& set,
                             LayoutUnit growth_potential) -> LayoutUnit {
    DCHECK(growth_potential >= 0 || growth_potential == kIndefiniteSize);

    // If this set won't take a share of the extra space, e.g. has zero growth
    // potential, exit so that this set is filtered out of |share_ratio_sum|.
    if (!growth_potential)
      return LayoutUnit();

    wtf_size_t set_track_count = set.track_count;
    DCHECK_LE(set_track_count, growable_track_count);

    ShareRatioType set_share_ratio =
        is_equal_distribution ? set_track_count : set.FlexFactor();

    // Since |share_ratio_sum| can be greater than the wtf_size_t limit, cap the
    // value of |set_share_ratio| to prevent overflows.
    if (set_share_ratio > share_ratio_sum) {
      DCHECK(is_flex_factor_sum_overflowing_limits);
      set_share_ratio = share_ratio_sum;
    }

    LayoutUnit extra_space_share;
    if (AreEqual(set_share_ratio, share_ratio_sum)) {
      // If this set's share ratio and the remaining ratio sum are the same, it
      // means that this set will receive all of the remaining space. Hence, we
      // can optimize a little by directly using the extra space as this set's
      // share and break early by decreasing the remaining growable track count
      // to 0 (even if there are further growable tracks, since the share ratio
      // sum will be reduced to 0, their space share will also be 0).
      set_track_count = growable_track_count;
      extra_space_share = extra_space;
    } else {
      DCHECK(!AreEqual<ShareRatioType>(share_ratio_sum, 0));
      DCHECK_LT(set_share_ratio, share_ratio_sum);

      extra_space_share = LayoutUnit::FromRawValue(
          (extra_space.RawValue() * set_share_ratio) / share_ratio_sum);
    }

    if (growth_potential != kIndefiniteSize)
      extra_space_share = std::min(extra_space_share, growth_potential);
    DCHECK_LE(extra_space_share, extra_space);

    growable_track_count -= set_track_count;
    share_ratio_sum -= set_share_ratio;
    extra_space -= extra_space_share;
    return extra_space_share;
  };

  // Distribute space up to limits:
  //   - For base sizes, grow the base size up to the growth limit.
  //   - For growth limits, the only case where a growth limit should grow at
  //   this step is when its set has already been marked "infinitely growable".
  //   Increase the growth limit up to the 'fit-content' argument (if any); note
  //   that these arguments could prevent this step to fulfill the entirety of
  //   the extra space and further distribution would be needed.
  for (auto* set : *sets_to_grow) {
    // Break early if there are no further tracks to grow.
    if (!growable_track_count)
      break;
    set->item_incurred_increase =
        ExtraSpaceShare(*set, GrowthPotentialForSet(*set, contribution_type));
  }

  // Distribute space beyond limits:
  //   - For base sizes, every affected track can grow indefinitely.
  //   - For growth limits, grow tracks up to their 'fit-content' argument.
  if (sets_to_grow_beyond_limit && extra_space) {
#if DCHECK_IS_ON()
    // We expect |sets_to_grow_beyond_limit| to be ordered by growth potential
    // for the following section of the algorithm to work.
    //
    // For base sizes, since going beyond limits should only happen after we
    // grow every track up to their growth limits, it should be easy to see that
    // every growth potential is now zero, so they're already ordered.
    //
    // Now let's consider growth limits: we forced the sets to be sorted by
    // growth potential ignoring the "infinitely growable" flag, meaning that
    // ultimately they will be sorted by remaining space to their 'fit-content'
    // parameter (if it exists, infinite otherwise). If we ended up here, we
    // must have filled the sets marked as "infinitely growable" up to their
    // 'fit-content' parameter; therefore, if we only consider sets with
    // remaining space to their 'fit-content' limit in the following
    // distribution step, they should still be ordered.
    LayoutUnit previous_growable_potential;
    for (auto* set : *sets_to_grow_beyond_limit) {
      LayoutUnit growth_potential = GrowthPotentialForSet(
          *set, contribution_type, InfinitelyGrowableBehavior::kIgnore);
      if (growth_potential) {
        if (previous_growable_potential == kIndefiniteSize) {
          DCHECK_EQ(growth_potential, kIndefiniteSize);
        } else {
          DCHECK(growth_potential >= previous_growable_potential ||
                 growth_potential == kIndefiniteSize);
        }
        previous_growable_potential = growth_potential;
      }
    }
#endif

    auto BeyondLimitsGrowthPotential =
        [contribution_type](const NGGridSet& set) -> LayoutUnit {
      // For growth limits, ignore the "infinitely growable" flag and grow all
      // affected tracks up to their 'fit-content' argument (note that
      // |GrowthPotentialForSet| already accounts for it).
      return !IsDistributionForGrowthLimits(contribution_type)
                 ? kIndefiniteSize
                 : GrowthPotentialForSet(set, contribution_type,
                                         InfinitelyGrowableBehavior::kIgnore);
    };

    // If we reached this point, we must have exhausted every growable track up
    // to their limits, meaning |growable_track_count| should be 0 and we need
    // to recompute it considering their 'fit-content' limits instead.
    DCHECK_EQ(growable_track_count, 0u);

    for (auto* set : *sets_to_grow_beyond_limit) {
      if (BeyondLimitsGrowthPotential(*set))
        growable_track_count += set->track_count;
    }

    // In |IncreaseTrackSizesToAccommodateGridItems| we guaranteed that, when
    // dealing with flexible tracks, there shouldn't be any set to grow beyond
    // limits. Thus, the only way to reach the section below is when we are
    // distributing space equally among sets.
    DCHECK(is_equal_distribution);
    share_ratio_sum = growable_track_count;

    for (auto* set : *sets_to_grow_beyond_limit) {
      // Break early if there are no further tracks to grow.
      if (!growable_track_count)
        break;
      set->item_incurred_increase +=
          ExtraSpaceShare(*set, BeyondLimitsGrowthPotential(*set));
    }
  }
}

void DistributeExtraSpaceToSetsEqually(
    LayoutUnit extra_space,
    GridItemContributionType contribution_type,
    GridSetPtrVector* sets_to_grow,
    GridSetPtrVector* sets_to_grow_beyond_limit = nullptr) {
  DistributeExtraSpaceToSets</* is_equal_distribution */ true>(
      extra_space, /* flex_factor_sum */ 0, contribution_type, sets_to_grow,
      sets_to_grow_beyond_limit);
}

void DistributeExtraSpaceToWeightedSets(
    LayoutUnit extra_space,
    const double flex_factor_sum,
    GridItemContributionType contribution_type,
    GridSetPtrVector* sets_to_grow) {
  DistributeExtraSpaceToSets</* is_equal_distribution */ false>(
      extra_space, flex_factor_sum, contribution_type, sets_to_grow,
      /* sets_to_grow_beyond_limit */ nullptr);
}

}  // namespace

void NGGridLayoutAlgorithm::IncreaseTrackSizesToAccommodateGridItems(
    GridItemDataPtrVector::iterator group_begin,
    GridItemDataPtrVector::iterator group_end,
    const NGGridLayoutData& layout_data,
    const bool is_group_spanning_flex_track,
    SizingConstraint sizing_constraint,
    GridItemContributionType contribution_type,
    NGGridSizingTrackCollection* track_collection) const {
  DCHECK(track_collection);
  const auto track_direction = track_collection->Direction();

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    set_iterator.CurrentSet().planned_increase = kIndefiniteSize;
  }

  GridSetPtrVector sets_to_grow;
  GridSetPtrVector sets_to_grow_beyond_limit;

  while (group_begin != group_end) {
    GridItemData& grid_item = **(group_begin++);
    DCHECK(grid_item.IsSpanningIntrinsicTrack(track_direction));

    sets_to_grow.Shrink(0);
    sets_to_grow_beyond_limit.Shrink(0);

    ClampedDouble flex_factor_sum = 0;
    LayoutUnit spanned_tracks_size = track_collection->GutterSize() *
                                     (grid_item.SpanSize(track_direction) - 1);
    for (auto set_iterator =
             GetSetIteratorForItem(grid_item, *track_collection);
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      auto& current_set = set_iterator.CurrentSet();

      spanned_tracks_size +=
          AffectedSizeForContribution(current_set, contribution_type);

      if (is_group_spanning_flex_track &&
          !current_set.track_size.HasFlexMaxTrackBreadth()) {
        // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
        //   Distributing space only to flexible tracks (i.e. treating all other
        //   tracks as having a fixed sizing function).
        continue;
      }

      if (IsContributionAppliedToSet(current_set, contribution_type)) {
        if (current_set.planned_increase == kIndefiniteSize)
          current_set.planned_increase = LayoutUnit();

        if (is_group_spanning_flex_track)
          flex_factor_sum += current_set.FlexFactor();

        sets_to_grow.push_back(&current_set);
        if (ShouldUsedSizeGrowBeyondLimit(current_set, contribution_type))
          sets_to_grow_beyond_limit.push_back(&current_set);
      }
    }

    if (sets_to_grow.empty())
      continue;

    // Subtract the corresponding size (base size or growth limit) of every
    // spanned track from the grid item's size contribution to find the item's
    // remaining size contribution. For infinite growth limits, substitute with
    // the track's base size. This is the space to distribute, floor it at zero.
    LayoutUnit extra_space = ContributionSizeForGridItem(
        layout_data, contribution_type, track_direction, sizing_constraint,
        &grid_item);
    extra_space = (extra_space - spanned_tracks_size).ClampNegativeToZero();

    if (!extra_space)
      continue;

    // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
    //   If the sum of the flexible sizing functions of all flexible tracks
    //   spanned by the item is greater than zero, distributing space to such
    //   tracks according to the ratios of their flexible sizing functions
    //   rather than distributing space equally.
    if (!is_group_spanning_flex_track || AreEqual<double>(flex_factor_sum, 0)) {
      DistributeExtraSpaceToSetsEqually(
          extra_space, contribution_type, &sets_to_grow,
          sets_to_grow_beyond_limit.empty() ? &sets_to_grow
                                            : &sets_to_grow_beyond_limit);
    } else {
      // 'fr' units are only allowed as a maximum in track definitions, meaning
      // that no set has an intrinsic max track sizing function that would allow
      // it to grow beyond limits (see |ShouldUsedSizeGrowBeyondLimit|).
      DCHECK(sets_to_grow_beyond_limit.empty());
      DistributeExtraSpaceToWeightedSets(extra_space, flex_factor_sum,
                                         contribution_type, &sets_to_grow);
    }

    // For each affected track, if the track's item-incurred increase is larger
    // than its planned increase, set the planned increase to that value.
    for (auto* set : sets_to_grow) {
      DCHECK_NE(set->item_incurred_increase, kIndefiniteSize);
      DCHECK_NE(set->planned_increase, kIndefiniteSize);
      set->planned_increase =
          std::max(set->item_incurred_increase, set->planned_increase);
    }
  }

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    GrowAffectedSizeByPlannedIncrease(set_iterator.CurrentSet(),
                                      contribution_type);
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-content
void NGGridLayoutAlgorithm::ResolveIntrinsicTrackSizes(
    const NGGridLayoutData& layout_data,
    SizingConstraint sizing_constraint,
    NGGridSizingTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection && grid_items);
  const auto track_direction = track_collection->Direction();

  GridItemDataPtrVector reordered_grid_items;
  reordered_grid_items.ReserveInitialCapacity(grid_items->Size());

  for (auto& grid_item : *grid_items) {
    if (grid_item.IsSpanningIntrinsicTrack(track_direction))
      reordered_grid_items.emplace_back(&grid_item);
  }

  // Reorder grid items to process them as follows:
  //   - First, consider items spanning a single non-flexible track.
  //   - Next, consider items with span size of 2 not spanning a flexible track.
  //   - Repeat incrementally for items with greater span sizes until all items
  //   not spanning a flexible track have been considered.
  //   - Finally, consider all items spanning a flexible track.
  auto CompareGridItemsForIntrinsicTrackResolution =
      [track_direction](GridItemData* lhs, GridItemData* rhs) -> bool {
    if (lhs->IsSpanningFlexibleTrack(track_direction) ||
        rhs->IsSpanningFlexibleTrack(track_direction)) {
      // Ignore span sizes if one of the items spans a track with a flexible
      // sizing function; items not spanning such tracks should come first.
      return !lhs->IsSpanningFlexibleTrack(track_direction);
    }
    return lhs->SpanSize(track_direction) < rhs->SpanSize(track_direction);
  };
  std::sort(reordered_grid_items.begin(), reordered_grid_items.end(),
            CompareGridItemsForIntrinsicTrackResolution);

  // First, process the items that don't span a flexible track.
  auto** current_group_begin = reordered_grid_items.begin();
  while (current_group_begin != reordered_grid_items.end() &&
         !(*current_group_begin)->IsSpanningFlexibleTrack(track_direction)) {
    // Each iteration considers all items with the same span size.
    wtf_size_t current_group_span_size =
        (*current_group_begin)->SpanSize(track_direction);

    auto** current_group_end = current_group_begin;
    do {
      DCHECK(!(*current_group_end)->IsSpanningFlexibleTrack(track_direction));
      ++current_group_end;
    } while (current_group_end != reordered_grid_items.end() &&
             !(*current_group_end)->IsSpanningFlexibleTrack(track_direction) &&
             (*current_group_end)->SpanSize(track_direction) ==
                 current_group_span_size);

    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, layout_data,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForIntrinsicMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, layout_data,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForContentBasedMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, layout_data,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForMaxContentMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, layout_data,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForIntrinsicMaximums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, current_group_end, layout_data,
        /* is_group_spanning_flex_track */ false, sizing_constraint,
        GridItemContributionType::kForMaxContentMaximums, track_collection);

    // Move to the next group with greater span size.
    current_group_begin = current_group_end;
  }

  // From https://drafts.csswg.org/css-grid-2/#algo-spanning-flex-items:
  //   Increase sizes to accommodate spanning items crossing flexible tracks:
  //   Next, repeat the previous step instead considering (together, rather than
  //   grouped by span size) all items that do span a track with a flexible
  //   sizing function...
#if DCHECK_IS_ON()
  // Every grid item of the remaining group should span a flexible track.
  for (auto** it = current_group_begin; it != reordered_grid_items.end(); ++it)
    DCHECK((*it)->IsSpanningFlexibleTrack(track_direction));
#endif

  // Now, process items spanning flexible tracks (if any).
  if (current_group_begin != reordered_grid_items.end()) {
    // We can safely skip contributions for maximums since a <flex> definition
    // does not have an intrinsic max track sizing function.
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, reordered_grid_items.end(), layout_data,
        /* is_group_spanning_flex_track */ true, sizing_constraint,
        GridItemContributionType::kForIntrinsicMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, reordered_grid_items.end(), layout_data,
        /* is_group_spanning_flex_track */ true, sizing_constraint,
        GridItemContributionType::kForContentBasedMinimums, track_collection);
    IncreaseTrackSizesToAccommodateGridItems(
        current_group_begin, reordered_grid_items.end(), layout_data,
        /* is_group_spanning_flex_track */ true, sizing_constraint,
        GridItemContributionType::kForMaxContentMinimums, track_collection);
  }
}

// https://drafts.csswg.org/css-grid-2/#algo-grow-tracks
void NGGridLayoutAlgorithm::MaximizeTracks(
    SizingConstraint sizing_constraint,
    NGGridSizingTrackCollection* track_collection) const {
  const LayoutUnit free_space =
      DetermineFreeSpace(sizing_constraint, *track_collection);
  if (!free_space)
    return;

  GridSetPtrVector sets_to_grow;
  sets_to_grow.ReserveInitialCapacity(track_collection->GetSetCount());
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    sets_to_grow.push_back(&set_iterator.CurrentSet());
  }

  DistributeExtraSpaceToSetsEqually(
      free_space, GridItemContributionType::kForFreeSpace, &sets_to_grow);

  for (auto* set : sets_to_grow)
    set->IncreaseBaseSize(set->BaseSize() + set->item_incurred_increase);

  // TODO(ethavar): If this would cause the grid to be larger than the grid
  // container’s inner size as limited by its 'max-width/height', then redo this
  // step, treating the available grid space as equal to the grid container’s
  // inner size when it’s sized to its 'max-width/height'.
}

// https://drafts.csswg.org/css-grid-2/#algo-stretch
void NGGridLayoutAlgorithm::StretchAutoTracks(
    SizingConstraint sizing_constraint,
    NGGridSizingTrackCollection* track_collection) const {
  const auto track_direction = track_collection->Direction();

  // Stretching auto tracks should only occur if we have a "stretch" (or
  // default) content distribution.
  const auto& content_alignment = (track_direction == kForColumns)
                                      ? Style().JustifyContent()
                                      : Style().AlignContent();

  if (content_alignment.Distribution() != ContentDistributionType::kStretch &&
      (content_alignment.Distribution() != ContentDistributionType::kDefault ||
       content_alignment.GetPosition() != ContentPosition::kNormal)) {
    return;
  }

  // Expand tracks that have an 'auto' max track sizing function by dividing any
  // remaining positive, definite free space equally amongst them.
  GridSetPtrVector sets_to_grow;
  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (set.track_size.HasAutoMaxTrackBreadth() &&
        !set.track_size.IsFitContent()) {
      sets_to_grow.push_back(&set);
    }
  }

  if (sets_to_grow.empty())
    return;

  LayoutUnit free_space =
      DetermineFreeSpace(sizing_constraint, *track_collection);

  // If the free space is indefinite, but the grid container has a definite
  // min-width/height, use that size to calculate the free space for this step
  // instead.
  if (free_space == kIndefiniteSize) {
    free_space = (track_direction == kForColumns)
                     ? grid_min_available_size_.inline_size
                     : grid_min_available_size_.block_size;

    DCHECK_NE(free_space, kIndefiniteSize);
    free_space -= track_collection->TotalTrackSize();
  }

  if (free_space <= 0)
    return;

  DistributeExtraSpaceToSetsEqually(free_space,
                                    GridItemContributionType::kForFreeSpace,
                                    &sets_to_grow, &sets_to_grow);
  for (auto* set : sets_to_grow)
    set->IncreaseBaseSize(set->BaseSize() + set->item_incurred_increase);
}

// https://drafts.csswg.org/css-grid-2/#algo-flex-tracks
void NGGridLayoutAlgorithm::ExpandFlexibleTracks(
    const NGGridLayoutData& layout_data,
    SizingConstraint sizing_constraint,
    NGGridSizingTrackCollection* track_collection,
    GridItems* grid_items) const {
  DCHECK(track_collection && grid_items);
  LayoutUnit free_space =
      DetermineFreeSpace(sizing_constraint, *track_collection);

  // If the free space is zero or if sizing the grid container under a
  // min-content constraint, the used flex fraction is zero.
  if (!free_space)
    return;

  const auto gutter_size = track_collection->GutterSize();
  const auto track_direction = track_collection->Direction();

  // https://drafts.csswg.org/css-grid-2/#algo-find-fr-size
  GridSetPtrVector flexible_sets;
  auto FindFrSize = [&](SetIterator set_iterator,
                        LayoutUnit leftover_space) -> double {
    ClampedDouble flex_factor_sum = 0;
    wtf_size_t total_track_count = 0;
    flexible_sets.Shrink(0);

    while (!set_iterator.IsAtEnd()) {
      auto& set = set_iterator.CurrentSet();
      if (set.track_size.HasFlexMaxTrackBreadth() &&
          !AreEqual<double>(set.FlexFactor(), 0)) {
        flex_factor_sum += set.FlexFactor();
        flexible_sets.push_back(&set);
      } else {
        leftover_space -= set.BaseSize();
      }
      total_track_count += set.track_count;
      set_iterator.MoveToNextSet();
    }

    // Remove the gutters between spanned tracks.
    leftover_space -= gutter_size * (total_track_count - 1);

    if (leftover_space < 0 || flexible_sets.empty())
      return 0;

    // From css-grid-2 spec: "If the product of the hypothetical fr size and
    // a flexible track’s flex factor is less than the track’s base size,
    // restart this algorithm treating all such tracks as inflexible."
    //
    // We will process the same algorithm a bit different; since we define the
    // hypothetical fr size as the leftover space divided by the flex factor
    // sum, we can reinterpret the statement above as follows:
    //
    //   (leftover space / flex factor sum) * flexible set's flex factor <
    //       flexible set's base size
    //
    // Reordering the terms of such expression we get:
    //
    //   leftover space / flex factor sum <
    //       flexible set's base size / flexible set's flex factor
    //
    // The term on the right is constant for every flexible set, while the term
    // on the left changes whenever we restart the algorithm treating some of
    // those sets as inflexible. Note that, if the expression above is false for
    // a given set, any other set with a lesser (base size / flex factor) ratio
    // will also fail such expression.
    //
    // Based on this observation, we can process the sets in non-increasing
    // ratio, when the current set does not fulfill the expression, no further
    // set will fulfill it either (and we can return the hypothetical fr size).
    // Otherwise, determine which sets should be treated as inflexible, exclude
    // them from the leftover space and flex factor sum computation, and keep
    // checking the condition for sets with lesser ratios.
    auto CompareSetsByBaseSizeFlexFactorRatio = [](NGGridSet* lhs,
                                                   NGGridSet* rhs) -> bool {
      // Avoid divisions by reordering the terms of the comparison.
      return lhs->BaseSize().RawValue() * rhs->FlexFactor() >
             rhs->BaseSize().RawValue() * lhs->FlexFactor();
    };
    std::sort(flexible_sets.begin(), flexible_sets.end(),
              CompareSetsByBaseSizeFlexFactorRatio);

    auto** current_set = flexible_sets.begin();
    while (leftover_space > 0 && current_set != flexible_sets.end()) {
      flex_factor_sum = base::ClampMax(flex_factor_sum, 1);

      auto** next_set = current_set;
      while (next_set != flexible_sets.end() &&
             (*next_set)->FlexFactor() * leftover_space.RawValue() <
                 (*next_set)->BaseSize().RawValue() * flex_factor_sum) {
        ++next_set;
      }

      // Any upcoming flexible set will receive a share of free space of at
      // least their base size; return the current hypothetical fr size.
      if (current_set == next_set) {
        DCHECK(!AreEqual<double>(flex_factor_sum, 0));
        return leftover_space.RawValue() / flex_factor_sum;
      }

      // Otherwise, treat all those sets that does not receive a share of free
      // space of at least their base size as inflexible, effectively excluding
      // them from the leftover space and flex factor sum computation.
      for (auto** it = current_set; it != next_set; ++it) {
        flex_factor_sum -= (*it)->FlexFactor();
        leftover_space -= (*it)->BaseSize();
      }
      current_set = next_set;
    }
    return 0;
  };

  double fr_size = 0;
  if (free_space != kIndefiniteSize) {
    // Otherwise, if the free space is a definite length, the used flex fraction
    // is the result of finding the size of an fr using all of the grid tracks
    // and a space to fill of the available grid space.
    fr_size = FindFrSize(track_collection->GetSetIterator(),
                         (track_direction == kForColumns)
                             ? grid_available_size_.inline_size
                             : grid_available_size_.block_size);
  } else {
    // Otherwise, if the free space is an indefinite length, the used flex
    // fraction is the maximum of:
    //   - For each grid item that crosses a flexible track, the result of
    //   finding the size of an fr using all the grid tracks that the item
    //   crosses and a space to fill of the item’s max-content contribution.
    for (auto& grid_item : *grid_items) {
      if (grid_item.IsSpanningFlexibleTrack(track_direction)) {
        double grid_item_fr_size = FindFrSize(
            GetSetIteratorForItem(grid_item, *track_collection),
            ContributionSizeForGridItem(
                layout_data, GridItemContributionType::kForMaxContentMaximums,
                track_direction, sizing_constraint, &grid_item));
        fr_size = std::max(grid_item_fr_size, fr_size);
      }
    }

    //   - For each flexible track, if the flexible track’s flex factor is
    //   greater than one, the result of dividing the track’s base size by its
    //   flex factor; otherwise, the track’s base size.
    for (auto set_iterator = track_collection->GetConstSetIterator();
         !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
      auto& set = set_iterator.CurrentSet();
      if (!set.track_size.HasFlexMaxTrackBreadth())
        continue;

      DCHECK_GT(set.track_count, 0u);
      double set_flex_factor =
          base::ClampMax(set.FlexFactor(), set.track_count);
      fr_size = std::max(set.BaseSize().RawValue() / set_flex_factor, fr_size);
    }
  }

  // Notice that the fr size multiplied by a set's flex factor can result in a
  // non-integer size; since we floor the expanded size to fit in a LayoutUnit,
  // when multiple sets lose the fractional part of the computation we may not
  // distribute the entire free space. We fix this issue by accumulating the
  // leftover fractional part from every flexible set.
  double leftover_size = 0;

  for (auto set_iterator = track_collection->GetSetIterator();
       !set_iterator.IsAtEnd(); set_iterator.MoveToNextSet()) {
    auto& set = set_iterator.CurrentSet();
    if (!set.track_size.HasFlexMaxTrackBreadth())
      continue;

    const ClampedDouble fr_share = fr_size * set.FlexFactor() + leftover_size;
    // Add an epsilon to round up values very close to the next integer.
    const LayoutUnit expanded_size =
        LayoutUnit::FromRawValue(fr_share + kDoubleEpsilon);

    if (!expanded_size.MightBeSaturated() && expanded_size >= set.BaseSize()) {
      set.IncreaseBaseSize(expanded_size);
      // The epsilon added above might make |expanded_size| greater than
      // |fr_share|, in that case avoid a negative leftover by flooring to 0.
      leftover_size = base::ClampMax(fr_share - expanded_size.RawValue(), 0);
    }
  }

  // TODO(ethavar): If using this flex fraction would cause the grid to be
  // smaller than the grid container’s min-width/height (or larger than the grid
  // container’s max-width/height), then redo this step, treating the free space
  // as definite and the available grid space as equal to the grid container’s
  // inner size when it’s sized to its min-width/height (max-width/height).
}

LayoutUnit NGGridLayoutAlgorithm::GutterSize(
    GridTrackSizingDirection track_direction) const {
  const bool is_for_columns = track_direction == kForColumns;
  const auto& gutter_size =
      is_for_columns ? Style().ColumnGap() : Style().RowGap();

  if (!gutter_size)
    return LayoutUnit();

  LayoutUnit available_size = (is_for_columns ? grid_available_size_.inline_size
                                              : grid_available_size_.block_size)
                                  .ClampIndefiniteToZero();
  return MinimumValueForLength(*gutter_size, available_size);
}

// TODO(ikilpatrick): Determine if other uses of this method need to respect
// |grid_min_available_size_| similar to |StretchAutoTracks|.
LayoutUnit NGGridLayoutAlgorithm::DetermineFreeSpace(
    SizingConstraint sizing_constraint,
    const NGGridSizingTrackCollection& track_collection) const {
  const auto track_direction = track_collection.Direction();

  // https://drafts.csswg.org/css-sizing-3/#auto-box-sizes: both min-content and
  // max-content block sizes are the size of the content after layout.
  if (track_direction == kForRows)
    sizing_constraint = SizingConstraint::kLayout;

  switch (sizing_constraint) {
    case SizingConstraint::kLayout: {
      LayoutUnit free_space = (track_direction == kForColumns)
                                  ? grid_available_size_.inline_size
                                  : grid_available_size_.block_size;

      if (free_space != kIndefiniteSize) {
        // If tracks consume more space than the grid container has available,
        // clamp the free space to zero as there's no more room left to grow.
        free_space = (free_space - track_collection.TotalTrackSize())
                         .ClampNegativeToZero();
      }
      return free_space;
    }
    case SizingConstraint::kMaxContent:
      // If sizing under a max-content constraint, the free space is infinite.
      return kIndefiniteSize;
    case SizingConstraint::kMinContent:
      // If sizing under a min-content constraint, the free space is zero.
      return LayoutUnit();
  }
}

namespace {

// Returns the alignment offset for either the inline or block direction.
LayoutUnit AlignmentOffset(LayoutUnit container_size,
                           LayoutUnit size,
                           LayoutUnit margin_start,
                           LayoutUnit margin_end,
                           LayoutUnit baseline_offset,
                           AxisEdge axis_edge,
                           bool is_overflow_safe) {
  LayoutUnit free_space = container_size - size - margin_start - margin_end;
  // If overflow is 'safe', we have to make sure we don't overflow the
  // 'start' edge (potentially cause some data loss as the overflow is
  // unreachable).
  if (is_overflow_safe)
    free_space = free_space.ClampNegativeToZero();
  switch (axis_edge) {
    case AxisEdge::kStart:
      return margin_start;
    case AxisEdge::kCenter:
      return margin_start + (free_space / 2);
    case AxisEdge::kEnd:
      return margin_start + free_space;
    case AxisEdge::kFirstBaseline:
    case AxisEdge::kLastBaseline:
      return baseline_offset;
  }
  NOTREACHED();
  return LayoutUnit();
}

void AlignmentOffsetForOutOfFlow(
    const AxisEdge inline_axis_edge,
    const AxisEdge block_axis_edge,
    const LogicalSize container_size,
    NGLogicalStaticPosition::InlineEdge* inline_edge,
    NGLogicalStaticPosition::BlockEdge* block_edge,
    LogicalOffset* offset) {
  using InlineEdge = NGLogicalStaticPosition::InlineEdge;
  using BlockEdge = NGLogicalStaticPosition::BlockEdge;

  switch (inline_axis_edge) {
    case AxisEdge::kStart:
    case AxisEdge::kFirstBaseline:
      *inline_edge = InlineEdge::kInlineStart;
      break;
    case AxisEdge::kCenter:
      *inline_edge = InlineEdge::kInlineCenter;
      offset->inline_offset += container_size.inline_size / 2;
      break;
    case AxisEdge::kEnd:
    case AxisEdge::kLastBaseline:
      *inline_edge = InlineEdge::kInlineEnd;
      offset->inline_offset += container_size.inline_size;
      break;
  }

  switch (block_axis_edge) {
    case AxisEdge::kStart:
    case AxisEdge::kFirstBaseline:
      *block_edge = BlockEdge::kBlockStart;
      break;
    case AxisEdge::kCenter:
      *block_edge = BlockEdge::kBlockCenter;
      offset->block_offset += container_size.block_size / 2;
      break;
    case AxisEdge::kEnd:
    case AxisEdge::kLastBaseline:
      *block_edge = BlockEdge::kBlockEnd;
      offset->block_offset += container_size.block_size;
      break;
  }
}

}  // namespace

NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpace(
    NGCacheSlot cache_slot,
    const GridItemData& grid_item,
    const NGGridLayoutData& layout_data,
    const LogicalSize& containing_grid_area_size,
    absl::optional<LayoutUnit> opt_fixed_block_size,
    absl::optional<LayoutUnit> opt_fragment_relative_block_offset,
    bool min_block_size_should_encompass_intrinsic_size) const {
  const auto& container_constraint_space = ConstraintSpace();

  NGConstraintSpaceBuilder builder(
      container_constraint_space, grid_item.node.Style().GetWritingDirection(),
      /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);

  builder.SetCacheSlot(cache_slot);
  builder.SetIsPaintedAtomically(true);

  if (opt_fixed_block_size) {
    builder.SetAvailableSize(
        {containing_grid_area_size.inline_size, *opt_fixed_block_size});
    builder.SetIsFixedBlockSize(true);
  } else {
    builder.SetAvailableSize(containing_grid_area_size);
  }

  if (grid_item.IsSubgrid()) {
    if (containing_grid_area_size.inline_size != kIndefiniteSize)
      builder.SetIsFixedInlineSize(true);
    if (containing_grid_area_size.block_size != kIndefiniteSize)
      builder.SetIsFixedBlockSize(true);
  }

  builder.SetPercentageResolutionSize(containing_grid_area_size);
  builder.SetInlineAutoBehavior(grid_item.inline_auto_behavior);
  builder.SetBlockAutoBehavior(grid_item.block_auto_behavior);

  // TODO(ethavar): Currently, we inherit a subgridded track collection, but our
  // new approach to subgrid layout requires to pass the grid sizing subtree.
  if (layout_data.columns() && layout_data.rows() &&
      grid_item.has_subgridded_columns) {
    const auto& range_indices = grid_item.is_parallel_with_root_grid
                                    ? grid_item.column_range_indices
                                    : grid_item.row_range_indices;

    const auto& track_collection = grid_item.is_parallel_with_root_grid
                                       ? layout_data.Columns()
                                       : layout_data.Rows();

    builder.SetSubgriddedColumns(std::make_unique<NGGridLayoutTrackCollection>(
        track_collection.CreateSubgridCollection(
            range_indices.begin, range_indices.end, kForColumns)));
  }

  if (layout_data.columns() && layout_data.rows() &&
      grid_item.has_subgridded_rows) {
    const auto& range_indices = grid_item.is_parallel_with_root_grid
                                    ? grid_item.row_range_indices
                                    : grid_item.column_range_indices;

    const auto& track_collection = grid_item.is_parallel_with_root_grid
                                       ? layout_data.Rows()
                                       : layout_data.Columns();

    builder.SetSubgriddedRows(std::make_unique<NGGridLayoutTrackCollection>(
        track_collection.CreateSubgridCollection(range_indices.begin,
                                                 range_indices.end, kForRows)));
  }

  if (container_constraint_space.HasBlockFragmentation() &&
      opt_fragment_relative_block_offset) {
    if (min_block_size_should_encompass_intrinsic_size)
      builder.SetMinBlockSizeShouldEncompassIntrinsicSize();

    SetupSpaceBuilderForFragmentation(
        container_constraint_space, grid_item.node,
        *opt_fragment_relative_block_offset, &builder, /* is_new_fc */ true,
        container_builder_.RequiresContentBeforeBreaking());
  }
  return builder.ToConstraintSpace();
}

NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpaceForLayout(
    const GridItemData& grid_item,
    const NGGridLayoutData& layout_data,
    LogicalRect* containing_grid_area,
    absl::optional<LayoutUnit> opt_fragment_relative_block_offset,
    bool min_block_size_should_encompass_intrinsic_size) const {
  ComputeGridItemOffsetAndSize(grid_item, layout_data.Columns(),
                               &containing_grid_area->offset.inline_offset,
                               &containing_grid_area->size.inline_size);

  ComputeGridItemOffsetAndSize(grid_item, layout_data.Rows(),
                               &containing_grid_area->offset.block_offset,
                               &containing_grid_area->size.block_size);

  return CreateConstraintSpace(NGCacheSlot::kLayout, grid_item, layout_data,
                               containing_grid_area->size,
                               /* opt_fixed_block_size */ absl::nullopt,
                               opt_fragment_relative_block_offset,
                               min_block_size_should_encompass_intrinsic_size);
}

NGConstraintSpace NGGridLayoutAlgorithm::CreateConstraintSpaceForMeasure(
    const GridItemData& grid_item,
    const NGGridLayoutData& layout_data,
    GridTrackSizingDirection track_direction,
    absl::optional<LayoutUnit> opt_fixed_block_size) const {
  LogicalOffset unused_offset;
  LogicalSize containing_grid_area_size(kIndefiniteSize, kIndefiniteSize);

  if (track_direction == kForColumns) {
    ComputeGridItemOffsetAndSize(grid_item, layout_data.Rows(),
                                 &unused_offset.block_offset,
                                 &containing_grid_area_size.block_size);
  } else {
    ComputeGridItemOffsetAndSize(grid_item, layout_data.Columns(),
                                 &unused_offset.inline_offset,
                                 &containing_grid_area_size.inline_size);
  }
  return CreateConstraintSpace(NGCacheSlot::kMeasure, grid_item, layout_data,
                               containing_grid_area_size, opt_fixed_block_size);
}

namespace {

// Determining the grid's baseline is prioritized based on grid order (as
// opposed to DOM order). The baseline of the grid is determined by the first
// grid item with baseline alignment in the first row. If no items have
// baseline alignment, fall back to the first item in row-major order.
class BaselineAccumulator {
  STACK_ALLOCATED();

 public:
  explicit BaselineAccumulator(FontBaseline font_baseline)
      : font_baseline_(font_baseline) {}

  void Accumulate(const GridItemData& grid_item,
                  const NGBoxFragment& fragment,
                  const LayoutUnit block_offset) {
    auto StartsBefore = [](const GridArea& a, const GridArea& b) -> bool {
      if (a.rows.StartLine() < b.rows.StartLine())
        return true;
      if (a.rows.StartLine() > b.rows.StartLine())
        return false;
      return a.columns.StartLine() < b.columns.StartLine();
    };

    auto EndsAfter = [](const GridArea& a, const GridArea& b) -> bool {
      if (a.rows.EndLine() > b.rows.EndLine())
        return true;
      if (a.rows.EndLine() < b.rows.EndLine())
        return false;
      // Use greater-or-equal to prefer the "last" grid-item.
      return a.columns.EndLine() >= b.columns.EndLine();
    };

    if (!first_fallback_baseline_ ||
        StartsBefore(grid_item.resolved_position,
                     first_fallback_baseline_->resolved_position)) {
      first_fallback_baseline_.emplace(
          grid_item.resolved_position,
          block_offset + fragment.FirstBaselineOrSynthesize(font_baseline_));
    }

    if (!last_fallback_baseline_ ||
        EndsAfter(grid_item.resolved_position,
                  last_fallback_baseline_->resolved_position)) {
      last_fallback_baseline_.emplace(
          grid_item.resolved_position,
          block_offset + fragment.LastBaselineOrSynthesize(font_baseline_));
    }

    // Keep track of the first/last set which has content.
    const auto& set_indices = grid_item.SetIndices(kForRows);
    if (first_set_index_ == kNotFound || set_indices.begin < first_set_index_)
      first_set_index_ = set_indices.begin;
    if (last_set_index_ == kNotFound || set_indices.end - 1 > last_set_index_)
      last_set_index_ = set_indices.end - 1;
  }

  void AccumulateRows(const NGGridLayoutTrackCollection& rows) {
    for (wtf_size_t i = 0; i < rows.GetSetCount(); ++i) {
      LayoutUnit set_offset = rows.GetSetOffset(i);
      LayoutUnit major_baseline = rows.MajorBaseline(i);
      if (major_baseline != LayoutUnit::Min()) {
        LayoutUnit baseline_offset = set_offset + major_baseline;
        if (!first_major_baseline_)
          first_major_baseline_.emplace(i, baseline_offset);
        last_major_baseline_.emplace(i, baseline_offset);
      }

      LayoutUnit minor_baseline = rows.MinorBaseline(i);
      if (minor_baseline != LayoutUnit::Min()) {
        LayoutUnit baseline_offset =
            set_offset + rows.ComputeSetSpanSize(i, i + 1) - minor_baseline;
        if (!first_minor_baseline_)
          first_minor_baseline_.emplace(i, baseline_offset);
        last_minor_baseline_.emplace(i, baseline_offset);
      }
    }
  }

  absl::optional<LayoutUnit> FirstBaseline() const {
    if (first_major_baseline_ &&
        first_major_baseline_->set_index == first_set_index_) {
      return first_major_baseline_->baseline;
    }
    if (first_minor_baseline_ &&
        first_minor_baseline_->set_index == first_set_index_) {
      return first_minor_baseline_->baseline;
    }
    if (first_fallback_baseline_)
      return first_fallback_baseline_->baseline;
    return absl::nullopt;
  }

  absl::optional<LayoutUnit> LastBaseline() const {
    if (last_minor_baseline_ &&
        last_minor_baseline_->set_index == last_set_index_) {
      return last_minor_baseline_->baseline;
    }
    if (last_major_baseline_ &&
        last_major_baseline_->set_index == last_set_index_) {
      return last_major_baseline_->baseline;
    }
    if (last_fallback_baseline_)
      return last_fallback_baseline_->baseline;
    return absl::nullopt;
  }

 private:
  struct SetIndexAndBaseline {
    SetIndexAndBaseline(wtf_size_t set_index, LayoutUnit baseline)
        : set_index(set_index), baseline(baseline) {}
    wtf_size_t set_index;
    LayoutUnit baseline;
  };
  struct PositionAndBaseline {
    PositionAndBaseline(const GridArea& resolved_position, LayoutUnit baseline)
        : resolved_position(resolved_position), baseline(baseline) {}
    GridArea resolved_position;
    LayoutUnit baseline;
  };

  FontBaseline font_baseline_;
  wtf_size_t first_set_index_ = kNotFound;
  wtf_size_t last_set_index_ = kNotFound;

  absl::optional<SetIndexAndBaseline> first_major_baseline_;
  absl::optional<SetIndexAndBaseline> first_minor_baseline_;
  absl::optional<PositionAndBaseline> first_fallback_baseline_;

  absl::optional<SetIndexAndBaseline> last_major_baseline_;
  absl::optional<SetIndexAndBaseline> last_minor_baseline_;
  absl::optional<PositionAndBaseline> last_fallback_baseline_;
};

}  // namespace

void NGGridLayoutAlgorithm::PlaceGridItems(
    const GridItems& grid_items,
    const NGGridLayoutData& layout_data,
    Vector<EBreakBetween>* out_row_break_between,
    Vector<GridItemPlacementData>* out_grid_items_placement_data) {
  DCHECK(out_row_break_between);

  const auto& container_space = ConstraintSpace();
  const auto container_writing_direction =
      container_space.GetWritingDirection();
  bool should_propagate_child_break_values =
      container_space.ShouldPropagateChildBreakValues();

  if (should_propagate_child_break_values) {
    *out_row_break_between = Vector<EBreakBetween>(
        layout_data.Rows().GetSetCount() + 1, EBreakBetween::kAuto);
  }

  BaselineAccumulator baseline_accumulator(Style().GetFontBaseline());

  for (const auto& grid_item : grid_items) {
    LogicalRect containing_grid_area;
    const auto space = CreateConstraintSpaceForLayout(grid_item, layout_data,
                                                      &containing_grid_area);

    const auto& item_style = grid_item.node.Style();
    const auto margins = ComputeMarginsFor(space, item_style, container_space);

    auto* result = grid_item.node.Layout(space);
    const auto& physical_fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());
    NGBoxFragment fragment(container_writing_direction, physical_fragment);

    auto BaselineOffset = [&](GridTrackSizingDirection track_direction,
                              LayoutUnit size) -> LayoutUnit {
      if (!grid_item.IsBaselineAlignedForDirection(track_direction))
        return LayoutUnit();

      NGBoxFragment baseline_fragment(
          grid_item.BaselineWritingDirection(track_direction),
          physical_fragment);
      // The baseline offset is the difference between the grid item's baseline
      // and its track baseline.
      const LayoutUnit baseline_delta =
          Baseline(layout_data, grid_item, track_direction) -
          GetLogicalBaseline(
              baseline_fragment,
              grid_item.IsLastBaselineSpecifiedForDirection(track_direction));
      if (grid_item.BaselineGroup(track_direction) == BaselineGroup::kMajor)
        return baseline_delta;

      // BaselineGroup::kMinor
      const LayoutUnit item_size = (track_direction == kForColumns)
                                       ? fragment.InlineSize()
                                       : fragment.BlockSize();
      return size - baseline_delta - item_size;
    };

    LayoutUnit inline_baseline_offset =
        BaselineOffset(kForColumns, containing_grid_area.size.inline_size);
    LayoutUnit block_baseline_offset =
        BaselineOffset(kForRows, containing_grid_area.size.block_size);

    // Apply the grid-item's alignment (if any).
    containing_grid_area.offset += LogicalOffset(
        AlignmentOffset(containing_grid_area.size.inline_size,
                        fragment.InlineSize(), margins.inline_start,
                        margins.inline_end, inline_baseline_offset,
                        grid_item.InlineAxisAlignment(),
                        grid_item.IsInlineAxisOverflowSafe()),
        AlignmentOffset(containing_grid_area.size.block_size,
                        fragment.BlockSize(), margins.block_start,
                        margins.block_end, block_baseline_offset,
                        grid_item.BlockAxisAlignment(),
                        grid_item.IsBlockAxisOverflowSafe()));

    // Grid is special in that %-based offsets resolve against the grid-area.
    // Determine the relative offset here (instead of in the builder). This is
    // safe as grid *also* has special inflow-bounds logic (otherwise this
    // wouldn't work).
    absl::optional<LogicalOffset> relative_offset = LogicalOffset();
    if (item_style.GetPosition() == EPosition::kRelative) {
      *relative_offset += ComputeRelativeOffsetForBoxFragment(
          physical_fragment, container_writing_direction,
          containing_grid_area.size);
    }

    NGBlockNode(grid_item.node).StoreMargins(container_space, margins);

    // If |out_grid_items_placement_data| is present we just want to record the
    // initial position of all the children for the purposes of fragmentation.
    // Don't add these to the builder.
    if (out_grid_items_placement_data) {
      out_grid_items_placement_data->emplace_back(
          containing_grid_area.offset, *relative_offset,
          result->HasDescendantThatDependsOnPercentageBlockSize());
    } else {
      container_builder_.AddResult(*result, containing_grid_area.offset,
                                   relative_offset);
      baseline_accumulator.Accumulate(grid_item, fragment,
                                      containing_grid_area.offset.block_offset);
    }

    if (should_propagate_child_break_values) {
      auto item_break_before = JoinFragmentainerBreakValues(
          item_style.BreakBefore(), result->InitialBreakBefore());
      auto item_break_after = JoinFragmentainerBreakValues(
          item_style.BreakAfter(), result->FinalBreakAfter());

      const auto& set_indices = grid_item.SetIndices(kForRows);
      (*out_row_break_between)[set_indices.begin] =
          JoinFragmentainerBreakValues(
              (*out_row_break_between)[set_indices.begin], item_break_before);
      (*out_row_break_between)[set_indices.end] = JoinFragmentainerBreakValues(
          (*out_row_break_between)[set_indices.end], item_break_after);
    }
  }

  // Propagate the baselines.
  if (layout_data.Rows().HasBaselines()) {
    baseline_accumulator.AccumulateRows(layout_data.Rows());
  }
  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);
}

// This is only used in NGGridLayoutAlgorithm::PlaceGridItemsForFragmentation(),
// but placed here to add WTF VectorTraits.
struct ResultAndOffsets {
  DISALLOW_NEW();

 public:
  ResultAndOffsets(const NGLayoutResult* result,
                   LogicalOffset offset,
                   LogicalOffset relative_offset)
      : result(result), offset(offset), relative_offset(relative_offset) {}

  void Trace(Visitor* visitor) const { visitor->Trace(result); }

  Member<const NGLayoutResult> result;
  LogicalOffset offset;
  LogicalOffset relative_offset;
};

void NGGridLayoutAlgorithm::PlaceGridItemsForFragmentation(
    const GridItems& grid_items,
    const Vector<EBreakBetween>& row_break_between,
    NGGridLayoutData* layout_data,
    Vector<GridItemPlacementData>* grid_items_placement_data,
    Vector<LayoutUnit>* row_offset_adjustments,
    LayoutUnit* intrinsic_block_size,
    LayoutUnit* consumed_grid_block_size) {
  DCHECK(layout_data && grid_items_placement_data && row_offset_adjustments &&
         intrinsic_block_size && consumed_grid_block_size);

  // TODO(ikilpatrick): Update |SetHasSeenAllChildren| and early exit if true.
  const auto container_writing_direction =
      ConstraintSpace().GetWritingDirection();

  // The following roughly comes from:
  // https://drafts.csswg.org/css-grid-1/#fragmentation-alg
  //
  // We are interested in cases where the grid-item *may* expand due to
  // fragmentation (lines pushed down by a fragmentation line, etc).
  auto MinBlockSizeShouldEncompassIntrinsicSize =
      [&](const GridItemData& grid_item,
          bool has_descendant_that_depends_on_percentage_block_size) -> bool {
    // If this item has (any) descendant that is percentage based, we can end
    // up in a situation where we'll constantly try and expand the row. E.g.
    // <div style="display: grid;">
    //   <div style="min-height: 100px;">
    //     <div style="height: 200%;"></div>
    //   </div>
    // </div>
    if (has_descendant_that_depends_on_percentage_block_size)
      return false;

    if (grid_item.node.IsMonolithic())
      return false;

    const auto& item_style = grid_item.node.Style();

    // NOTE: We currently assume that writing-mode roots are monolithic, but
    // this may change in the future.
    DCHECK_EQ(container_writing_direction.GetWritingMode(),
              item_style.GetWritingMode());

    // Only allow growth on "auto" block-size items, (a fixed block-size item
    // can't grow).
    if (!item_style.LogicalHeight().IsAutoOrContentOrIntrinsic())
      return false;

    // Only allow growth on items which only span a single row.
    if (grid_item.SpanSize(kForRows) > 1)
      return false;

    // If we have a fixed maximum track, we assume that we've hit this maximum,
    // and as such shouldn't grow.
    if (grid_item.IsSpanningFixedMaximumTrack(kForRows) &&
        !grid_item.IsSpanningIntrinsicTrack(kForRows))
      return false;

    return !grid_item.IsSpanningFixedMinimumTrack(kForRows) ||
           Style().LogicalHeight().IsAutoOrContentOrIntrinsic();
  };

  wtf_size_t previous_expansion_row_set_index = kNotFound;
  auto IsExpansionMakingProgress = [&](wtf_size_t row_set_index) -> bool {
    return previous_expansion_row_set_index == kNotFound ||
           row_set_index > previous_expansion_row_set_index;
  };

  HeapVector<ResultAndOffsets> result_and_offsets;
  HeapVector<GridItemPlacementData*> out_of_fragmentainer_space_item_placement;
  BaselineAccumulator baseline_accumulator(Style().GetFontBaseline());
  LayoutUnit max_row_expansion;
  wtf_size_t expansion_row_set_index;
  wtf_size_t breakpoint_row_set_index;
  bool has_subsequent_children;

  auto UpdateBreakpointRowSetIndex = [&](wtf_size_t row_set_index) {
    if (row_set_index >= breakpoint_row_set_index)
      return;

    // We may have inserted a row-breakpoint due to an item running out of
    // fragmentainer space.
    // Clear this list if we select a different row-breakpoint.
    out_of_fragmentainer_space_item_placement.clear();
    breakpoint_row_set_index = row_set_index;
  };

  LayoutUnit fragmentainer_space = FragmentainerSpaceLeft(ConstraintSpace());
  base::span<const Member<const NGBreakToken>> child_break_tokens;
  if (BreakToken())
    child_break_tokens = BreakToken()->ChildBreakTokens();

  auto PlaceItems = [&]() {
    // Reset our state.
    result_and_offsets.clear();
    out_of_fragmentainer_space_item_placement.clear();
    baseline_accumulator = BaselineAccumulator(Style().GetFontBaseline());
    max_row_expansion = LayoutUnit();
    expansion_row_set_index = kNotFound;
    breakpoint_row_set_index = kNotFound;
    has_subsequent_children = false;

    auto child_break_token_it = child_break_tokens.begin();
    auto* placement_data_it = grid_items_placement_data->begin();

    for (const auto& grid_item : grid_items) {
      // Grab the offsets and break-token (if present) for this child.
      auto& item_placement_data = *(placement_data_it++);
      const NGBlockBreakToken* break_token = nullptr;
      if (child_break_token_it != child_break_tokens.end()) {
        if ((*child_break_token_it)->InputNode() == grid_item.node)
          break_token = To<NGBlockBreakToken>((child_break_token_it++)->Get());
      }

      const LayoutUnit fragment_relative_block_offset =
          IsBreakInside(break_token) ? LayoutUnit()
                                     : item_placement_data.offset.block_offset -
                                           *consumed_grid_block_size;
      const bool min_block_size_should_encompass_intrinsic_size =
          MinBlockSizeShouldEncompassIntrinsicSize(
              grid_item,
              item_placement_data
                  .has_descendant_that_depends_on_percentage_block_size);
      LogicalRect grid_area;
      const auto space = CreateConstraintSpaceForLayout(
          grid_item, *layout_data, &grid_area, fragment_relative_block_offset,
          min_block_size_should_encompass_intrinsic_size);

      // Make the grid area relative to this fragment.
      const auto item_row_set_index = grid_item.SetIndices(kForRows).begin;
      grid_area.offset.block_offset +=
          (*row_offset_adjustments)[item_row_set_index] -
          *consumed_grid_block_size;

      // Check to see if this child should be placed within this fragmentainer.
      // We base this calculation on the grid-area rather than the offset.
      // The row can either be:
      //  - Above, we've handled it already in a previous fragment.
      //  - Below, we'll handle it within a subsequent fragment.
      //
      // NOTE: Basing this calculation of the row position has the effect that
      // a child with a negative margin will be placed in the fragmentainer
      // with its row, but placed above the block-start edge of the
      // fragmentainer.
      if (fragmentainer_space != kIndefiniteSize &&
          grid_area.offset.block_offset >= fragmentainer_space) {
        has_subsequent_children = true;
        continue;
      }
      if (grid_area.offset.block_offset < LayoutUnit() && !break_token)
        continue;

      auto* result = grid_item.node.Layout(space, break_token);
      result_and_offsets.emplace_back(
          result,
          LogicalOffset(item_placement_data.offset.inline_offset,
                        fragment_relative_block_offset),
          item_placement_data.relative_offset);

      // We may have failed to generate a fragment (due to running out of
      // fragmentainer space). Force a breakpoint at the row, so we shift the
      // item into the next fragmentainer.
      if (result->Status() != NGLayoutResult::kSuccess) {
        DCHECK_EQ(result->Status(), NGLayoutResult::kOutOfFragmentainerSpace);
        UpdateBreakpointRowSetIndex(item_row_set_index);
        out_of_fragmentainer_space_item_placement.push_back(
            &item_placement_data);
        continue;
      }

      const NGBoxFragment fragment(
          container_writing_direction,
          To<NGPhysicalBoxFragment>(result->PhysicalFragment()));
      baseline_accumulator.Accumulate(grid_item, fragment,
                                      fragment_relative_block_offset);

      // If the row has container separation we are able to push it into the
      // next fragmentainer. If it doesn't we, need to take the current
      // breakpoint (even if it is undesirable).
      const bool row_has_container_separation =
          grid_area.offset.block_offset > LayoutUnit();

      if (row_has_container_separation &&
          item_row_set_index < breakpoint_row_set_index) {
        const auto break_between = row_break_between[item_row_set_index];

        // The row may have a forced break, move it to the next fragmentainer.
        if (IsForcedBreakValue(ConstraintSpace(), break_between)) {
          container_builder_.SetHasForcedBreak();
          UpdateBreakpointRowSetIndex(item_row_set_index);
          continue;
        }

        container_builder_.SetPreviousBreakAfter(break_between);
        const NGBreakAppeal appeal_before = CalculateBreakAppealBefore(
            ConstraintSpace(), grid_item.node, *result, container_builder_,
            row_has_container_separation);
        if (!MovePastBreakpoint(ConstraintSpace(), grid_item.node, *result,
                                fragment_relative_block_offset, appeal_before,
                                /* builder */ nullptr)) {
          UpdateBreakpointRowSetIndex(item_row_set_index);

          // We are choosing to add an early breakpoint at a row. Propagate our
          // space shortage to the column balancer.
          PropagateSpaceShortage(ConstraintSpace(), result,
                                 fragment_relative_block_offset,
                                 &container_builder_);

          // We may have "break-before:avoid" or similar on this row. Instead
          // of just breaking on this row, search upwards for a row with a
          // better EBreakBetween.
          if (IsAvoidBreakValue(ConstraintSpace(), break_between)) {
            for (int index = item_row_set_index - 1; index >= 0; --index) {
              // Only consider rows within this fragmentainer.
              LayoutUnit offset = layout_data->Rows().GetSetOffset(index) +
                                  (*row_offset_adjustments)[index] -
                                  *consumed_grid_block_size;
              if (offset <= LayoutUnit())
                break;

              // Forced row breaks should have been already handled, accept any
              // row with an "auto" break-between.
              if (row_break_between[index] == EBreakBetween::kAuto) {
                UpdateBreakpointRowSetIndex(index);
                break;
              }
            }
          }
          continue;
        }
      }

      // This item may want to expand due to fragmentation. Record how much we
      // should grow the row by (if applicable).
      if (min_block_size_should_encompass_intrinsic_size &&
          item_row_set_index <= expansion_row_set_index &&
          IsExpansionMakingProgress(item_row_set_index) &&
          fragmentainer_space != kIndefiniteSize &&
          grid_area.BlockEndOffset() <= fragmentainer_space) {
        // Check if we've found a different row to expand.
        if (expansion_row_set_index != item_row_set_index) {
          expansion_row_set_index = item_row_set_index;
          max_row_expansion = LayoutUnit();
        }

        LayoutUnit item_expansion;
        if (result->PhysicalFragment().BreakToken()) {
          // This item may have a break, and will want to expand into the next
          // fragmentainer, (causing the row to expand into the next
          // fragmentainer). We can't use the size of the fragment, as we don't
          // know how large the subsequent fragments will be (and how much
          // they'll expand the row).
          //
          // Instead of using the size of the fragment, expand the row to the
          // rest of the fragmentainer, with an additional epsilon. This epsilon
          // will ensure that we continue layout for children in this row in
          // the next fragmentainer. Without it we'd drop those subsequent
          // fragments.
          item_expansion =
              (fragmentainer_space - grid_area.BlockEndOffset()).AddEpsilon();
        } else {
          item_expansion = fragment.BlockSize() - grid_area.BlockEndOffset();
        }

        max_row_expansion = std::max(max_row_expansion, item_expansion);
      }
    }
  };

  // Adjust by |delta| the pre-computed item-offset for all grid items with a
  // row begin index greater or equal than |row_index|.
  auto AdjustItemOffsets = [&](wtf_size_t row_index, LayoutUnit delta) {
    auto current_item = grid_items.begin();

    for (auto& item_placement_data : *grid_items_placement_data) {
      if (row_index <= (current_item++)->SetIndices(kForRows).begin)
        item_placement_data.offset.block_offset += delta;
    }
  };

  // Adjust our grid break-token data to accommodate the larger item in the row.
  // Returns true if this function adjusted the break-token data in any way.
  auto ExpandRow = [&]() -> bool {
    if (max_row_expansion == 0)
      return false;

    DCHECK_GT(max_row_expansion, 0);
    DCHECK(IsExpansionMakingProgress(expansion_row_set_index));

    *intrinsic_block_size += max_row_expansion;
    AdjustItemOffsets(expansion_row_set_index + 1, max_row_expansion);
    layout_data->Rows().AdjustSetOffsets(expansion_row_set_index + 1,
                                         max_row_expansion);

    previous_expansion_row_set_index = expansion_row_set_index;
    return true;
  };

  // Shifts the row where we wish to take a breakpoint (indicated by
  // |breakpoint_row_set_index|) into the next fragmentainer.
  // Returns true if this function adjusted the break-token data in any way.
  auto ShiftBreakpointIntoNextFragmentainer = [&]() -> bool {
    if (breakpoint_row_set_index == kNotFound)
      return false;

    LayoutUnit row_offset =
        layout_data->Rows().GetSetOffset(breakpoint_row_set_index) +
        (*row_offset_adjustments)[breakpoint_row_set_index];

    const LayoutUnit fragment_relative_row_offset =
        row_offset - *consumed_grid_block_size;

    // We may be within the initial column-balancing pass (where we have an
    // indefinite fragmentainer size). If we have a forced break, re-run
    // |PlaceItems()| assuming the breakpoint offset is the fragmentainer size.
    if (fragmentainer_space == kIndefiniteSize) {
      fragmentainer_space = fragment_relative_row_offset;
      return true;
    }

    const LayoutUnit row_offset_delta =
        fragmentainer_space - fragment_relative_row_offset;

    // An expansion may have occurred in |ExpandRow| which already pushed this
    // row into the next fragmentainer.
    if (row_offset_delta <= LayoutUnit())
      return false;

    row_offset += row_offset_delta;
    *intrinsic_block_size += row_offset_delta;
    AdjustItemOffsets(breakpoint_row_set_index, row_offset_delta);

    auto* it = row_offset_adjustments->begin() + breakpoint_row_set_index;
    while (it != row_offset_adjustments->end())
      *(it++) += row_offset_delta;

    // For any items that ran out of fragmentainer-space, make them block-start
    // aligned (as they may be center/end aligned, and still not have enough
    // space).
    for (auto* item_placement_data : out_of_fragmentainer_space_item_placement)
      item_placement_data->offset.block_offset = row_offset;

    return true;
  };

  PlaceItems();

  // See if we need to expand any rows, and if so re-run |PlaceItems()|. We
  // track the previous row we expanded, so this loop should eventually break.
  while (ExpandRow())
    PlaceItems();

  // See if we need to take a row break-point, and if-so re-run |PlaceItems()|.
  // We only need to do this once.
  if (ShiftBreakpointIntoNextFragmentainer())
    PlaceItems();

  if (has_subsequent_children)
    container_builder_.SetHasSubsequentChildren();

  // Add all the results into the builder.
  for (auto& result_and_offset : result_and_offsets) {
    container_builder_.AddResult(*result_and_offset.result,
                                 result_and_offset.offset,
                                 result_and_offset.relative_offset);
  }

  // Propagate the baselines.
  if (auto first_baseline = baseline_accumulator.FirstBaseline())
    container_builder_.SetFirstBaseline(*first_baseline);
  if (auto last_baseline = baseline_accumulator.LastBaseline())
    container_builder_.SetLastBaseline(*last_baseline);

  if (fragmentainer_space != kIndefiniteSize)
    *consumed_grid_block_size += fragmentainer_space;
}

void NGGridLayoutAlgorithm::PlaceOutOfFlowItems(
    const NGGridLayoutData& layout_data,
    const LayoutUnit block_size,
    HeapVector<Member<LayoutBox>>& oof_children) {
  DCHECK(!oof_children.empty());

  HeapVector<Member<LayoutBox>> oofs;
  std::swap(oofs, oof_children);

  bool should_process_block_end = true;
  if (UNLIKELY(InvolvedInBlockFragmentation(container_builder_))) {
    should_process_block_end = !container_builder_.DidBreakSelf() &&
                               !container_builder_.ShouldBreakInside();
  }

  const auto& node = Node();
  const auto& container_style = Style();
  const auto& placement_data = node.CachedPlacementData();

  const LayoutUnit previous_consumed_block_size =
      BreakToken() ? BreakToken()->ConsumedBlockSize() : LayoutUnit();
  const LogicalSize total_fragment_size = {container_builder_.InlineSize(),
                                           block_size};
  const auto default_containing_block_size =
      ShrinkLogicalSize(total_fragment_size, BorderScrollbarPadding());

  for (LayoutBox* oof_child : oofs) {
    NGBlockNode child(oof_child);
    DCHECK(child.IsOutOfFlowPositioned());

    absl::optional<LogicalRect> containing_block_rect;
    GridItemData out_of_flow_item(child, container_style);

    // TODO(layout-dev): If the below ends up being removed (as a result of
    // [1]), we could likely implement some of the same optimizations as OOFs in
    // flex [2] (i.e. checking `should_process_block_end` and
    // `should_process_block_center` earlier on). However, given that with
    // grid-area, the static position can be in any fragment, these
    // optimizations would overcomplicate the logic.
    //
    // [1] https://github.com/w3c/csswg-drafts/issues/7661
    // [2] https://chromium-review.googlesource.com/c/chromium/src/+/3927797
    if (out_of_flow_item.IsGridContainingBlock()) {
      containing_block_rect = ComputeOutOfFlowItemContainingRect(
          placement_data, layout_data, container_style,
          container_builder_.Borders(), total_fragment_size, &out_of_flow_item);
    }

    auto child_offset = containing_block_rect
                            ? containing_block_rect->offset
                            : BorderScrollbarPadding().StartOffset();
    const auto containing_block_size = containing_block_rect
                                           ? containing_block_rect->size
                                           : default_containing_block_size;

    NGLogicalStaticPosition::InlineEdge inline_edge;
    NGLogicalStaticPosition::BlockEdge block_edge;

    AlignmentOffsetForOutOfFlow(out_of_flow_item.InlineAxisAlignment(),
                                out_of_flow_item.BlockAxisAlignment(),
                                containing_block_size, &inline_edge,
                                &block_edge, &child_offset);

    // Make the child offset relative to our fragment.
    child_offset.block_offset -= previous_consumed_block_size;

    // We will attempt to add OOFs in the fragment in which their static
    // position belongs. However, the last fragment has the most up-to-date grid
    // geometry information (e.g. any expanded rows, etc), so for center aligned
    // items or items with a grid-area that is not in the first or last
    // fragment, we could end up with an incorrect static position.
    if (should_process_block_end ||
        child_offset.block_offset <= FragmentainerCapacity(ConstraintSpace())) {
      container_builder_.AddOutOfFlowChildCandidate(
          out_of_flow_item.node, child_offset, inline_edge, block_edge);
    } else {
      oof_children.emplace_back(oof_child);
    }
  }
}

namespace {

Vector<std::div_t> ComputeTrackSizesInRange(
    const NGGridLayoutTrackCollection& track_collection,
    const wtf_size_t range_begin_set_index,
    const wtf_size_t range_set_count) {
  Vector<std::div_t> track_sizes;
  track_sizes.ReserveInitialCapacity(range_set_count);

  const wtf_size_t ending_set_index = range_begin_set_index + range_set_count;
  for (wtf_size_t i = range_begin_set_index; i < ending_set_index; ++i) {
    // Set information is stored as offsets. To determine the size of a single
    // track in a givent set, first determine the total size the set takes up
    // by finding the difference between the offsets and subtracting the gutter
    // size for each track in the set.
    LayoutUnit set_size =
        track_collection.GetSetOffset(i + 1) - track_collection.GetSetOffset(i);
    const wtf_size_t set_track_count = track_collection.GetSetTrackCount(i);

    DCHECK_GE(set_size, 0);
    set_size = (set_size - track_collection.GutterSize() * set_track_count)
                   .ClampNegativeToZero();

    // Once we have determined the size of the set, we can find the size of a
    // given track by dividing the |set_size| by the |set_track_count|.
    DCHECK_GT(set_track_count, 0u);
    track_sizes.emplace_back(std::div(set_size.RawValue(), set_track_count));
  }
  return track_sizes;
}

// For out of flow items that are located in the middle of a range, computes
// the extra offset relative to the start of its containing range.
LayoutUnit ComputeTrackOffsetInRange(
    const NGGridLayoutTrackCollection& track_collection,
    const wtf_size_t range_begin_set_index,
    const wtf_size_t range_set_count,
    const wtf_size_t offset_in_range) {
  if (!range_set_count || !offset_in_range)
    return LayoutUnit();

  // To compute the index offset, we have to determine the size of the
  // tracks within the grid item's span.
  Vector<std::div_t> track_sizes = ComputeTrackSizesInRange(
      track_collection, range_begin_set_index, range_set_count);

  // Calculate how many sets there are from the start of the range to the
  // |offset_in_range|. This division can produce a remainder, which would
  // mean that not all of the sets are repeated the same amount of times from
  // the start to the |offset_in_range|.
  const wtf_size_t floor_set_track_count = offset_in_range / range_set_count;
  const wtf_size_t remaining_track_count = offset_in_range % range_set_count;

  // Iterate over the sets and add the sizes of the tracks to |index_offset|.
  LayoutUnit index_offset = track_collection.GutterSize() * offset_in_range;
  for (wtf_size_t i = 0; i < track_sizes.size(); ++i) {
    // If we have a remainder from the |floor_set_track_count|, we have to
    // consider it to get the correct offset.
    const wtf_size_t set_count =
        floor_set_track_count + ((remaining_track_count > i) ? 1 : 0);
    index_offset +=
        LayoutUnit::FromRawValue(std::min<int>(set_count, track_sizes[i].rem) +
                                 (set_count * track_sizes[i].quot));
  }
  return index_offset;
}

template <bool snap_to_end_of_track>
LayoutUnit TrackOffset(const NGGridLayoutTrackCollection& track_collection,
                       const wtf_size_t range_index,
                       const wtf_size_t offset_in_range) {
  const wtf_size_t range_begin_set_index =
      track_collection.RangeBeginSetIndex(range_index);
  const wtf_size_t range_track_count =
      track_collection.RangeTrackCount(range_index);
  const wtf_size_t range_set_count =
      track_collection.RangeSetCount(range_index);

  LayoutUnit track_offset;
  if (offset_in_range == range_track_count) {
    DCHECK(snap_to_end_of_track);
    track_offset =
        track_collection.GetSetOffset(range_begin_set_index + range_set_count);
  } else {
    DCHECK(offset_in_range || !snap_to_end_of_track);
    DCHECK_LT(offset_in_range, range_track_count);

    // If an out of flow item starts/ends in the middle of a range, compute and
    // add the extra offset to the start offset of the range.
    track_offset =
        track_collection.GetSetOffset(range_begin_set_index) +
        ComputeTrackOffsetInRange(track_collection, range_begin_set_index,
                                  range_set_count, offset_in_range);
  }

  // |track_offset| includes the gutter size at the end of the last track,
  // when we snap to the end of last track such gutter size should be removed.
  // However, only snap if this range is not collapsed or if it can snap to the
  // end of the last track in the previous range of the collection.
  if (snap_to_end_of_track && (range_set_count || range_index))
    track_offset -= track_collection.GutterSize();
  return track_offset;
}

LayoutUnit TrackStartOffset(const NGGridLayoutTrackCollection& track_collection,
                            const wtf_size_t range_index,
                            const wtf_size_t offset_in_range) {
  if (!track_collection.RangeCount()) {
    // If the start line of an out of flow item is not 'auto' in an empty and
    // undefined grid, start offset is the start border scrollbar padding.
    DCHECK_EQ(range_index, 0u);
    DCHECK_EQ(offset_in_range, 0u);
    return track_collection.GetSetOffset(0);
  }

  const wtf_size_t range_track_count =
      track_collection.RangeTrackCount(range_index);

  if (offset_in_range == range_track_count &&
      range_index == track_collection.RangeCount() - 1) {
    // The only case where we allow the offset to be equal to the number of
    // tracks in the range is for the last range in the collection, which should
    // match the end line of the implicit grid; snap to the track end instead.
    return TrackOffset</* snap_to_end_of_track */ true>(
        track_collection, range_index, offset_in_range);
  }

  DCHECK_LT(offset_in_range, range_track_count);
  return TrackOffset</* snap_to_end_of_track */ false>(
      track_collection, range_index, offset_in_range);
}

LayoutUnit TrackEndOffset(const NGGridLayoutTrackCollection& track_collection,
                          const wtf_size_t range_index,
                          const wtf_size_t offset_in_range) {
  if (!track_collection.RangeCount()) {
    // If the end line of an out of flow item is not 'auto' in an empty and
    // undefined grid, end offset is the start border scrollbar padding.
    DCHECK_EQ(range_index, 0u);
    DCHECK_EQ(offset_in_range, 0u);
    return track_collection.GetSetOffset(0);
  }

  if (!offset_in_range && !range_index) {
    // Only allow the offset to be 0 for the first range in the collection,
    // which is the start line of the implicit grid; don't snap to the end.
    return TrackOffset</* snap_to_end_of_track */ false>(
        track_collection, range_index, offset_in_range);
  }

  DCHECK_GT(offset_in_range, 0u);
  return TrackOffset</* snap_to_end_of_track */ true>(
      track_collection, range_index, offset_in_range);
}

void ComputeOutOfFlowOffsetAndSize(
    const GridItemData& out_of_flow_item,
    const NGGridLayoutTrackCollection& track_collection,
    const NGBoxStrut& borders,
    const LogicalSize& border_box_size,
    LayoutUnit* start_offset,
    LayoutUnit* size) {
  DCHECK(start_offset && size && out_of_flow_item.IsOutOfFlow());
  OutOfFlowItemPlacement item_placement;
  LayoutUnit end_offset;

  // The default padding box value for |size| is used for out of flow items in
  // which both the start line and end line are defined as 'auto'.
  if (track_collection.Direction() == kForColumns) {
    item_placement = out_of_flow_item.column_placement;
    *start_offset = borders.inline_start;
    end_offset = border_box_size.inline_size - borders.inline_end;
  } else {
    item_placement = out_of_flow_item.row_placement;
    *start_offset = borders.block_start;
    end_offset = border_box_size.block_size - borders.block_end;
  }

  // If the start line is defined, the size will be calculated by subtracting
  // the offset at |start_index|; otherwise, use the computed border start.
  if (item_placement.range_index.begin != kNotFound) {
    DCHECK_NE(item_placement.offset_in_range.begin, kNotFound);

    *start_offset =
        TrackStartOffset(track_collection, item_placement.range_index.begin,
                         item_placement.offset_in_range.begin);
  }

  // If the end line is defined, the offset (which can be the offset at the
  // start index or the start border) and the added grid gap after the spanned
  // tracks are subtracted from the offset at the end index.
  if (item_placement.range_index.end != kNotFound) {
    DCHECK_NE(item_placement.offset_in_range.end, kNotFound);

    end_offset =
        TrackEndOffset(track_collection, item_placement.range_index.end,
                       item_placement.offset_in_range.end);
  }

  // |start_offset| can be greater than |end_offset| if the used track sizes or
  // gutter size saturated the set offsets of the track collection.
  *size = (end_offset - *start_offset).ClampNegativeToZero();
}

}  // namespace

void NGGridLayoutAlgorithm::ComputeGridItemOffsetAndSize(
    const GridItemData& grid_item,
    const NGGridLayoutTrackCollection& track_collection,
    LayoutUnit* start_offset,
    LayoutUnit* size) const {
  DCHECK(start_offset && size && !grid_item.IsOutOfFlow());

  const auto& set_indices = grid_item.SetIndices(track_collection.Direction());
  *start_offset = track_collection.GetSetOffset(set_indices.begin);
  *size =
      track_collection.ComputeSetSpanSize(set_indices.begin, set_indices.end);

  if (size->MightBeSaturated())
    *size = LayoutUnit();
}

// static
LogicalRect NGGridLayoutAlgorithm::ComputeOutOfFlowItemContainingRect(
    const NGGridPlacementData& placement_data,
    const NGGridLayoutData& layout_data,
    const ComputedStyle& grid_style,
    const NGBoxStrut& borders,
    const LogicalSize& border_box_size,
    GridItemData* out_of_flow_item) {
  DCHECK(out_of_flow_item && out_of_flow_item->IsOutOfFlow());

  out_of_flow_item->ComputeOutOfFlowItemPlacement(layout_data.Columns(),
                                                  placement_data, grid_style);
  out_of_flow_item->ComputeOutOfFlowItemPlacement(layout_data.Rows(),
                                                  placement_data, grid_style);

  LogicalRect containing_rect;

  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, layout_data.Columns(), borders, border_box_size,
      &containing_rect.offset.inline_offset, &containing_rect.size.inline_size);

  ComputeOutOfFlowOffsetAndSize(
      *out_of_flow_item, layout_data.Rows(), borders, border_box_size,
      &containing_rect.offset.block_offset, &containing_rect.size.block_size);

  return containing_rect;
}

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::ResultAndOffsets)
