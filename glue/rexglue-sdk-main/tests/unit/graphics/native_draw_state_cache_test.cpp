#include <catch2/catch_test_macros.hpp>

#include "graphics/gta4_native/native_draw_state_cache.h"

namespace {
using Cache = rex::graphics::gta4_native::NativeDrawStateCache<6>;

struct Draw {
  uint64_t pipeline = 1;
  uint64_t layout = 1;
  std::array<uint64_t, 6> descriptors{1, 2, 3, 4, 5, 6};
  std::array<uint32_t, 6> viewport{0, 0, 640, 480, 0, 1};
  std::array<int64_t, 4> scissor{0, 0, 640, 480};
  std::array<uint32_t, 3> depth_bias{};
  std::array<uint32_t, 6> stencil{1, 255, 255, 1, 255, 255};
  std::array<uint32_t, 4> blend{};
  std::array<uint64_t, 3> constants{100, 200, 300};
  bool operator==(const Draw&) const = default;
};

// Model the state reaching a draw through only the commands selected by the
// cache. Compare that result to unconditionally recording all requested state.
void Emit(Cache& cache, const Draw& request, Draw& recorded) {
  if (cache.UpdatePipeline(request.pipeline)) recorded.pipeline = request.pipeline;
  if (cache.UpdateDescriptors(request.layout, request.descriptors)) {
    recorded.layout = request.layout;
    recorded.descriptors = request.descriptors;
  }
  if (cache.UpdateViewport(request.viewport)) recorded.viewport = request.viewport;
  if (cache.UpdateScissor(request.scissor)) recorded.scissor = request.scissor;
  if (cache.UpdateDepthBias(request.depth_bias)) recorded.depth_bias = request.depth_bias;
  if (cache.UpdateStencil(request.stencil)) recorded.stencil = request.stencil;
  if (cache.UpdateBlendConstants(request.blend)) recorded.blend = request.blend;
  if (cache.UpdatePushConstants(request.layout, request.constants)) {
    recorded.constants = request.constants;
  }
}
}  // namespace

TEST_CASE("native draw cache preserves state across draw and external command transitions",
          "[gta4-native][draw-state]") {
  Cache cache;
  Draw requested;
  Draw recorded;
  Emit(cache, requested, recorded);
  REQUIRE(recorded == requested);
  REQUIRE_FALSE(cache.UpdatePipeline(requested.pipeline));
  REQUIRE_FALSE(cache.UpdateDescriptors(requested.layout, requested.descriptors));
  REQUIRE_FALSE(cache.UpdateViewport(requested.viewport));
  REQUIRE_FALSE(cache.UpdateScissor(requested.scissor));
  REQUIRE_FALSE(cache.UpdateDepthBias(requested.depth_bias));
  REQUIRE_FALSE(cache.UpdateStencil(requested.stencil));
  REQUIRE_FALSE(cache.UpdateBlendConstants(requested.blend));
  REQUIRE_FALSE(cache.UpdatePushConstants(requested.layout, requested.constants));

  SECTION("each cached descriptor including the storage set can change") {
    for (size_t set = 0; set < requested.descriptors.size(); ++set) {
      requested.descriptors[set] = 99;
      Emit(cache, requested, recorded);
      REQUIRE(recorded == requested);
    }
  }
  SECTION("pipeline layouts change even with identical descriptor handles") {
    requested.layout = 2;
    Emit(cache, requested, recorded);
    REQUIRE(recorded == requested);
  }
  SECTION("front and back stencil fields change independently") {
    for (size_t field = 0; field < requested.stencil.size(); ++field) {
      requested.stencil[field] = 7;
      Emit(cache, requested, recorded);
      REQUIRE(recorded == requested);
    }
  }
  SECTION("float comparisons retain signed zero and NaN bit patterns") {
    requested.viewport[0] = 0x80000000u;
    requested.depth_bias[0] = 0x7FC00000u;
    requested.blend[0] = 0x7FC00001u;
    Emit(cache, requested, recorded);
    REQUIRE(recorded == requested);
    REQUIRE_FALSE(cache.UpdateViewport(requested.viewport));
    REQUIRE_FALSE(cache.UpdateDepthBias(requested.depth_bias));
    REQUIRE_FALSE(cache.UpdateBlendConstants(requested.blend));
  }
  SECTION("resolve or presenter commands disturb every cached value") {
    cache.Reset();
    recorded = {};
    recorded.pipeline = 99;
    recorded.layout = 99;
    recorded.descriptors.fill(99);
    recorded.viewport.fill(99);
    recorded.scissor.fill(99);
    recorded.depth_bias.fill(99);
    recorded.stencil.fill(99);
    recorded.blend.fill(99);
    recorded.constants.fill(99);
    Emit(cache, requested, recorded);
    REQUIRE(recorded == requested);
  }
  SECTION("binding a different pipeline re-establishes all dynamic state") {
    REQUIRE(cache.UpdatePipeline(2));
    REQUIRE(cache.UpdateViewport(requested.viewport));
    REQUIRE(cache.UpdateScissor(requested.scissor));
    REQUIRE(cache.UpdateDepthBias(requested.depth_bias));
    REQUIRE(cache.UpdateStencil(requested.stencil));
    REQUIRE(cache.UpdateBlendConstants(requested.blend));
    REQUIRE(cache.UpdatePushConstants(requested.layout, requested.constants));
    REQUIRE_FALSE(cache.UpdateDescriptors(requested.layout, requested.descriptors));
  }
}
