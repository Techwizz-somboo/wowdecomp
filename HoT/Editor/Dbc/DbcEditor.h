#ifndef HOT_EDITOR_DBC_DBCEDITOR_H
#define HOT_EDITOR_DBC_DBCEDITOR_H

#include <cstdint>

// Typed rows of the loaded client tables for Lua, local preview edits and saves through the server.
namespace HoT::Editor::Dbc {

void Apply();
void OnIdle();
void OnGameDestroy();

}

#endif
