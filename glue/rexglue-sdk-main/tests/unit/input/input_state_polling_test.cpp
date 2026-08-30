#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <rex/input/input_system.h>

namespace rex::input {
namespace {

class StatePollingDriver final : public InputDriver {
 public:
  StatePollingDriver() : InputDriver(nullptr, 0) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }

  X_RESULT GetCapabilities(uint32_t, uint32_t, X_INPUT_CAPABILITIES* out_caps) override {
    capability_queries.push_back(out_caps != nullptr);
    return X_ERROR_SUCCESS;
  }

  X_RESULT GetState(uint32_t, X_INPUT_STATE* out_state) override {
    state_polls.push_back(out_state != nullptr);
    if (out_state) {
      out_state->gamepad.thumb_rx = pending_motion;
      pending_motion = 0;
    }
    return X_ERROR_SUCCESS;
  }

  X_RESULT SetState(uint32_t, X_INPUT_VIBRATION*) override { return X_ERROR_DEVICE_NOT_CONNECTED; }

  X_RESULT GetKeystroke(uint32_t, uint32_t, X_INPUT_KEYSTROKE*) override {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  std::vector<bool> capability_queries;
  std::vector<bool> state_polls;
  int16_t pending_motion = 123;
};

class MutableStateDriver final : public InputDriver {
 public:
  MutableStateDriver() : InputDriver(nullptr, 0) {}

  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  X_RESULT GetCapabilities(uint32_t, uint32_t, X_INPUT_CAPABILITIES*) override {
    return connection_result;
  }
  X_RESULT GetState(uint32_t, X_INPUT_STATE* out_state) override {
    if (connection_result != X_ERROR_SUCCESS) {
      return connection_result;
    }
    *out_state = state;
    return X_ERROR_SUCCESS;
  }
  X_RESULT SetState(uint32_t, X_INPUT_VIBRATION*) override { return X_ERROR_DEVICE_NOT_CONNECTED; }
  X_RESULT GetKeystroke(uint32_t, uint32_t, X_INPUT_KEYSTROKE*) override {
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  X_INPUT_STATE state{};
  X_RESULT connection_result = X_ERROR_SUCCESS;
};

}  // namespace

TEST_CASE("Null state queries do not poll or drain driver state", "[input][polling]") {
  InputSystem input(nullptr);
  auto driver = std::make_unique<StatePollingDriver>();
  StatePollingDriver* driver_ptr = driver.get();
  input.AddDriver(std::move(driver));

  CHECK(input.GetState(0, nullptr) == X_ERROR_SUCCESS);
  CHECK(input.GetState(0, nullptr) == X_ERROR_SUCCESS);
  CHECK(driver_ptr->state_polls.empty());
  REQUIRE(driver_ptr->capability_queries.size() == 2);
  CHECK(driver_ptr->capability_queries.front());
  CHECK(driver_ptr->capability_queries.back());

  X_INPUT_STATE state = {};
  CHECK(input.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(driver_ptr->state_polls.size() == 1);
  CHECK(driver_ptr->state_polls.front());
  CHECK(state.gamepad.thumb_rx == 123);

  // Non-null calls are explicit sampling boundaries. With no frame identity in
  // this interface, replaying a relative delta would emulate a held stick.
  CHECK(input.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(driver_ptr->state_polls.size() == 2);
  CHECK(state.gamepad.thumb_rx == 0);
}

TEST_CASE("Merged packet numbers follow the final combined state", "[input][polling]") {
  InputSystem input(nullptr);
  auto changing_driver = std::make_unique<MutableStateDriver>();
  auto high_packet_driver = std::make_unique<MutableStateDriver>();
  MutableStateDriver* changing_driver_ptr = changing_driver.get();
  MutableStateDriver* high_packet_driver_ptr = high_packet_driver.get();
  changing_driver_ptr->state.packet_number = 100;
  changing_driver_ptr->state.gamepad.buttons = X_INPUT_GAMEPAD_A;
  high_packet_driver_ptr->state.packet_number = 500;
  input.AddDriver(std::move(changing_driver));
  input.AddDriver(std::move(high_packet_driver));

  X_INPUT_STATE state = {};
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  CHECK(state.packet_number == 500);
  CHECK(state.gamepad.buttons == X_INPUT_GAMEPAD_A);

  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  CHECK(state.packet_number == 500);

  // The lower-packet producer changes the combined output while the maximum
  // producer packet remains unchanged. The aggregate still needs a new packet.
  changing_driver_ptr->state.gamepad.buttons = X_INPUT_GAMEPAD_B;
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  CHECK(state.packet_number == 501);
  CHECK(state.gamepad.buttons == X_INPUT_GAMEPAD_B);
}

TEST_CASE("Controller and keyboard drivers remain simultaneously usable",
          "[input][polling][simultaneous]") {
  InputSystem input(nullptr);
  auto controller_driver = std::make_unique<MutableStateDriver>();
  auto keyboard_driver = std::make_unique<MutableStateDriver>();
  MutableStateDriver* controller = controller_driver.get();
  MutableStateDriver* keyboard = keyboard_driver.get();

  controller->state.gamepad.buttons = X_INPUT_GAMEPAD_A;
  controller->state.gamepad.left_trigger = 64;
  controller->state.gamepad.thumb_lx = -16000;
  keyboard->state.gamepad.buttons = X_INPUT_GAMEPAD_B;
  keyboard->state.gamepad.left_trigger = 128;
  keyboard->state.gamepad.thumb_lx = 12000;
  keyboard->state.gamepad.thumb_ry = 14000;
  input.AddDriver(std::move(controller_driver));
  input.AddDriver(std::move(keyboard_driver));

  X_INPUT_STATE state = {};
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  CHECK((state.gamepad.buttons & X_INPUT_GAMEPAD_A) != 0);
  CHECK((state.gamepad.buttons & X_INPUT_GAMEPAD_B) != 0);
  CHECK(state.gamepad.left_trigger == 128);
  CHECK(state.gamepad.thumb_lx == -16000);
  CHECK(state.gamepad.thumb_ry == 14000);

  controller->connection_result = X_ERROR_DEVICE_NOT_CONNECTED;
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  CHECK(state.gamepad.buttons == X_INPUT_GAMEPAD_B);

  controller->state.gamepad.buttons = X_INPUT_GAMEPAD_X;
  controller->connection_result = X_ERROR_SUCCESS;
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  CHECK((state.gamepad.buttons & X_INPUT_GAMEPAD_X) != 0);
  CHECK((state.gamepad.buttons & X_INPUT_GAMEPAD_B) != 0);
}

TEST_CASE("The last merged input state can be observed without polling drivers",
          "[input][polling][snapshot]") {
  InputSystem input(nullptr);
  auto driver = std::make_unique<StatePollingDriver>();
  StatePollingDriver* driver_ptr = driver.get();
  input.AddDriver(std::move(driver));

  X_INPUT_STATE cached = {};
  CHECK_FALSE(input.TryGetLastState(0, &cached));
  CHECK_FALSE(input.TryGetLastState(0, nullptr));

  X_INPUT_STATE state = {};
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(driver_ptr->state_polls.size() == 1);
  REQUIRE(input.TryGetLastState(0, &cached));
  CHECK(driver_ptr->state_polls.size() == 1);
  CHECK(cached.packet_number == state.packet_number);
  CHECK(cached.gamepad.thumb_rx == state.gamepad.thumb_rx);
}

TEST_CASE("The cached input state is invalidated after device loss",
          "[input][polling][snapshot]") {
  InputSystem input(nullptr);
  auto driver = std::make_unique<MutableStateDriver>();
  MutableStateDriver* driver_ptr = driver.get();
  driver_ptr->state.gamepad.buttons = X_INPUT_GAMEPAD_LEFT_SHOULDER;
  input.AddDriver(std::move(driver));

  X_INPUT_STATE state = {};
  REQUIRE(input.GetState(0, &state) == X_ERROR_SUCCESS);
  REQUIRE(input.TryGetLastState(0, &state));
  CHECK((state.gamepad.buttons & X_INPUT_GAMEPAD_LEFT_SHOULDER) != 0);

  driver_ptr->connection_result = X_ERROR_DEVICE_NOT_CONNECTED;
  CHECK(input.GetState(0, &state) == X_ERROR_DEVICE_NOT_CONNECTED);
  CHECK_FALSE(input.TryGetLastState(0, &state));
}

}  // namespace rex::input
