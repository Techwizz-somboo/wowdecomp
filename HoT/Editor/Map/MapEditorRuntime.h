#ifndef HOT_EDITOR_MAP_MAPEDITORRUNTIME_H
#define HOT_EDITOR_MAP_MAPEDITORRUNTIME_H

class CGWorldFrame;

namespace HoT::MapEditor::Runtime {

void Apply();
void OnGameClientInitialize();
void OnGameClientDestroy();

void BeforeWorldRender(CGWorldFrame* worldFrame);
void AfterWorldRender(CGWorldFrame* worldFrame);

}

#endif
