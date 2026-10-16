#include <string>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::TowerDefence {

static constexpr uint32_t MAX_SCOREBOARD_ROWS = 64;
static constexpr uint32_t MAX_SAVED_GAME_ROWS = 32;
static constexpr uint32_t MAX_TOWER_ROWS = 64;

struct ScoreboardRow {
    std::string m_name;
    uint32_t m_score = 0;
    uint32_t m_wave = 0;
    uint32_t m_money = 0;
    uint32_t m_lives = 0;
    std::string m_difficulty;
    std::string m_playDate;
    bool m_highlight = false;
};

struct SavedGameRow {
    uint32_t m_id = 0;
    uint32_t m_mode = 0;
    uint32_t m_wave = 0;
    uint32_t m_lives = 0;
    uint32_t m_difficulty = 0;
};

struct TowerRow {
    uint32_t m_towerSet = 0;
    uint32_t m_towerId = 0;
    uint32_t m_cost = 0;
    std::string m_name;
    std::string m_icon;
};

static std::vector<ScoreboardRow> s_rows;
static uint32_t s_mode;
static bool s_hasData;

static std::vector<SavedGameRow> s_saves;
static bool s_hasSaves;

static std::vector<TowerRow> s_towers;
static bool s_hasTowers;

static std::string ReadString(Net::Packet& packet) {
    char buffer[128];
    packet.GetString(buffer, sizeof(buffer));
    return buffer;
}

static void SignalCounter(CDataStore* msg, const char* eventName) {
    Net::Packet packet(msg);
    uint32_t amount = packet.GetUInt32();
    Lua::SignalEvent(eventName, "%u", amount);
}

static int32_t Packet_SMSG_TOWERDEFENCE_SCOREBOARD(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_rows.clear();
    s_mode = packet.GetUInt32();

    uint32_t count = packet.GetUInt32();

    if (count > MAX_SCOREBOARD_ROWS) {
        count = MAX_SCOREBOARD_ROWS;
    }

    s_rows.reserve(count);

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        ScoreboardRow row;
        row.m_name = ReadString(packet);
        row.m_score = packet.GetUInt32();
        row.m_wave = packet.GetUInt32();
        row.m_money = packet.GetUInt32();
        row.m_lives = packet.GetUInt32();
        row.m_difficulty = ReadString(packet);
        row.m_playDate = ReadString(packet);
        row.m_highlight = packet.GetUInt8() != 0;
        s_rows.push_back(std::move(row));
    }

    s_hasData = true;
    Lua::SignalEvent("HOT_TOWERDEFENCE_SCOREBOARD", nullptr);
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_SAVED_GAMES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_saves.clear();

    uint32_t openUI = packet.GetUInt8();
    uint32_t count = packet.GetUInt32();

    if (count > MAX_SAVED_GAME_ROWS) {
        count = MAX_SAVED_GAME_ROWS;
    }

    s_saves.reserve(count);

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        SavedGameRow row;
        row.m_id = packet.GetUInt32();
        row.m_mode = packet.GetUInt32();
        row.m_wave = packet.GetUInt32();
        row.m_lives = packet.GetUInt32();
        row.m_difficulty = packet.GetUInt32();
        s_saves.push_back(row);
    }

    s_hasSaves = true;
    Lua::SignalEvent("HOT_TOWERDEFENCE_SAVED_GAMES", "%u", openUI);
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_TOWER_LIST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_towers.clear();

    uint32_t count = packet.GetUInt32();

    if (count > MAX_TOWER_ROWS) {
        count = MAX_TOWER_ROWS;
    }

    s_towers.reserve(count);

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        TowerRow row;
        row.m_towerSet = packet.GetUInt32();
        row.m_towerId = packet.GetUInt32();
        row.m_cost = packet.GetUInt32();
        row.m_name = ReadString(packet);
        row.m_icon = ReadString(packet);
        s_towers.push_back(std::move(row));
    }

    s_hasTowers = true;
    Lua::SignalEvent("HOT_TOWERDEFENCE_TOWERS", nullptr);
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_MONEY(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    SignalCounter(msg, "HOT_TOWERDEFENCE_MONEY");
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_WAVE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    SignalCounter(msg, "HOT_TOWERDEFENCE_WAVE");
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_LIVES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    SignalCounter(msg, "HOT_TOWERDEFENCE_LIVES");
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_SCORE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    SignalCounter(msg, "HOT_TOWERDEFENCE_SCORE");
    return 1;
}

static int32_t Packet_SMSG_TOWERDEFENCE_GAME_END(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Lua::SignalEvent("HOT_TOWERDEFENCE_GAME_END", nullptr);
    return 1;
}

static int32_t Script_RequestTowerDefenceScoreboard(lua_State* L) {
    uint32_t mode = static_cast<uint32_t>(lua_tonumber(L, 1));
    Net::Packet(Net::CMSG_TOWERDEFENCE_SCOREBOARD_REQUEST).PutUInt32(mode).Send();
    return 0;
}

// Returns mode and rows of name, score, wave, money, lives, difficulty, play date, plus a trailing true on the highlighted run.
static int32_t Script_GetTowerDefenceScoreboard(lua_State* L) {
    if (!s_hasData) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, s_mode);

    lua_createtable(L, static_cast<int>(s_rows.size()), 0);
    int tbl = lua_gettop(L);

    for (int i = 0; i < static_cast<int>(s_rows.size()); i++) {
        const ScoreboardRow& entry = s_rows[i];

        lua_createtable(L, entry.m_highlight ? 8 : 7, 0);
        int row = lua_gettop(L);

        lua_pushstring(L, entry.m_name.c_str());
        lua_rawseti(L, row, 1);

        lua_pushnumber(L, entry.m_score);
        lua_rawseti(L, row, 2);

        lua_pushnumber(L, entry.m_wave);
        lua_rawseti(L, row, 3);

        lua_pushnumber(L, entry.m_money);
        lua_rawseti(L, row, 4);

        lua_pushnumber(L, entry.m_lives);
        lua_rawseti(L, row, 5);

        lua_pushstring(L, entry.m_difficulty.c_str());
        lua_rawseti(L, row, 6);

        lua_pushstring(L, entry.m_playDate.c_str());
        lua_rawseti(L, row, 7);

        if (entry.m_highlight) {
            lua_pushboolean(L, 1);
            lua_rawseti(L, row, 8);
        }

        lua_rawseti(L, tbl, i + 1);
    }

    lua_settop(L, tbl);
    return 2;
}

static int32_t Script_RequestTowerDefenceSavedGames(lua_State* L) {
    Net::Packet(Net::CMSG_TOWERDEFENCE_SAVED_GAMES).Send();
    return 0;
}

// Returns an array of { id, mode, wave, lives, difficulty }.
static int32_t Script_GetTowerDefenceSavedGames(lua_State* L) {
    if (!s_hasSaves) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, static_cast<int>(s_saves.size()), 0);
    int tbl = lua_gettop(L);

    for (int i = 0; i < static_cast<int>(s_saves.size()); i++) {
        const SavedGameRow& entry = s_saves[i];

        lua_createtable(L, 5, 0);
        int row = lua_gettop(L);

        lua_pushnumber(L, entry.m_id);
        lua_rawseti(L, row, 1);

        lua_pushnumber(L, entry.m_mode);
        lua_rawseti(L, row, 2);

        lua_pushnumber(L, entry.m_wave);
        lua_rawseti(L, row, 3);

        lua_pushnumber(L, entry.m_lives);
        lua_rawseti(L, row, 4);

        lua_pushnumber(L, entry.m_difficulty);
        lua_rawseti(L, row, 5);

        lua_rawseti(L, tbl, i + 1);
    }

    lua_settop(L, tbl);
    return 1;
}

static int32_t Script_TowerDefenceSpawnTower(lua_State* L) {
    uint8_t towerId = static_cast<uint8_t>(lua_tonumber(L, 1));
    Net::Packet(Net::CMSG_TOWERDEFENCE_SPAWN_TOWER).PutUInt8(towerId).Send();
    return 0;
}

static int32_t Script_TowerDefenceCancelTower(lua_State* L) {
    Net::Packet(Net::CMSG_TOWERDEFENCE_CANCEL_TOWER).Send();
    return 0;
}

static int32_t Script_TowerDefenceConfirmTower(lua_State* L) {
    Net::Packet(Net::CMSG_TOWERDEFENCE_CONFIRM_TOWER).Send();
    return 0;
}

static int32_t Script_TowerDefenceStartWave(lua_State* L) {
    Net::Packet(Net::CMSG_TOWERDEFENCE_START_WAVE).Send();
    return 0;
}

static int32_t Script_TowerDefenceStartGame(lua_State* L) {
    uint8_t difficulty = static_cast<uint8_t>(lua_tonumber(L, 1));
    uint8_t mode = static_cast<uint8_t>(lua_tonumber(L, 2));
    Net::Packet(Net::CMSG_TOWERDEFENCE_START_GAME).PutUInt8(difficulty).PutUInt8(mode).Send();
    return 0;
}

static int32_t Script_TowerDefenceLoadGame(lua_State* L) {
    uint32_t saveId = static_cast<uint32_t>(lua_tonumber(L, 1));
    Net::Packet(Net::CMSG_TOWERDEFENCE_LOAD_GAME).PutUInt32(saveId).Send();
    return 0;
}

static int32_t Script_TowerDefenceDeleteGame(lua_State* L) {
    uint32_t saveId = static_cast<uint32_t>(lua_tonumber(L, 1));
    Net::Packet(Net::CMSG_TOWERDEFENCE_DELETE_GAME).PutUInt32(saveId).Send();
    return 0;
}

static int32_t Script_TowerDefenceEndGame(lua_State* L) {
    Net::Packet(Net::CMSG_TOWERDEFENCE_END_GAME).Send();
    return 0;
}

static int32_t Script_RequestTowerDefenceTowers(lua_State* L) {
    Net::Packet(Net::CMSG_TOWERDEFENCE_TOWER_LIST).Send();
    return 0;
}

// Returns the towers of one set as an array of { towerId, name, cost, icon }.
static int32_t Script_GetTowerDefenceTowers(lua_State* L) {
    if (!s_hasTowers) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t towerSet = static_cast<uint32_t>(lua_tonumber(L, 1));

    lua_createtable(L, static_cast<int>(s_towers.size()), 0);
    int tbl = lua_gettop(L);

    int written = 0;

    for (const TowerRow& entry : s_towers) {
        if (entry.m_towerSet != towerSet) {
            continue;
        }

        lua_createtable(L, 4, 0);
        int row = lua_gettop(L);

        lua_pushnumber(L, entry.m_towerId);
        lua_rawseti(L, row, 1);

        lua_pushstring(L, entry.m_name.c_str());
        lua_rawseti(L, row, 2);

        lua_pushnumber(L, entry.m_cost);
        lua_rawseti(L, row, 3);

        lua_pushstring(L, entry.m_icon.c_str());
        lua_rawseti(L, row, 4);

        lua_rawseti(L, tbl, ++written);
    }

    lua_settop(L, tbl);
    return 1;
}

class TowerDefenceModule : public Module {
    public:
    const char* Name() const override {
        return "TowerDefence";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_SCOREBOARD, Packet_SMSG_TOWERDEFENCE_SCOREBOARD);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_SAVED_GAMES, Packet_SMSG_TOWERDEFENCE_SAVED_GAMES);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_MONEY, Packet_SMSG_TOWERDEFENCE_MONEY);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_WAVE, Packet_SMSG_TOWERDEFENCE_WAVE);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_LIVES, Packet_SMSG_TOWERDEFENCE_LIVES);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_SCORE, Packet_SMSG_TOWERDEFENCE_SCORE);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_GAME_END, Packet_SMSG_TOWERDEFENCE_GAME_END);
        Net::RegisterWorldHandler(Net::SMSG_TOWERDEFENCE_TOWER_LIST, Packet_SMSG_TOWERDEFENCE_TOWER_LIST);

        Lua::RegisterFunction("RequestTowerDefenceScoreboard", Script_RequestTowerDefenceScoreboard);
        Lua::RegisterFunction("GetTowerDefenceScoreboard", Script_GetTowerDefenceScoreboard);
        Lua::RegisterFunction("RequestTowerDefenceSavedGames", Script_RequestTowerDefenceSavedGames);
        Lua::RegisterFunction("GetTowerDefenceSavedGames", Script_GetTowerDefenceSavedGames);
        Lua::RegisterFunction("TowerDefenceSpawnTower", Script_TowerDefenceSpawnTower);
        Lua::RegisterFunction("TowerDefenceCancelTower", Script_TowerDefenceCancelTower);
        Lua::RegisterFunction("TowerDefenceConfirmTower", Script_TowerDefenceConfirmTower);
        Lua::RegisterFunction("TowerDefenceStartWave", Script_TowerDefenceStartWave);
        Lua::RegisterFunction("TowerDefenceStartGame", Script_TowerDefenceStartGame);
        Lua::RegisterFunction("TowerDefenceLoadGame", Script_TowerDefenceLoadGame);
        Lua::RegisterFunction("TowerDefenceDeleteGame", Script_TowerDefenceDeleteGame);
        Lua::RegisterFunction("TowerDefenceEndGame", Script_TowerDefenceEndGame);
        Lua::RegisterFunction("RequestTowerDefenceTowers", Script_RequestTowerDefenceTowers);
        Lua::RegisterFunction("GetTowerDefenceTowers", Script_GetTowerDefenceTowers);
    }

    void OnEnterWorld() override {
        s_rows.clear();
        s_mode = 0;
        s_hasData = false;
        s_saves.clear();
        s_hasSaves = false;
        s_towers.clear();
        s_hasTowers = false;
    }
};

Module* GetModule() {
    static TowerDefenceModule module;
    return &module;
}

}
