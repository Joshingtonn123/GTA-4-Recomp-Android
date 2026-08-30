#pragma once

#include <cstdint>

namespace gta4::input {

enum class KeyboardActionRoute : uint8_t {
  kGameplayReplay,
  kPhoneActiveGameplayControl,
  kFrontendReplayWithConsumerFallback,
};

constexpr KeyboardActionRoute ClassifyKeyboardActionRoute(uint32_t action_index) {
  if (action_index == 21 || action_index == 22) {
    return KeyboardActionRoute::kPhoneActiveGameplayControl;
  }
  if (action_index >= 64 && action_index <= 84) {
    return KeyboardActionRoute::kFrontendReplayWithConsumerFallback;
  }
  return KeyboardActionRoute::kGameplayReplay;
}

constexpr bool IsContextAction(KeyboardActionRoute route) {
  return route != KeyboardActionRoute::kGameplayReplay;
}

constexpr bool UsesActiveGameplayControl(KeyboardActionRoute route) {
  return route == KeyboardActionRoute::kPhoneActiveGameplayControl;
}

constexpr bool NeedsFrontendConsumerFallback(KeyboardActionRoute route) {
  return route == KeyboardActionRoute::kFrontendReplayWithConsumerFallback;
}

}  // namespace gta4::input
