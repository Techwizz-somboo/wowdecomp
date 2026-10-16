#ifndef HOT_CHARACTER_PLAYERRENDERLIMIT_H
#define HOT_CHARACTER_PLAYERRENDERLIMIT_H

#include <cstdint>

class CGObject_C;

namespace HoT {
class Module;
}

// Caps how many other players are drawn, keeping target, party, raid, friends and guild first and then the nearest.
namespace HoT::PlayerRenderLimit {

// True for a player the limit currently hides.
bool IsHidden(const CGObject_C* object);

// Called first in CGWorldFrame::UpdateObject, true means the object was hidden and the update is skipped.
bool SuppressUpdate(CGObject_C* object);

// Called at the end of CGPlayer_C::ShouldRender.
void OnShouldRender(CGObject_C* object, int32_t& hidden);

Module* GetModule();

}

#endif
