#ifndef HOT_CORE_MODULES_H
#define HOT_CORE_MODULES_H

#include <vector>
#include "HoT/Lua/Script.h"

namespace HoT {

// A feature registers its packet handlers, Lua functions, events and CVars from these callbacks.
class Module {
    public:
    virtual ~Module() = default;

    virtual const char* Name() const = 0;

    // CVars are loaded and archives are open, the client database is not.
    virtual void OnInitializeGlobal() {}

    // Every client DB table is loaded.
    virtual void OnClientDBInitialized() {}

    // Client services, the glue manager and the world are initialized.
    virtual void OnInitializeClient() {}

    // A character entered the world.
    virtual void OnPlayerClientInitialize() {}

    // Enter world was pressed and the character info is set, before the world loads.
    virtual void OnEnterWorld() {}

    // A loading screen came up, for login and for every teleport.
    virtual void OnLoadingScreenStarted() {}

    // The world is being torn down, on logout, disconnect and character select.
    virtual void OnDestroyGame() {}

    // The glue or game Lua state was rebuilt and every HoT function and event is registered in it.
    virtual void OnScriptsLoaded(Lua::State state) {}

    // Every frame, in glue and in game.
    virtual void OnIdle() {}
};

// The list lives in HoT/Modules.cpp so that no feature depends on static initialization order.
std::vector<Module*>& Modules();

}

#endif
