#ifndef HOT_EDITOR_CREATUREEDITOR_H
#define HOT_EDITOR_CREATUREEDITOR_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

class CGWorldFrame;

// Client half of the in-game creature editor: carries the server snapshot to Lua, sends edits back and draws the waypoint path.
namespace HoT::Editor::CreatureEditor {

void Apply();
void OnGameDestroy();
void OnWorldRender(CGWorldFrame* frame);

void Request(uint64_t guid);
void Forget();
void Move(uint64_t guid, const C3Vector& position, float orientation);

// The loaded path of the snapshot creature, used by the waypoint tool.
uint64_t SnapshotGuid();
uint32_t PathId();
size_t NodeCount();
bool GetNode(size_t index, uint32_t& point, C3Vector& position);
void PreviewNode(uint32_t point, const C3Vector& position);
void MoveNode(uint32_t point, const C3Vector& position);
void AddNode(uint32_t afterPoint, const C3Vector& position);
void DeleteNode(uint32_t point);
uint32_t SelectedPoint();
void SetSelectedPoint(uint32_t point);

// The waypoint tool draws the path even when the UI has not asked for it.
void SetPathForced(bool forced);

// The lift the path drawing puts on every node, picking uses the same.
constexpr float kNodeLift = 0.4f;

}

#endif
