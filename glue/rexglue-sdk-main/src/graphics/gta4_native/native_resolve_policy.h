#pragma once

#include <cstdint>

namespace rex::graphics::gta4_native {

struct NativeResolveWriteRegion {
  int32_t destination_x = 0;
  int32_t destination_y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t target_width = 0;
  uint32_t target_height = 0;
};

// A resolve may discard the destination's previous contents only when the
// shader overwrites the complete destination subresource. Partial resolves
// must retain LOAD semantics because GTA can assemble a texture incrementally.
constexpr bool IsFullNativeResolveSubresourceOverwrite(const NativeResolveWriteRegion& region) {
  return region.target_width != 0 && region.target_height != 0 && region.destination_x == 0 &&
         region.destination_y == 0 && region.width == region.target_width &&
         region.height == region.target_height;
}

}  // namespace rex::graphics::gta4_native
