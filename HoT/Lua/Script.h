#ifndef HOT_LUA_SCRIPT_H
#define HOT_LUA_SCRIPT_H

#include <cstdarg>
#include <cstdint>
#include "Engine/Source/lua-5.1/src/lua.h"

namespace HoT::Lua {

enum class State {
    Any,
    Game,
    Glue,
};

void RegisterFunction(const char* name, lua_CFunction function, State state = State::Game);

// Names must outlive the client, string literals are the intended use.
void RegisterEvent(const char* name, State state = State::Any);

bool SignalEvent(const char* name, const char* format, ...);
bool vSignalEvent(const char* name, const char* format, va_list args);
bool HasEvent(const char* name);

// Index into the current event table, -1 when the name is not in it.
int32_t EventIndex(const char* name);

State CurrentState();

void LoadGame(const char** stockEvents, uint32_t stockCount);
void LoadGlue(const char** stockEvents, uint32_t stockCount);

}

#endif
