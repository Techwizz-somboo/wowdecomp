#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::Feature {

static uint32_t s_mask;
static bool s_hasData;

bool IsEnabled(uint32_t featureId) {
    return s_hasData && featureId < 32 && (s_mask & (1u << featureId));
}

static int32_t Packet_SMSG_FEATURE_TOGGLE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    s_mask = Net::Packet(msg).GetUInt32();
    s_hasData = true;
    Lua::SignalEvent("HOT_FEATURE_UPDATE", nullptr);
    return 1;
}

static int32_t Script_RequestFeatureFlags(lua_State* L) {
    Net::Packet(Net::CMSG_FEATURE_TOGGLE_REQUEST).Send();
    return 0;
}

static int32_t Script_HasFeatureFlags(lua_State* L) {
    lua_pushboolean(L, s_hasData);
    return 1;
}

static int32_t Script_IsFeatureEnabled(lua_State* L) {
    lua_pushboolean(L, IsEnabled(static_cast<uint32_t>(lua_tonumber(L, 1))));
    return 1;
}

class FeatureModule : public Module {
    public:
    const char* Name() const override {
        return "Feature";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_FEATURE_TOGGLE, Packet_SMSG_FEATURE_TOGGLE);

        Lua::RegisterFunction("RequestFeatureFlags", Script_RequestFeatureFlags);
        Lua::RegisterFunction("HasFeatureFlags", Script_HasFeatureFlags);
        Lua::RegisterFunction("IsFeatureEnabled", Script_IsFeatureEnabled);
    }
};

Module* GetModule() {
    static FeatureModule module;
    return &module;
}

}
