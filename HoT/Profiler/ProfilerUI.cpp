#include "HoT/Profiler/ProfilerUI.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Os/OsFile.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "HoT/Core/Chat.h"
#include "HoT/Core/Log.h"
#include "WoW/Source/Glue/CWOWClientStatus.h"

namespace HoT::Profiler::UI {

static const char* const kToc = "Interface\\GraphicsProfiler\\GraphicsProfiler.toc";
static const char* const kFrame = "GraphicsProfilerFrame";

static bool s_loaded = false;

bool Load() {
    if (s_loaded && CScriptObject::GetScriptObjectByName(kFrame, CSimpleFrame::GetObjectType())) {
        return true;
    }

    if (!FrameScript_GetContext()) {
        return false;
    }

    if (!SFile::FileExistsEx(kToc, 1)) {
        Chat::Print("Graphics profiler: %s is missing", kToc);
        return false;
    }

    OsCreateDirectory("Logs", 0);
    CWOWClientStatus status("Logs\\GraphicsProfiler.log");

    // Not part of the signed interface, nothing checks this digest.
    MD5_CTX md5;
    MD5Init(&md5);
    FrameXML_CreateFrames(kToc, nullptr, &md5, &status);

    if (!CScriptObject::GetScriptObjectByName(kFrame, CSimpleFrame::GetObjectType())) {
        LOG_INFO << "graphics profiler: the UI failed to load, see Logs\\GraphicsProfiler.log";
        Chat::Print("Graphics profiler: the UI failed to load, see Logs\\GraphicsProfiler.log");
        return false;
    }

    s_loaded = true;

    return true;
}

void SetShown(bool shown) {
    lua_State* L = FrameScript_GetContext();

    if (!L || !s_loaded) {
        return;
    }

    int32_t top = lua_gettop(L);
    lua_getfield(L, LUA_GLOBALSINDEX, "GraphicsProfiler_SetShown");

    if (lua_type(L, -1) != LUA_TFUNCTION) {
        lua_settop(L, top);
        return;
    }

    lua_pushboolean(L, shown ? 1 : 0);

    if (lua_pcall(L, 1, 0, 0) != 0) {
        LOG_INFO << "graphics profiler: GraphicsProfiler_SetShown failed: " << (lua_tostring(L, -1) ? lua_tostring(L, -1) : "?");
    }

    lua_settop(L, top);
}

void OnScriptsReloaded() {
    s_loaded = false;
}

}
