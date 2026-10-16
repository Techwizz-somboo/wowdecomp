#ifndef HOT_SPELLS_DESCRIPTIONVARS_H
#define HOT_SPELLS_DESCRIPTIONVARS_H

#include <cstdint>

class CGPlayer_C;
class SpellRec;

namespace HoT::Spells {

// Resolves the HoT spell description variables and the unrounded ranged ones, false leaves the op to the stock switch.
bool GetDescriptionVariable(uint32_t op, CGPlayer_C* player, const SpellRec* spell, float& value);

}

#endif
