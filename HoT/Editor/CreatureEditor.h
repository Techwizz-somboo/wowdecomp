#ifndef HOT_EDITOR_CREATUREEDITOR_H
#define HOT_EDITOR_CREATUREEDITOR_H

class CGWorldFrame;

// Client half of the in-game creature editor: carries the server snapshot to Lua, sends edits back and draws the waypoint path.
namespace HoT::Editor::CreatureEditor {

void Apply();
void OnGameDestroy();
void OnWorldRender(CGWorldFrame* frame);

}

#endif
