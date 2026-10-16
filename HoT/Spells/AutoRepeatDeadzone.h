#ifndef HOT_SPELLS_AUTOREPEATDEADZONE_H
#define HOT_SPELLS_AUTOREPEATDEADZONE_H

class CGUnit_C;

namespace HoT {
class Module;
}

// CVar autoRepeatMeleeDeadzone, the melee range that stops auto shot as a percent of stock, 0 never stops it.
namespace HoT::AutoRepeatDeadzone {

// Replaces the IsInMeleeRange check in CGPlayer_C::AutoCombatModeEventHandler.
bool IsInMeleeRange(const CGUnit_C* unit, CGUnit_C* target);

Module* GetModule();

}

#endif
