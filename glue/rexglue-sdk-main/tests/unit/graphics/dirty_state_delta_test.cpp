/**
 * @file dirty_state_delta_test.cpp
 * @brief Dirty-state extraction and versioning tests for the GTA IV native renderer.
 */

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "graphics/gta4_native/dirty_state_delta.h"

namespace gta4 = rex::graphics::gta4_native;

namespace {

struct TestLayoutStorage {
  std::array<gta4::DirtyBitSpan, 2> vertex_constants{{{0, 0, 8, 0}, {0, 8, 8, 8}}};
  std::array<gta4::DirtyBitSpan, 2> pixel_constants{{{1, 60, 4, 12}, {2, 0, 4, 16}}};
  std::array<gta4::DirtyBitSpan, 1> texture_fetches{{{2, 8, 8, 0}}};
  std::array<gta4::DirtyBitSpan, 1> samplers{{{3, 16, 8, 4}}};
  std::array<gta4::DirtyBitSpan, 1> booleans{{{4, 0, 8, 0}}};
  std::array<gta4::DirtyBitSpan, 1> integers{{{4, 8, 8, 0}}};
  std::array<gta4::DirtyBitSpan, 1> fixed_state{{{3, 0, 8, 0}}};
  std::array<gta4::DirtyBitSpan, 1> dynamic_state{{{3, 8, 8, 0}}};

  gta4::DirtyStateLayout Layout() const {
    return {
        {vertex_constants, 16}, {pixel_constants, 20}, {texture_fetches, 8}, {samplers, 12},
        {booleans, 8},          {integers, 8},         {fixed_state, 8},     {dynamic_state, 8},
    };
  }
};

template <typename T, size_t Size>
std::span<const std::byte> Bytes(const std::array<T, Size>& values) {
  return std::as_bytes(std::span(values));
}

}  // namespace

TEST_CASE("GTA IV native dirty state produces typed coalesced ranges and masks") {
  const TestLayoutStorage storage;
  const gta4::NativeDirtyWords words = {
      0x000000000000039C, 0xC000000000000000, 0x0000000000000503,
      0x00000000000B0609, 0x0000000000008182,
  };
  const gta4::DirtyDeltaBuildResult build = gta4::BuildDirtyStateDelta(words, storage.Layout());

  REQUIRE(build.valid());
  REQUIRE(build.delta.vertex_constant_ranges.ranges ==
          std::vector<gta4::DirtyElementRange>{{2, 3}, {7, 3}});
  REQUIRE(build.delta.pixel_constant_ranges.ranges ==
          std::vector<gta4::DirtyElementRange>{{14, 4}});
  REQUIRE(build.delta.texture_fetch_stage_ranges.ranges ==
          std::vector<gta4::DirtyElementRange>{{0, 1}, {2, 1}});
  REQUIRE(build.delta.sampler_stage_ranges.ranges ==
          std::vector<gta4::DirtyElementRange>{{4, 2}, {7, 1}});
  REQUIRE(build.delta.boolean_constant_mask.words == std::vector<uint64_t>{0x82});
  REQUIRE(build.delta.integer_constant_mask.words == std::vector<uint64_t>{0x81});
  REQUIRE(build.delta.fixed_state_mask.words == std::vector<uint64_t>{0x09});
  REQUIRE(build.delta.dynamic_state_mask.words == std::vector<uint64_t>{0x06});
  REQUIRE(build.delta.TouchedComponents() == gta4::AllDirtyStateComponents());
}

TEST_CASE("GTA IV native dirty state handles bit 63 and masks larger than one word") {
  const std::array<gta4::DirtyBitSpan, 1> fixed_spans{{{4, 63, 1, 70}}};
  gta4::DirtyStateLayout layout;
  layout.fixed_state = {fixed_spans, 71};
  gta4::NativeDirtyWords words{};
  words[4] = uint64_t{1} << 63;

  const gta4::DirtyDeltaBuildResult build = gta4::BuildDirtyStateDelta(words, layout);
  REQUIRE(build.valid());
  REQUIRE(build.delta.fixed_state_mask.words.size() == 2);
  REQUIRE(build.delta.fixed_state_mask.words[0] == 0);
  REQUIRE(build.delta.fixed_state_mask.words[1] == 0x40);
  REQUIRE(build.delta.fixed_state_mask.Test(70));
  REQUIRE_FALSE(build.delta.fixed_state_mask.Test(71));
}

TEST_CASE("GTA IV native dirty state reuses caller-owned delta storage") {
  const TestLayoutStorage storage;
  gta4::NativeDirtyWords words = {
      0x000000000000039C, 0xC000000000000000, 0x0000000000000503,
      0x00000000000B0609, 0x0000000000008182,
  };
  gta4::DirtyStateDelta delta;
  gta4::DirtyDeltaScratch scratch;
  REQUIRE(gta4::BuildDirtyStateDelta(words, storage.Layout(), delta, scratch).valid());
  const size_t scratch_capacity = scratch.dirty_elements.capacity();
  const size_t range_capacity = delta.vertex_constant_ranges.ranges.capacity();
  const size_t mask_capacity = delta.fixed_state_mask.words.capacity();

  words = {};
  REQUIRE(gta4::BuildDirtyStateDelta(words, storage.Layout(), delta, scratch).valid());
  REQUIRE_FALSE(delta.Any());
  REQUIRE(scratch.dirty_elements.capacity() == scratch_capacity);
  REQUIRE(delta.vertex_constant_ranges.ranges.capacity() == range_capacity);
  REQUIRE(delta.fixed_state_mask.words.capacity() == mask_capacity);

  const std::array<gta4::DirtyBitSpan, 1> invalid_span{{{5, 0, 1, 0}}};
  gta4::DirtyStateLayout invalid_layout;
  invalid_layout.fixed_state = {invalid_span, 1};
  REQUIRE_FALSE(gta4::BuildDirtyStateDelta(words, invalid_layout, delta, scratch).valid());
  REQUIRE_FALSE(delta.Any());
  REQUIRE(delta.fixed_state_mask.element_count == 0);
}

TEST_CASE("GTA IV native dirty layout validation identifies the exact bad span") {
  const std::array<gta4::DirtyBitSpan, 2> spans{{{0, 0, 1, 0}, {5, 0, 1, 1}}};
  gta4::DirtyStateLayout layout;
  layout.vertex_constants = {spans, 2};
  const gta4::DirtyLayoutValidationResult validation = gta4::ValidateDirtyStateLayout(layout);

  REQUIRE_FALSE(validation.valid());
  REQUIRE(validation.error == gta4::DirtyLayoutError::kDirtyWordOutOfRange);
  REQUIRE(validation.component == gta4::DirtyStateComponent::kVertexConstants);
  REQUIRE(validation.span_index == 1);

  const std::array<gta4::DirtyBitSpan, 1> bit_overflow{{{0, 63, 2, 0}}};
  layout.vertex_constants = {bit_overflow, 2};
  REQUIRE(gta4::ValidateDirtyStateLayout(layout).error ==
          gta4::DirtyLayoutError::kDirtyBitRangeOutOfRange);

  const std::array<gta4::DirtyBitSpan, 1> element_overflow{{{0, 0, 2, 1}}};
  layout.vertex_constants = {element_overflow, 2};
  REQUIRE(gta4::ValidateDirtyStateLayout(layout).error ==
          gta4::DirtyLayoutError::kElementRangeOutOfRange);
}

TEST_CASE("GTA IV native component versions advance once per dirty delta") {
  const TestLayoutStorage storage;
  gta4::NativeDirtyWords words{};
  words[0] = 0x03;
  words[3] = 0x01;
  const gta4::DirtyStateDelta delta = gta4::BuildDirtyStateDelta(words, storage.Layout()).delta;
  gta4::StateVersionVector versions;

  const gta4::StateVersionUpdateResult first = gta4::ApplyDirtyStateVersions(versions, delta);
  REQUIRE(first.applied());
  REQUIRE(versions.Get(gta4::DirtyStateComponent::kVertexConstants).revision == 1);
  REQUIRE(versions.Get(gta4::DirtyStateComponent::kFixedState).revision == 1);
  REQUIRE(versions.Get(gta4::DirtyStateComponent::kPixelConstants).revision == 0);

  const gta4::StateVersionUpdateResult second = gta4::ApplyDirtyStateVersions(versions, delta);
  REQUIRE(second.applied());
  REQUIRE(versions.Get(gta4::DirtyStateComponent::kVertexConstants).revision == 2);
  REQUIRE(versions.Get(gta4::DirtyStateComponent::kFixedState).revision == 2);

  const gta4::DirtyStateDelta empty_delta;
  const gta4::StateVersionVector before_empty = versions;
  REQUIRE(gta4::ApplyDirtyStateVersions(versions, empty_delta).applied());
  REQUIRE(versions.components == before_empty.components);
}

TEST_CASE("GTA IV native version exhaustion is atomic and rollover is explicit") {
  gta4::DirtyStateDelta delta;
  delta.fixed_state_mask.element_count = 1;
  delta.fixed_state_mask.words = {1};

  gta4::StateVersionVector rollover_versions;
  auto& rollover =
      rollover_versions.components[static_cast<size_t>(gta4::DirtyStateComponent::kFixedState)];
  rollover.revision = std::numeric_limits<uint64_t>::max();
  const gta4::StateVersionUpdateResult rolled =
      gta4::ApplyDirtyStateVersions(rollover_versions, delta);
  REQUIRE(rolled.applied());
  REQUIRE(rollover.epoch == 1);
  REQUIRE(rollover.revision == 0);

  gta4::StateVersionVector exhausted_versions;
  auto& exhausted =
      exhausted_versions.components[static_cast<size_t>(gta4::DirtyStateComponent::kFixedState)];
  exhausted.epoch = std::numeric_limits<uint64_t>::max();
  exhausted.revision = std::numeric_limits<uint64_t>::max();
  const gta4::StateVersionVector before = exhausted_versions;
  const gta4::StateVersionUpdateResult rejected =
      gta4::ApplyDirtyStateVersions(exhausted_versions, delta);
  REQUIRE_FALSE(rejected.applied());
  REQUIRE(rejected.status == gta4::StateVersionUpdateStatus::kVersionSpaceExhausted);
  REQUIRE(exhausted_versions.components == before.components);
}

TEST_CASE("GTA IV native draw state reuse ignores unrelated component changes") {
  gta4::StateVersionVector versions;
  const gta4::DirtyStateComponentMask dependencies =
      gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kVertexConstants) |
      gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kSamplerStages);
  const gta4::DrawStateVersionToken token =
      gta4::CaptureDrawStateVersionToken(versions, dependencies);
  REQUIRE(gta4::EvaluateDrawStateReuse(token, versions, dependencies).reusable());

  ++versions.components[static_cast<size_t>(gta4::DirtyStateComponent::kDynamicState)].revision;
  REQUIRE(gta4::EvaluateDrawStateReuse(token, versions, dependencies).reusable());

  ++versions.components[static_cast<size_t>(gta4::DirtyStateComponent::kSamplerStages)].revision;
  const gta4::DrawStateReuseDecision stale =
      gta4::EvaluateDrawStateReuse(token, versions, dependencies);
  REQUIRE_FALSE(stale.reusable());
  REQUIRE(stale.status == gta4::DrawStateReuseStatus::kComponentVersionMismatch);
  REQUIRE(stale.mismatched_components ==
          gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kSamplerStages));

  const auto smaller_dependencies =
      gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kVertexConstants);
  REQUIRE(gta4::EvaluateDrawStateReuse(token, versions, smaller_dependencies).status ==
          gta4::DrawStateReuseStatus::kDependencySetMismatch);
  REQUIRE_FALSE(gta4::EvaluateDrawStateReuse({}, versions, dependencies).reusable());
}

TEST_CASE("GTA IV native diagnostic comparison finds snapshot semantic divergence") {
  const std::array<uint32_t, 3> matching_delta = {10, 20, 30};
  const std::array<uint32_t, 3> matching_snapshot = matching_delta;
  const std::array<uint8_t, 4> divergent_delta = {1, 2, 3, 4};
  const std::array<uint8_t, 4> divergent_snapshot = {1, 2, 9, 4};
  const std::array<uint16_t, 2> short_delta = {100, 200};
  const std::array<uint16_t, 3> long_snapshot = {100, 200, 300};

  gta4::DiagnosticSemanticStateView delta_view;
  gta4::DiagnosticSemanticStateView snapshot_view;
  const size_t vertex_index = static_cast<size_t>(gta4::DirtyStateComponent::kVertexConstants);
  const size_t fixed_index = static_cast<size_t>(gta4::DirtyStateComponent::kFixedState);
  const size_t dynamic_index = static_cast<size_t>(gta4::DirtyStateComponent::kDynamicState);
  delta_view[vertex_index] = Bytes(matching_delta);
  snapshot_view[vertex_index] = Bytes(matching_snapshot);
  delta_view[fixed_index] = Bytes(divergent_delta);
  snapshot_view[fixed_index] = Bytes(divergent_snapshot);
  delta_view[dynamic_index] = Bytes(short_delta);
  snapshot_view[dynamic_index] = Bytes(long_snapshot);

  const gta4::DiagnosticSemanticComparisonResult comparison =
      gta4::CompareDiagnosticSemanticState(delta_view, snapshot_view);
  REQUIRE_FALSE(comparison.equivalent());
  REQUIRE(comparison.mismatched_components ==
          (gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kFixedState) |
           gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kDynamicState)));
  REQUIRE(comparison.details[fixed_index].first_mismatch_offset == 2);
  REQUIRE(comparison.details[dynamic_index].first_mismatch_offset == Bytes(short_delta).size());
  REQUIRE(comparison.details[vertex_index].first_mismatch_offset == gta4::kNoSemanticMismatch);

  const auto vertex_only =
      gta4::DirtyStateComponentBit(gta4::DirtyStateComponent::kVertexConstants);
  REQUIRE(
      gta4::CompareDiagnosticSemanticState(delta_view, snapshot_view, vertex_only).equivalent());
}
