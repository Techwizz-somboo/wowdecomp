#ifndef CONTROLLER_CONTROLLER_H
#define CONTROLLER_CONTROLLER_H

#include <cstdint>
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"

class CSimpleFrame;
class CStatus;
struct lua_State;

// Native gamepad support through SDL3. Everything the client calls lives here so forks only need these hooks.
namespace Controller {

void Initialize();
void Shutdown();

// Called from OsInputGet when the OS queue is empty.
void Poll(int32_t appActive);

// Returns the binding name for a pad key, or nullptr for any other key.
const char* KeyName(KEY key);

// True for pad binding names such as PAD1, used to accept them as bindable keys.
bool IsKeyName(const char* name);

// Appends the GAME_PAD_* events to the stock list and registers the gamepad Lua API.
const char** ScriptEvents(bool game, const char** stockEvents, uint32_t stockCount, uint32_t* count);

// Adds the gamepad binding commands, default pad bindings and the UI glue, call after Bindings.xml.
void LoadGameUI(CStatus* status);

FrameScript_Object::ScriptIx* GetFrameScript(CSimpleFrame* frame, const char* name, FrameScript_Object::ScriptData& data);
void RegisterFrameMethods(lua_State* L);
void OnFrameDestroyed(CSimpleFrame* frame);

}

#endif
