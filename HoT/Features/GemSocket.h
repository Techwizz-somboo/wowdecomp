#ifndef HOT_FEATURES_GEMSOCKET_H
#define HOT_FEATURES_GEMSOCKET_H

struct lua_State;

namespace HoT {
class Module;
}

namespace HoT::GemSocket {

// Returns true when the click opened the gem socket panel, called from Script_UseContainerItem after crafting.
bool HandleUseContainerItem(lua_State* L);

Module* GetModule();

}

#endif
