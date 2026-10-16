#ifndef HOT_SPELLS_TARGETING_H
#define HOT_SPELLS_TARGETING_H

#include "Engine/Source/lua-5.1/src/lua.h"

class C3Vector;
class CGWorldFrame;
struct HITTESTRESULT;

namespace HoT::Spells {

// Takes a "cursor" or "self" second argument off CastSpellByName, true when the cast already ran.
bool CastSpellByNameAt(lua_State* L, lua_CFunction cast);

// Places a pending "cursor" cast at the tracked terrain point, true when the track was used for it.
bool ConsumeCastAtCursor(const HITTESTRESULT& result);

// Pulls a ground targeting segment that points past the spell's range back to the farthest valid spot.
void ClampGroundTarget(CGWorldFrame* frame, C3Vector* start, C3Vector* end);

}

#endif
