#include "HoT/Core/Log.h"
#include "HoT/Character/HD/HDCharacters.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"
#include "HoT/Runtime/LauncherSettings.h"
#include "HoT/Streaming/BackgroundDownloader.h"

namespace HoT::LauncherSettings {

static int32_t Script_IsHDPatchEnabled(lua_State* L) {
    lua_pushboolean(L, HdPatch());
    return 1;
}

static int32_t Script_SetHDPatchEnabled(lua_State* L) {
    SetHdPatch(lua_toboolean(L, 1) != 0);
    Streaming::Trigger();
    HoT::HD::RefreshPatch();
    return 0;
}

static int32_t Script_AreHDCharacterModelsAvailable(lua_State* L) {
    lua_pushboolean(L, HoT::HD::IsAvailable());
    return 1;
}

static int32_t Script_GetDownloadSpeedLimit(lua_State* L) {
    lua_pushnumber(L, MaxDownloadMBps());
    return 1;
}

static int32_t Script_SetDownloadSpeedLimit(lua_State* L) {
    double mbps = lua_isnumber(L, 1) ? lua_tonumber(L, 1) : 0.0;
    SetMaxDownloadMBps(mbps);
    return 0;
}

class LauncherSettingsModule : public Module {
    public:
    const char* Name() const override { return "LauncherSettings"; }

    void OnInitializeGlobal() override {
        Lua::RegisterFunction("IsHDPatchEnabled", Script_IsHDPatchEnabled, Lua::State::Any);
        Lua::RegisterFunction("SetHDPatchEnabled", Script_SetHDPatchEnabled, Lua::State::Any);
        Lua::RegisterFunction("AreHDCharacterModelsAvailable", Script_AreHDCharacterModelsAvailable, Lua::State::Any);
        Lua::RegisterFunction("GetDownloadSpeedLimit", Script_GetDownloadSpeedLimit, Lua::State::Any);
        Lua::RegisterFunction("SetDownloadSpeedLimit", Script_SetDownloadSpeedLimit, Lua::State::Any);
    }
};

Module* GetModule() {
    static LauncherSettingsModule module;
    return &module;
}

}
