#ifndef HOT_EDITOR_SELECTION_H
#define HOT_EDITOR_SELECTION_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

class CGObject_C;

// The world object selection shared by the creature and gameobject editors, with native drag and the Ctrl+click gizmo.
namespace HoT::Editor::Selection {

enum class Kind : uint8_t {
    None = 0,
    Creature = 1,
    GameObject = 2,
};

Kind SelectedKind();
uint64_t SelectedGuid();
CGObject_C* SelectedObject();

// Selects the unit or gameobject behind the guid and asks the server for its editor snapshot.
bool Select(uint64_t guid);

// Selects the creature or gameobject under the mouse, any visible one counts even when the game would not let it be clicked.
bool ClickSelect(float x, float y);

// Selects the closest spawned creature or gameobject to the view, Kind::None takes either.
uint64_t SelectNearest(Kind kind, float maxDistance);
void Clear();

// The server replaced the object (respawn after a save), keep the selection on the new guid.
void Replace(uint64_t oldGuid, uint64_t newGuid);

// The editors call these when a snapshot arrives, a pending spawn becomes the selection.
void OnCreatureSnapshot(uint64_t guid);
void OnGameObjectSnapshot(uint64_t guid);

void Apply();
void OnGameDestroy();

}

#endif
