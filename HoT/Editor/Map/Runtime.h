#ifndef HOT_EDITOR_MAP_RUNTIME_H
#define HOT_EDITOR_MAP_RUNTIME_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

// State the map handler shares with the Lua functions.
namespace HoT::Editor::Map::Runtime {

bool HasAccess();
bool IsMapLoaded();
const char* MapDirectory();
uint32_t MapId();

bool CursorValid();
const C3Vector& Cursor();
C3Vector CameraPosition();
float Fps();

// Ends every open stroke or object edit, used before undo, save and tool changes.
void EndEdits();

// Runs a fixed Lua snippet such as a picker callback.
void RunLua(const char* code);

}

#endif
