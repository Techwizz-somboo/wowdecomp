#ifndef CONTROLLER_BINDINGS_H
#define CONTROLLER_BINDINGS_H

#include <cstdint>

class KEYBINDING;

// Gamepad bindings are their own set, kept apart from the keyboard binding sets so pad keys never take a keyboard slot.
namespace Controller::Bindings {

void Load();
void Save();
void Reset();

// True for keys such as PAD1 or SHIFT-PAD1, with or without modifiers.
bool IsPadKey(const char* key);

// Looks a pad key up in the gamepad set, false for keyboard keys so the caller falls back to the stock sets.
bool Find(const char* key, KEYBINDING*& binding);

bool Set(const char* key, const char* command);
const char* GetAction(const char* key);
const char* GetKey(const char* command, int32_t index);

}

#endif
