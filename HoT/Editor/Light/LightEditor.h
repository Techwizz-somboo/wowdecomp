#ifndef HOT_EDITOR_LIGHT_LIGHTEDITOR_H
#define HOT_EDITOR_LIGHT_LIGHTEDITOR_H

#include <cstdint>

class CGWorldFrame;

// Client half of the light editor: Light.dbc coordinate codec, area light rebuilds, preview state and the world markers.
namespace HoT::Editor::LightEditor {

void Apply();
void OnGameDestroy();
void OnWorldRender(CGWorldFrame* frame);

// Converts between the loaded LightRec in world yards and the file row in DBC units, zero coordinates mark the map default light and stay zero.
void RecordToDbc(uint8_t* record);
void RecordToWorld(uint8_t* record);

// A Light row was written, the area lights of the current map point into g_lightDB and are built again.
void OnLightWritten();

}

#endif
