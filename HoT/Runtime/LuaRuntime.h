#ifndef HOT_RUNTIME_LUARUNTIME_H
#define HOT_RUNTIME_LUARUNTIME_H

#include <cstddef>
#include <cstdint>

struct lua_State;

namespace HoT {
class Module;
}

// Lua allocator, GC pacing, the Lua profiler and the per frame UI timings behind luaPerfStats.
namespace HoT::LuaRuntime {

enum class Phase {
    Update,
    Layout,
    Build,
    Draw,
    TexUpload,
    Raise,
    Compress,
    Occlude,
    AddResize,
};

enum class Counter {
    Batches,
    Textures,
    FontStrings,
    LayoutFrames,
    LayoutFailed,
};

extern bool g_statsEnabled;

void* CreatePool();
void* Realloc(void* ud, void* ptr, size_t osize, size_t nsize);
void DestroyPool(void* ud);

// After FrameScript_Initialize built the state.
void OnStateCreated(lua_State* L);

// Start and end of PaintScreen, the end paces the GC.
void BeginPaint();
void EndPaint();

int64_t BeginPhase(Phase phase);
void EndPhase(Phase phase, int64_t start);
void Increment(Counter counter);
int64_t EnterHandler(bool& outer);
void LeaveHandler(bool outer, int64_t start);

inline void Count(Counter counter) {
    if (g_statsEnabled) {
        Increment(counter);
    }
}

// Adds the scope's time to a phase of the current frame while luaPerfStats is on.
class Timer {
    public:
    explicit Timer(Phase phase) : m_phase(phase), m_start(g_statsEnabled ? BeginPhase(phase) : 0) {}

    ~Timer() {
        if (this->m_start) {
            EndPhase(this->m_phase, this->m_start);
        }
    }

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    private:
    Phase m_phase;
    int64_t m_start;
};

// Times a script handler, nested handlers only add to the call count.
class HandlerTimer {
    public:
    HandlerTimer() {
        if (g_statsEnabled) {
            this->m_start = EnterHandler(this->m_outer);
        }
    }

    ~HandlerTimer() {
        if (this->m_start) {
            LeaveHandler(this->m_outer, this->m_start);
        }
    }

    HandlerTimer(const HandlerTimer&) = delete;
    HandlerTimer& operator=(const HandlerTimer&) = delete;

    private:
    bool m_outer = false;
    int64_t m_start = 0;
};

Module* GetModule();

}

#endif
