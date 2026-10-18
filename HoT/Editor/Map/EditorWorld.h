#ifndef HOT_EDITOR_MAP_EDITORWORLD_H
#define HOT_EDITOR_MAP_EDITORWORLD_H

#include <cstdint>

namespace HoT::Editor::Map::EditorWorld {

// Parks edited areas outside the area of interest so CMap::PrepareUpdate does not free them, all parks every one.
void ParkDirtyAreas(bool all);
void UnparkAreas();

// Snapshot of the world settings the editor touches, taken on activate and put back on deactivate.
void SaveState();
void RestoreState();

bool GetFog();
void SetFog(bool enabled);

uint32_t TimeOfDay();
void SetTimeOfDay(uint32_t minutes);
void ClearTimeOfDay();
void ApplyTimeOfDay();

void Stats(uint32_t& areasLoaded, uint32_t& chunksDrawn);

}

#endif
