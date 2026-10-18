#ifndef HOT_EDITOR_MAP_MAP_H
#define HOT_EDITOR_MAP_MAP_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

// The Map workspace of the game editor, terrain, textures, chunk flags and doodad or WMO placement.
namespace HoT::Editor::Map {

// Registers the "map." tool handler and the Editor* Lua functions.
void Apply();

// Doodad and WMO ray picking for the shared selection, kind 0 is a doodad and 1 a WMO.
bool PickObject(const C3Vector& start, const C3Vector& end, int32_t& kind, uint32_t& uniqueId, float& distance);

void SelectObject(int32_t kind, uint32_t uniqueId, bool add);
void ClearObjectSelection();

}

#endif
