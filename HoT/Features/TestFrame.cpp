#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::TestFrame {

static char s_name[256];
static char s_shortDesc[512];
static char s_longDesc[4096];
static uint32_t s_talentLevel;
static uint32_t s_avgItemLevel;
static std::vector<uint32_t> s_allowedStatGroups;
static bool s_hasData;

static int32_t Packet_SMSG_CUSTOM_TEST_FRAME(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    packet.GetString(s_name, sizeof(s_name));
    packet.GetString(s_shortDesc, sizeof(s_shortDesc));
    packet.GetString(s_longDesc, sizeof(s_longDesc));
    s_talentLevel = packet.GetUInt32();
    s_avgItemLevel = packet.GetUInt32();

    uint32_t count = packet.GetUInt32();
    s_allowedStatGroups.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(4); i++) {
        s_allowedStatGroups.push_back(packet.GetUInt32());
    }

    s_hasData = true;
    Lua::SignalEvent("HOT_TEST_FRAME", nullptr);
    return 1;
}

static int32_t Script_GetTestFrameData(lua_State* L) {
    if (!s_hasData) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, s_name);
    lua_pushstring(L, s_shortDesc);
    lua_pushstring(L, s_longDesc);
    lua_pushnumber(L, s_talentLevel);
    lua_pushnumber(L, s_avgItemLevel);
    return 5;
}

static int32_t Script_GetTestFrameAllowedStatGroups(lua_State* L) {
    lua_createtable(L, static_cast<int32_t>(s_allowedStatGroups.size()), 0);

    for (size_t i = 0; i < s_allowedStatGroups.size(); i++) {
        lua_pushnumber(L, s_allowedStatGroups[i]);
        lua_rawseti(L, -2, static_cast<int32_t>(i + 1));
    }

    return 1;
}

static int32_t Script_SendTestFrameReply(lua_State* L) {
    Net::Packet packet(Net::CMSG_CUSTOM_TEST_FRAME_REPLY);

    for (int32_t i = 1; i <= 5; i++) {
        packet.PutUInt32(static_cast<uint32_t>(lua_tonumber(L, i)));
    }

    packet.Send();
    return 0;
}

class TestFrameModule : public Module {
    public:
    const char* Name() const override {
        return "TestFrame";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_CUSTOM_TEST_FRAME, Packet_SMSG_CUSTOM_TEST_FRAME);

        Lua::RegisterFunction("GetTestFrameData", Script_GetTestFrameData);
        Lua::RegisterFunction("GetTestFrameAllowedStatGroups", Script_GetTestFrameAllowedStatGroups);
        Lua::RegisterFunction("SendTestFrameReply", Script_SendTestFrameReply);
    }
};

Module* GetModule() {
    static TestFrameModule module;
    return &module;
}

}
