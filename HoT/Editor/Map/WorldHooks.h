#ifndef HOT_EDITOR_MAP_WORLDHOOKS_H
#define HOT_EDITOR_MAP_WORLDHOOKS_H

// Called by the decomp world update so edited ADTs survive the area of interest moving away.
namespace HoT::Editor::Map {

// All is for CMap::Preload, which purges every loaded area.
void BeforePrepareUpdate(bool all = false);
void AfterPrepareUpdate();

// Called from World::UnloadMap before CMap is torn down.
void OnWorldUnload();

}

#endif
