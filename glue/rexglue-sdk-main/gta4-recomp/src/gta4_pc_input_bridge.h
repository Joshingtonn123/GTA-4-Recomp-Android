#pragma once

#include <cstdint>

#include "gta4_init.h"

namespace gta4::input {

// Called by the existing sub_82163CE0 override after the retail predicate has
// run. Direct weapon keys use the retail action-8 eligibility path and only
// force the exact call site that feeds the retail weapon selector.
void MaybeForceDirectWeaponAction(PPCContext& ctx, uint8_t* base,
                                  uint32_t action_record, uint32_t caller);

}  // namespace gta4::input
