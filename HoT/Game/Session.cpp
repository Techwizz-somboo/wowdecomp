#include "HoT/Game/Session.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Core/Version.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "HoT/Streaming/MpqScanner.h"

namespace HoT::Session {

static int8_t s_securityLevel;
static bool s_inWorld;
static uint8_t s_charCreateGameMode;

int8_t SecurityLevel() {
    return s_securityLevel;
}

bool HasClearance(int8_t level) {
    return s_securityLevel >= level;
}

bool InWorld() {
    return s_inWorld;
}

void SetInWorld(bool inWorld) {
    s_inWorld = inWorld;
}

uint8_t CharCreateGameMode() {
    return s_charCreateGameMode;
}

void SetCharCreateGameMode(uint8_t mode) {
    s_charCreateGameMode = mode;
}

void SendSanityCheck(bool sendMpqs) {
    if (!sendMpqs) {
        Net::Packet packet(Net::CMSG_SANITY_CHECK);
        packet.PutString(Version::Commit());
        packet.PutInt32(0);
        packet.Send();
        return;
    }

    // One MPQ form check at a time, the queued scan answers every request made while it runs.
    static bool s_queued = false;

    if (s_queued) {
        return;
    }

    s_queued = true;

    Streaming::MpqScanner::ScanAsync([](const std::vector<Streaming::MpqInfo>& mpqs) {
        s_queued = false;

        if (!InWorld()) {
            return;
        }

        Net::Packet packet(Net::CMSG_SANITY_CHECK);
        packet.PutString(Version::Commit());
        packet.PutUInt32(static_cast<uint32_t>(mpqs.size()));

        for (const Streaming::MpqInfo& mpq : mpqs) {
            packet.PutString(mpq.m_filenameLower.c_str());
            packet.PutUInt8(mpq.m_updating ? 1 : 0);
            packet.PutString(mpq.m_digest.c_str());
        }

        packet.Send();
    });
}

static int32_t Packet_SMSG_RECEIVE_SECURITY_LEVEL(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    s_securityLevel = Net::Packet(msg).GetInt8();
    LOG_DEBUG << "security level " << static_cast<int32_t>(s_securityLevel);
    return 1;
}

static int32_t Packet_SMSG_REQUEST_CHAR_ENUM_LEVELS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt32();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        char name[64] = {};
        packet.GetString(name, sizeof(name));
        uint32_t level = packet.GetUInt32();
        uint32_t talentLevel = packet.GetUInt32();
        uint32_t subClass = packet.GetUInt32();
        uint32_t gameMode = static_cast<uint32_t>(packet.GetInt8());

        Lua::SignalEvent("HOT_CHAR_SELECT_UPDATE", "%s%u%u%u%u", name, level, talentLevel, subClass, gameMode);
    }

    return 1;
}

static int32_t Packet_SMSG_SANITY_CHECK(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    SendSanityCheck(true);
    return 1;
}

static int32_t Packet_SMSG_FUCKED_CLIENT_DETECTED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    int32_t count = packet.GetInt32();

    if (count <= 0) {
        return 1;
    }

    Lua::SignalEvent("HOT_BAD_MPQ_NUM", "%d", count);

    for (int32_t i = 0; i < count && packet.CanRead(1); i++) {
        char filename[256] = {};
        packet.GetString(filename, sizeof(filename));
        uint32_t crc = packet.GetUInt32();

        Lua::SignalEvent("HOT_BAD_MPQ_FILE_INFO", "%s%u", filename, crc);
    }

    return 1;
}

static int32_t Script_GetSecurityLevel(lua_State* L) {
    lua_pushnumber(L, s_securityLevel);
    return 1;
}

static int32_t Script_GetDllVersion(lua_State* L) {
    lua_pushstring(L, Version::Commit());
    return 1;
}

class SessionModule : public Module {
    public:
    const char* Name() const override {
        return "Session";
    }

    void OnInitializeGlobal() override {
        Net::RegisterRealmHandler(Net::SMSG_RECEIVE_SECURITY_LEVEL, Packet_SMSG_RECEIVE_SECURITY_LEVEL);
        Net::RegisterRealmHandler(Net::SMSG_REQUEST_CHAR_ENUM_LEVELS, Packet_SMSG_REQUEST_CHAR_ENUM_LEVELS);
        Net::RegisterWorldHandler(Net::SMSG_SANITY_CHECK, Packet_SMSG_SANITY_CHECK);
        Net::RegisterWorldHandler(Net::SMSG_FUCKED_CLIENT_DETECTED, Packet_SMSG_FUCKED_CLIENT_DETECTED);

        Lua::RegisterFunction("GetSecurityLevel", Script_GetSecurityLevel, Lua::State::Any);
        Lua::RegisterFunction("GetPlayerSecurityLevel", Script_GetSecurityLevel, Lua::State::Game);
        Lua::RegisterFunction("GetDllVersion", Script_GetDllVersion, Lua::State::Any);
    }
};

Module* GetModule() {
    static SessionModule module;
    return &module;
}

}
