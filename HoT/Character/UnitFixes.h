#ifndef HOT_CHARACTER_UNITFIXES_H
#define HOT_CHARACTER_UNITFIXES_H

#include <cstdint>

class CGUnit_C;

namespace HoT::UnitFixes {

// Called at the end of CGUnit_C::DoPowerRegen, predicts focus for an active player whose main power is something else.
void PredictFocusRegen(CGUnit_C* unit, uint32_t elapsed);

// Called at the end of CGUnit_C::DestroyUnitMount so the radius no longer counts the mount display.
void UpdateDismountedSelectionRadius(CGUnit_C* unit);

}

#endif
