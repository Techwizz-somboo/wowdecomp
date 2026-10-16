#ifndef CONTROLLER_FRAMES_H
#define CONTROLLER_FRAMES_H

#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript_Object.h"

class CSimpleFrame;
struct lua_State;

namespace Controller {

struct MappedState;

namespace Frames {

FrameScript_Object::ScriptIx* GetScript(CSimpleFrame* frame, const char* name, FrameScript_Object::ScriptData& data);
void RegisterMethods(lua_State* L);
void OnDestroyed(CSimpleFrame* frame);

// True when a frame with EnableGamePadButton took the press, its release goes to the same frame.
bool ButtonDown(int32_t button);
bool ButtonUp(int32_t button);

// True when a visible frame with EnableGamePadStick owns the sticks this frame.
bool DispatchSticks(const MappedState& state);

}

}

#endif
