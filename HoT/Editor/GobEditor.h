#ifndef HOT_EDITOR_GOBEDITOR_H
#define HOT_EDITOR_GOBEDITOR_H

#include <cstdint>
#include <string>
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Editor/Quat.h"

class CGGameObject_C;

// Client half of the in-game gameobject editor: the server snapshot for Lua, field edits and position commits.
namespace HoT::Editor::GobEditor {

struct DecodedGuid {
    uint64_t m_full = 0;
    uint16_t m_type = 0;
    uint32_t m_entry = 0;
    uint32_t m_low = 0;
};

DecodedGuid DecodeGuid(uint64_t full);
std::string GuidToString(uint64_t guid);

CGGameObject_C* GameObjectByGuid(uint64_t guid);

// The packed rotation shares storage with the passenger facing and pitch, like CPassenger reads it.
uint64_t& PackedRotation(CGGameObject_C* object);

void Apply();
void OnGameDestroy();

void Request(uint64_t guid);
void Forget();

// Sends the live client position and rotation of the object to the server.
void Commit(uint64_t guid);
void CommitTransform(uint64_t guid, const C3Vector& position, const Quat& rotation);

// The older Lua helpers select through the shared editor selection.
bool SelectGameObject(uint64_t guid);
void ClearSelection();
CGGameObject_C* SelectedGameObject();
uint64_t CurrentSelectedGobGUID();

}

#endif
