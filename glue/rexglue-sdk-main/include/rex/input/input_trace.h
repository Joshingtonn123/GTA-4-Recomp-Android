#pragma once

#include <cstdint>

namespace rex::input {

bool IsInputTraceEnabled();
uint64_t NextInputTraceSequence();
const char* InputTraceVirtualKeyName(uint32_t virtual_key);

}  // namespace rex::input
