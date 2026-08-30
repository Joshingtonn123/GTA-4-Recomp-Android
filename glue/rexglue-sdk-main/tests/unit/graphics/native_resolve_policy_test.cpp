#include <catch2/catch_test_macros.hpp>

#include "graphics/gta4_native/native_resolve_policy.h"

namespace gta4 = rex::graphics::gta4_native;

TEST_CASE("GTA IV native resolve discards only complete destination subresources") {
  REQUIRE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 0, 1920, 1080, 1920, 1080}));
  REQUIRE_FALSE(gta4::IsFullNativeResolveSubresourceOverwrite({1, 0, 1920, 1080, 1920, 1080}));
  REQUIRE_FALSE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 1, 1920, 1080, 1920, 1080}));
  REQUIRE_FALSE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 0, 1919, 1080, 1920, 1080}));
  REQUIRE_FALSE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 0, 1920, 1079, 1920, 1080}));
}

TEST_CASE("GTA IV native resolve treats one-pixel and zero-sized targets exactly") {
  REQUIRE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 0, 1, 1, 1, 1}));
  REQUIRE_FALSE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 0, 0, 0, 0, 0}));
  REQUIRE_FALSE(gta4::IsFullNativeResolveSubresourceOverwrite({0, 0, 0, 1, 1, 1}));
}
