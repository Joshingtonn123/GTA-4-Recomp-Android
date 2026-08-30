/**
 * @file native_frame_scheduling_test.cpp
 * @brief Texture eviction policy tests for the GTA IV native renderer.
 */

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <unordered_set>
#include <vector>

#include "graphics/gta4_native/native_frame_scheduling.h"

namespace gta4 = rex::graphics::gta4_native;

TEST_CASE("GTA IV native texture eviction waits out the full grace window") {
  constexpr uint32_t kGrace = gta4::kNativeTextureEvictionGraceFrames;
  const uint32_t last_used = 1000;
  // Referenced this frame or within the grace window: kept.
  REQUIRE_FALSE(gta4::ShouldEvictNativeTexture(last_used, last_used, kGrace));
  REQUIRE_FALSE(gta4::ShouldEvictNativeTexture(last_used + kGrace, last_used, kGrace));
  // One frame past the grace window: evicted.
  REQUIRE(gta4::ShouldEvictNativeTexture(last_used + kGrace + 1, last_used, kGrace));
  // A frame counter that moved backwards (title reset) must never evict, so a
  // stale stamp can only delay eviction, never destroy a live image.
  REQUIRE_FALSE(gta4::ShouldEvictNativeTexture(last_used - 1, last_used, kGrace));
  REQUIRE_FALSE(gta4::ShouldEvictNativeTexture(0, UINT32_MAX, kGrace));
}

TEST_CASE("GTA IV native no-op texture unlock clears the dirty capture state") {
  REQUIRE(gta4::ShouldClearNativeTextureDirtyFlag(true, true));
  REQUIRE_FALSE(gta4::ShouldClearNativeTextureDirtyFlag(false, true));
  REQUIRE_FALSE(gta4::ShouldClearNativeTextureDirtyFlag(true, false));
}

TEST_CASE("GTA IV native buffer capture reuses only clean matching metadata") {
  REQUIRE(gta4::CanReuseNativeBufferCapture(true, false, true));
  REQUIRE_FALSE(gta4::CanReuseNativeBufferCapture(false, false, true));
  REQUIRE_FALSE(gta4::CanReuseNativeBufferCapture(true, true, true));
  REQUIRE_FALSE(gta4::CanReuseNativeBufferCapture(true, false, false));
}

TEST_CASE("GTA IV native buffer shadow sampling validates complete selected payloads") {
  std::array<uint8_t, 1024> captured{};
  for (size_t index = 0; index < captured.size(); ++index) {
    captured[index] = uint8_t(index);
  }
  auto guest = captured;
  REQUIRE(gta4::NativeBufferShadowPayloadMatches(guest.data(), captured.data(), captured.size()));
  guest[777] ^= 1;
  REQUIRE_FALSE(
      gta4::NativeBufferShadowPayloadMatches(guest.data(), captured.data(), captured.size()));

  std::array<uint8_t, 32> small{};
  auto small_guest = small;
  small_guest[7] = 1;
  REQUIRE_FALSE(
      gta4::NativeBufferShadowPayloadMatches(small_guest.data(), small.data(), small.size()));
  REQUIRE_FALSE(gta4::NativeBufferShadowPayloadMatches(nullptr, small.data(), small.size()));
}

TEST_CASE("GTA IV native buffer shadow validation cadence is deterministic") {
  REQUIRE(gta4::ShouldValidateNativeBufferShadow(1));
  REQUIRE_FALSE(gta4::ShouldValidateNativeBufferShadow(2));
  REQUIRE_FALSE(gta4::ShouldValidateNativeBufferShadow(64));
  REQUIRE(gta4::ShouldValidateNativeBufferShadow(65));
  REQUIRE(gta4::ShouldValidateNativeBufferShadow(129));
  REQUIRE(gta4::ShouldDisableNativeBufferFastPath(true, false));
  REQUIRE_FALSE(gta4::ShouldDisableNativeBufferFastPath(true, true));
  REQUIRE_FALSE(gta4::ShouldDisableNativeBufferFastPath(false, false));
}

TEST_CASE("GTA IV native texture eviction protects references and handles frame resets") {
  constexpr uint32_t kGrace = gta4::kNativeTextureEvictionGraceFrames;
  REQUIRE_FALSE(gta4::ShouldEvictNativeTextureCandidate(true, true, 2000, 1, kGrace));
  REQUIRE_FALSE(gta4::ShouldEvictNativeTextureCandidate(false, true, 2000, 2000, kGrace));
  REQUIRE(gta4::ShouldEvictNativeTextureCandidate(false, true, 2000, 2000, 0));
  REQUIRE_FALSE(gta4::ShouldEvictNativeTextureCandidate(false, false, 999, 1000, kGrace));
  REQUIRE_FALSE(gta4::ShouldEvictNativeTextureCandidate(false, false, 1200, 1000, kGrace));
  REQUIRE(gta4::ShouldEvictNativeTextureCandidate(false, false, 2000, 1000, kGrace));
  REQUIRE(gta4::ShouldEvictNativeTextureCandidate(false, true, 2000, 1000, kGrace));
}

TEST_CASE("GTA IV native buffer cache reclaims only unreferenced old or over-budget entries") {
  REQUIRE_FALSE(gta4::ShouldReclaimNativeBuffer(false, true, 2000, 1000, 600));
  REQUIRE_FALSE(gta4::ShouldReclaimNativeBuffer(true, false, 1600, 1000, 600));
  REQUIRE(gta4::ShouldReclaimNativeBuffer(true, false, 1601, 1000, 600));
  REQUIRE(gta4::ShouldReclaimNativeBuffer(true, true, 1000, 1000, 600));
  REQUIRE_FALSE(gta4::ShouldReclaimNativeBuffer(true, false, 999, 1000, 600));
}

TEST_CASE("GTA IV native upload buffer shrink uses capacity and time hysteresis") {
  REQUIRE_FALSE(gta4::ShouldShrinkNativeUploadBuffer(32, 32, 16, 32, 120, 120));
  REQUIRE_FALSE(gta4::ShouldShrinkNativeUploadBuffer(256, 32, 120, 128, 119, 120));
  REQUIRE(gta4::ShouldShrinkNativeUploadBuffer(256, 32, 120, 128, 120, 120));
  REQUIRE_FALSE(gta4::ShouldShrinkNativeUploadBuffer(256, 32, 200, 208, 120, 120));
}

TEST_CASE("GTA IV native texture replacement retires every superseded generation") {
  std::unordered_set<uint64_t> retirements;
  uint64_t current_generation = 0;
  for (uint64_t replacement_generation = 1; replacement_generation <= 1000;
       ++replacement_generation) {
    if (gta4::ShouldRetireSupersededNativeTextureGeneration(current_generation,
                                                            replacement_generation)) {
      retirements.insert(current_generation);
    }
    current_generation = replacement_generation;
  }

  REQUIRE(retirements.size() == 999);
  REQUIRE(retirements.contains(1));
  REQUIRE(retirements.contains(999));
  REQUIRE_FALSE(retirements.contains(current_generation));
}

TEST_CASE("GTA IV native texture retirement survives pre-materialization references") {
  using Action = gta4::NativeTextureReleaseAction;
  REQUIRE(gta4::ClassifyNativeTextureRelease(true, false) == Action::kKeepPending);
  REQUIRE(gta4::ClassifyNativeTextureRelease(true, true) == Action::kKeepPending);
  REQUIRE(gta4::ClassifyNativeTextureRelease(false, true) == Action::kDestroyImage);
  REQUIRE(gta4::ClassifyNativeTextureRelease(false, false) == Action::kForget);
}

TEST_CASE("GTA IV native texture budget pressure is advisory and deterministic") {
  REQUIRE_FALSE(gta4::IsNativeTextureHeapUnderPressure(false, 1000, 1000));
  REQUIRE_FALSE(gta4::IsNativeTextureHeapUnderPressure(true, 999, 0));
  REQUIRE_FALSE(gta4::IsNativeTextureHeapUnderPressure(true, 899, 1000));
  REQUIRE(gta4::IsNativeTextureHeapUnderPressure(true, 900, 1000));
}

TEST_CASE("GTA IV native texture pressure uses enter and exit hysteresis") {
  REQUIRE_FALSE(gta4::UpdateNativeTexturePressure(false, true, 899, 1000));
  REQUIRE(gta4::UpdateNativeTexturePressure(false, true, 900, 1000));
  REQUIRE(gta4::UpdateNativeTexturePressure(true, true, 850, 1000));
  REQUIRE_FALSE(gta4::UpdateNativeTexturePressure(true, true, 849, 1000));
}

TEST_CASE("GTA IV native texture budget polling is bounded and reset safe") {
  REQUIRE_FALSE(gta4::ShouldPollNativeTextureBudget(119, 0, 120));
  REQUIRE(gta4::ShouldPollNativeTextureBudget(120, 0, 120));
  REQUIRE(gta4::ShouldPollNativeTextureBudget(1, 1000, 120));
  REQUIRE(gta4::ShouldPollNativeTextureBudget(1, 1, 0));
}

TEST_CASE("GTA IV native texture LRU orders by serial then generation") {
  std::vector<gta4::NativeTextureLruKey> keys = {{7, 9}, {3, 8}, {7, 4}, {3, 2}};
  std::sort(keys.begin(), keys.end());
  REQUIRE(keys[0].last_use_serial == 3);
  REQUIRE(keys[0].generation == 2);
  REQUIRE(keys[1].last_use_serial == 3);
  REQUIRE(keys[1].generation == 8);
  REQUIRE(keys[2].last_use_serial == 7);
  REQUIRE(keys[2].generation == 4);
  REQUIRE(keys[3].generation == 9);
}

TEST_CASE("GTA IV native texture allocation retries exactly once") {
  REQUIRE(gta4::ShouldRetryNativeTextureAllocation(0));
  REQUIRE_FALSE(gta4::ShouldRetryNativeTextureAllocation(1));
  REQUIRE_FALSE(gta4::ShouldRetryNativeTextureAllocation(2));
}
