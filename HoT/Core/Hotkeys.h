#ifndef HOT_CORE_HOTKEYS_H
#define HOT_CORE_HOTKEYS_H

namespace HoT::Hotkeys {

using PressFn = void (*)();

// Archived CVar naming a function key ("F11", "NONE" turns it off), seen before the UI and bindings in glue and game.
void Register(const char* cvar, const char* help, const char* defaultKey, PressFn onPress);

// Starts listening once an event context exists.
void Install();

}

#endif
