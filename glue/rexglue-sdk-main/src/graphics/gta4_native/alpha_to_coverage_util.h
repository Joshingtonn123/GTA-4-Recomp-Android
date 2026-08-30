#ifndef REX_GRAPHICS_GTA4_NATIVE_ALPHA_TO_COVERAGE_UTIL_H_
#define REX_GRAPHICS_GTA4_NATIVE_ALPHA_TO_COVERAGE_UTIL_H_

#include <cstdint>

namespace rex::graphics::gta4_native {

constexpr uint32_t kNativeAlphaToMaskEnable = 1u << 8;

// RB_COLORCONTROL stores the four 2-bit alpha-to-mask offsets in its upper
// byte. The native shaders consume a compact native-only layout: bits 0-7 are
// the Xenos offsets and bit 8 is enable.
constexpr uint32_t PackNativeAlphaToMask(uint32_t color_control) {
  return color_control & 0x10u
             ? ((color_control >> 24) & 0xFFu) | kNativeAlphaToMaskEnable
             : 0u;
}

constexpr bool IsNativeAlphaToMaskRequested(uint32_t packed_alpha_to_mask) {
  return (packed_alpha_to_mask & kNativeAlphaToMaskEnable) != 0;
}

constexpr bool IsNativeFragmentCoverageRequested(bool alpha_test_requested,
                                                  uint32_t packed_alpha_to_mask) {
  return alpha_test_requested || IsNativeAlphaToMaskRequested(packed_alpha_to_mask);
}

}  // namespace rex::graphics::gta4_native

#endif  // REX_GRAPHICS_GTA4_NATIVE_ALPHA_TO_COVERAGE_UTIL_H_
