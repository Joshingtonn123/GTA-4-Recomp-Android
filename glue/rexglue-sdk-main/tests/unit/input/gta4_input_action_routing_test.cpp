#include <catch2/catch_test_macros.hpp>

#include "gta4_input_action_routing.h"

namespace gta4::input {

TEST_CASE("Phone actions use GTA's active gameplay control", "[input][gta4][replay]") {
  const KeyboardActionRoute take_out = ClassifyKeyboardActionRoute(21);
  const KeyboardActionRoute put_away = ClassifyKeyboardActionRoute(22);

  CHECK(take_out == KeyboardActionRoute::kPhoneActiveGameplayControl);
  CHECK(put_away == KeyboardActionRoute::kPhoneActiveGameplayControl);
  CHECK(IsContextAction(take_out));
  CHECK(IsContextAction(put_away));
  CHECK(UsesActiveGameplayControl(take_out));
  CHECK(UsesActiveGameplayControl(put_away));
  CHECK_FALSE(NeedsFrontendConsumerFallback(take_out));
  CHECK_FALSE(NeedsFrontendConsumerFallback(put_away));
}

TEST_CASE("Frontend actions survive replay and retain a consumer fallback",
          "[input][gta4][replay]") {
  const KeyboardActionRoute pause = ClassifyKeyboardActionRoute(76);
  const KeyboardActionRoute accept = ClassifyKeyboardActionRoute(77);
  const KeyboardActionRoute cancel = ClassifyKeyboardActionRoute(78);

  CHECK(pause == KeyboardActionRoute::kFrontendReplayWithConsumerFallback);
  CHECK(accept == KeyboardActionRoute::kFrontendReplayWithConsumerFallback);
  CHECK(cancel == KeyboardActionRoute::kFrontendReplayWithConsumerFallback);
  CHECK(IsContextAction(pause));
  CHECK_FALSE(UsesActiveGameplayControl(pause));
  CHECK(NeedsFrontendConsumerFallback(pause));
}

TEST_CASE("Gameplay actions use only the normal replay path", "[input][gta4][replay]") {
  const KeyboardActionRoute accelerate = ClassifyKeyboardActionRoute(40);

  CHECK(accelerate == KeyboardActionRoute::kGameplayReplay);
  CHECK_FALSE(IsContextAction(accelerate));
  CHECK_FALSE(UsesActiveGameplayControl(accelerate));
  CHECK_FALSE(NeedsFrontendConsumerFallback(accelerate));
}

}  // namespace gta4::input
