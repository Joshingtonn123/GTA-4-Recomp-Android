#include <rex/input/input_trace.h>

#include <atomic>

#include <rex/cvar.h>
#include <rex/ui/virtual_key.h>

REXCVAR_DEFINE_BOOL(input_trace, false, "Input/Diagnostics",
                    "Trace changed keyboard and controller state end to end");

namespace rex::input {

namespace {

std::atomic<uint64_t> g_input_trace_sequence{0};

}  // namespace

bool IsInputTraceEnabled() {
  return REXCVAR_GET(input_trace);
}

uint64_t NextInputTraceSequence() {
  return g_input_trace_sequence.fetch_add(1, std::memory_order_relaxed) + 1;
}

const char* InputTraceVirtualKeyName(uint32_t virtual_key) {
  using rex::ui::VirtualKey;
  switch (static_cast<VirtualKey>(virtual_key)) {
    case VirtualKey::kEscape:
      return "escape";
    case VirtualKey::kUp:
      return "up";
    case VirtualKey::kDown:
      return "down";
    case VirtualKey::kLeft:
      return "left";
    case VirtualKey::kRight:
      return "right";
    case VirtualKey::kReturn:
      return "return";
    case VirtualKey::kSpace:
      return "space";
    case VirtualKey::kBack:
      return "backspace";
    case VirtualKey::kDelete:
      return "delete";
    case VirtualKey::kW:
      return "w";
    case VirtualKey::kA:
      return "a";
    case VirtualKey::kS:
      return "s";
    case VirtualKey::kD:
      return "d";
    default:
      return "other";
  }
}

}  // namespace rex::input
