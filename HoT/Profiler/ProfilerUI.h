#ifndef HOT_PROFILER_PROFILERUI_H
#define HOT_PROFILER_PROFILERUI_H

// The profiler window, compiled into the exe and served as Interface\GraphicsProfiler\... like the game editor UI.
namespace HoT::Profiler::UI {

// Loads the window into the current Lua state on first use, false when it failed.
bool Load();

void SetShown(bool shown);

// The Lua state was rebuilt and the window is gone.
void OnScriptsReloaded();

}

#endif
