#include "HoT/Discord/Discord.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "HoT/Discord/DiscordRPC.h"
#include "HoT/Lua/Script.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"

namespace HoT::Discord {

static CVar* s_enabledCvar = nullptr;

static void Sync(bool enabled) {
    if (enabled) {
        if (!RPC::IsInitialized() && RPC::Init()) {
            RPC::UpdateActivity("", "Staring at the login screen.", "icon_transparent_");
        }
    } else if (RPC::IsInitialized()) {
        RPC::Shutdown();
    }
}

static void OnEnabledChanged(const char* value) {
    Sync(CVars::Bool(value));
}

static const char* OptionalLuaString(lua_State* L, int32_t index, const char* fallback) {
    if (lua_type(L, index) != LUA_TSTRING) {
        return fallback;
    }

    const char* value = lua_tolstring(L, index, nullptr);

    return value ? value : fallback;
}

static int32_t Script_UpdateDiscordPresence(lua_State* L) {
    const char* status = lua_tolstring(L, 1, nullptr);
    const char* details = lua_tolstring(L, 2, nullptr);
    const char* largeImage = OptionalLuaString(L, 3, "icon_transparent_");
    const char* largeText = OptionalLuaString(L, 4, "");
    const char* smallImage = OptionalLuaString(L, 5, "");
    const char* smallText = OptionalLuaString(L, 6, "");

    // The DLL sends the first argument as details and the second as state, kept for the existing interface code.
    RPC::UpdateActivity(details ? details : "", status ? status : "", largeImage, largeText, smallImage, smallText);

    return 0;
}

static int32_t Script_ToggleDiscord(lua_State* L) {
    bool enabled = lua_tonumber(L, 1) == 1;

    // The CVar handler does the init or shutdown, and setting it through the CVar saves the choice.
    if (s_enabledCvar) {
        s_enabledCvar->Set(enabled ? "1" : "0", true, false, false, true);
    }

    return 0;
}

class DiscordModule : public Module {
    public:
    const char* Name() const override {
        return "Discord";
    }

    void OnInitializeGlobal() override {
        s_enabledCvar = CVars::Register("enableDiscord", "Enable Discord Rich Presence.", "1", OnEnabledChanged);

        Lua::RegisterFunction("UpdateDiscordPresence", Script_UpdateDiscordPresence, Lua::State::Any);
        Lua::RegisterFunction("ToggleDiscord", Script_ToggleDiscord, Lua::State::Any);
    }
};

Module* GetModule() {
    static DiscordModule module;
    return &module;
}

}
