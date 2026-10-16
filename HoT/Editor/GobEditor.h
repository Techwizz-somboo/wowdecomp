#ifndef HOT_EDITOR_GOBEDITOR_H
#define HOT_EDITOR_GOBEDITOR_H

#include <cstdint>
#include <string>

class CGGameObject_C;
class CGWorldFrame;

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
void OnGameInitialize();
void OnGameDestroy();
void OnWorldRender(CGWorldFrame* frame);

bool SelectGameObject(uint64_t guid);
void ClearSelection();
CGGameObject_C* SelectedGameObject();
uint64_t CurrentSelectedGobGUID();

}

#endif
