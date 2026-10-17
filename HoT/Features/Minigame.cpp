#include <utility>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::Minigame {

static constexpr uint32_t MAX_VALUE_PAIRS = 256;
static constexpr uint32_t MAX_TEXT_LENGTH = 4096;

// uint32 count, then count pairs of (uint32 key, int32 value), each fired as HOT_MINIGAME_VALUE(key, value).
static int32_t Packet_SMSG_MINIGAME_VALUES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    std::vector<std::pair<uint32_t, int32_t>> pairs;

    {
        Net::Packet packet(msg);
        uint32_t count = packet.GetUInt32();

        if (count > MAX_VALUE_PAIRS) {
            count = MAX_VALUE_PAIRS;
        }

        pairs.reserve(count);

        for (uint32_t i = 0; i < count && packet.CanRead(8); i++) {
            uint32_t key = packet.GetUInt32();
            int32_t value = packet.GetInt32();
            pairs.emplace_back(key, value);
        }
    }

    for (const auto& [key, value] : pairs) {
        Lua::SignalEvent("HOT_MINIGAME_VALUE", "%u%d", key, value);
    }

    return 1;
}

static int32_t Packet_SMSG_MINIGAME_TEXT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    uint32_t key = 0;
    char buffer[MAX_TEXT_LENGTH];

    {
        Net::Packet packet(msg);
        key = packet.GetUInt32();
        packet.GetString(buffer, sizeof(buffer));
    }

    Lua::SignalEvent("HOT_MINIGAME_TEXT", "%u%s", key, buffer);
    return 1;
}

// MinigameCommand(command, arg1, arg2, arg3), missing arguments are sent as 0.
static int32_t Script_MinigameCommand(lua_State* L) {
    Net::Packet packet(Net::CMSG_MINIGAME_COMMAND);

    for (int i = 1; i <= 4; i++) {
        packet.PutUInt32(lua_isnumber(L, i) ? static_cast<uint32_t>(lua_tonumber(L, i)) : 0);
    }

    packet.Send();
    return 0;
}

class MinigameModule : public Module {
    public:
    const char* Name() const override {
        return "Minigame";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_MINIGAME_VALUES, Packet_SMSG_MINIGAME_VALUES);
        Net::RegisterWorldHandler(Net::SMSG_MINIGAME_TEXT, Packet_SMSG_MINIGAME_TEXT);

        Lua::RegisterFunction("MinigameCommand", Script_MinigameCommand);
    }
};

Module* GetModule() {
    static MinigameModule module;
    return &module;
}

}
