#include <string>
#include <utility>
#include <vector>
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"

namespace HoT::GroupFinder {

// Long enough for the server side limits (title 63, comment 191) plus the terminator.
static constexpr uint32_t STRING_BUFFER = 256;

struct ActivityGroup {
    uint16_t m_id = 0;
    uint8_t m_categoryId = 0;
    std::string m_name;
    uint16_t m_orderIndex = 0;
};

struct Activity {
    uint16_t m_id = 0;
    uint8_t m_categoryId = 0;
    uint16_t m_groupId = 0;
    std::string m_fullName;
    std::string m_shortName;
    uint8_t m_minLevel = 0;
    uint8_t m_maxPlayers = 5;
    uint16_t m_itemLevelSuggestion = 0;
    uint16_t m_mapId = 0;
    uint8_t m_difficultyId = 0;
    uint16_t m_orderIndex = 0;
    uint32_t m_flags = 0;
};

struct SearchResult {
    uint32_t m_searchResultId = 0;
    uint16_t m_activityId = 0;
    std::string m_name;
    std::string m_comment;
    std::string m_leaderName;
    uint8_t m_leaderSubClass = 0;
    uint16_t m_requiredItemLevel = 0;
    uint8_t m_numMembers = 0;
    uint8_t m_maxMembers = 0;
    uint8_t m_numTanks = 0;
    uint8_t m_numHealers = 0;
    uint8_t m_numDamage = 0;
    std::vector<std::pair<uint8_t, uint8_t>> m_subClassTally;
    uint8_t m_autoAccept = 0;
    uint8_t m_hasSelf = 0;
    uint32_t m_ageSeconds = 0;
    uint8_t m_applicationStatus = 0;
};

struct ApplicantMember {
    std::string m_name;
    uint8_t m_level = 0;
    uint8_t m_subClass = 0;
    uint8_t m_roleMask = 0;
    uint8_t m_assignedRole = 0;
    uint16_t m_itemLevel = 0;
};

struct Applicant {
    uint32_t m_applicantId = 0;
    uint8_t m_status = 0;
    std::string m_comment;
    std::vector<ApplicantMember> m_members;
};

struct Application {
    uint32_t m_searchResultId = 0;
    uint8_t m_status = 0;
};

struct ActiveEntry {
    bool m_valid = false;
    uint32_t m_listingId = 0;
    uint16_t m_activityId = 0;
    std::string m_title;
    std::string m_comment;
    uint16_t m_requiredItemLevel = 0;
    uint8_t m_autoAccept = 0;
    uint32_t m_ageSeconds = 0;
    uint32_t m_numApplications = 0;
};

static uint32_t s_activityVersion;
static bool s_hasActivities;
static std::vector<ActivityGroup> s_activityGroups;
static std::vector<Activity> s_activities;
static ActiveEntry s_activeEntry;
static std::vector<SearchResult> s_searchResults;
static std::vector<Application> s_applications;
static std::vector<Applicant> s_applicants;

static const char* LuaOptString(lua_State* L, int32_t index) {
    if (lua_gettop(L) < index) {
        return "";
    }

    const char* value = lua_tolstring(L, index, nullptr);
    return value ? value : "";
}

static uint32_t LuaOptNumber(lua_State* L, int32_t index, uint32_t fallback = 0) {
    if (lua_gettop(L) < index || !lua_isnumber(L, index)) {
        return fallback;
    }

    return static_cast<uint32_t>(lua_tonumber(L, index));
}

static std::string ReadString(Net::Packet& packet) {
    char buffer[STRING_BUFFER];
    packet.GetString(buffer, sizeof(buffer));
    return buffer;
}

static const Activity* FindActivity(uint16_t activityId) {
    for (const Activity& activity : s_activities) {
        if (activity.m_id == activityId) {
            return &activity;
        }
    }

    return nullptr;
}

// Status 0 means the application is gone, so drop it rather than keeping a dead row.
static void SetApplicationStatus(uint32_t searchResultId, uint8_t status) {
    for (size_t i = 0; i < s_applications.size(); i++) {
        if (s_applications[i].m_searchResultId != searchResultId) {
            continue;
        }

        if (status == 0) {
            s_applications.erase(s_applications.begin() + i);
        } else {
            s_applications[i].m_status = status;
        }

        return;
    }

    if (status == 0) {
        return;
    }

    Application application;
    application.m_searchResultId = searchResultId;
    application.m_status = status;
    s_applications.push_back(application);
}

static void ReadSearchResult(Net::Packet& packet, SearchResult& out) {
    out.m_searchResultId = packet.GetUInt32();
    out.m_activityId = packet.GetUInt16();
    out.m_name = ReadString(packet);
    out.m_comment = ReadString(packet);
    out.m_leaderName = ReadString(packet);
    out.m_leaderSubClass = packet.GetUInt8();
    out.m_requiredItemLevel = packet.GetUInt16();
    out.m_numMembers = packet.GetUInt8();
    out.m_maxMembers = packet.GetUInt8();
    out.m_numTanks = packet.GetUInt8();
    out.m_numHealers = packet.GetUInt8();
    out.m_numDamage = packet.GetUInt8();

    uint8_t tallyCount = packet.GetUInt8();
    out.m_subClassTally.clear();

    for (uint8_t i = 0; i < tallyCount && packet.CanRead(1); i++) {
        uint8_t subClass = packet.GetUInt8();
        uint8_t count = packet.GetUInt8();
        out.m_subClassTally.emplace_back(subClass, count);
    }

    out.m_autoAccept = packet.GetUInt8();
    out.m_hasSelf = packet.GetUInt8();
    out.m_ageSeconds = packet.GetUInt32();
    out.m_applicationStatus = packet.GetUInt8();
}

static void ReadApplicant(Net::Packet& packet, Applicant& out) {
    out.m_applicantId = packet.GetUInt32();
    out.m_status = packet.GetUInt8();
    out.m_comment = ReadString(packet);

    uint8_t memberCount = packet.GetUInt8();
    out.m_members.clear();

    for (uint8_t i = 0; i < memberCount && packet.CanRead(1); i++) {
        ApplicantMember member;
        member.m_name = ReadString(packet);
        member.m_level = packet.GetUInt8();
        member.m_subClass = packet.GetUInt8();
        member.m_roleMask = packet.GetUInt8();
        member.m_assignedRole = packet.GetUInt8();
        member.m_itemLevel = packet.GetUInt16();
        out.m_members.push_back(std::move(member));
    }
}

static void Reset() {
    s_activityVersion = 0;
    s_hasActivities = false;
    s_activityGroups.clear();
    s_activities.clear();
    s_activeEntry = ActiveEntry();
    s_searchResults.clear();
    s_applications.clear();
    s_applicants.clear();
}

static int32_t Packet_SMSG_LFG_LIST_ACTIVITY_CACHE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_activityVersion = packet.GetUInt32();
    s_activityGroups.clear();
    s_activities.clear();

    uint32_t groupCount = packet.GetUInt32();

    for (uint32_t i = 0; i < groupCount && packet.CanRead(1); i++) {
        ActivityGroup group;
        group.m_id = packet.GetUInt16();
        group.m_categoryId = packet.GetUInt8();
        group.m_name = ReadString(packet);
        group.m_orderIndex = packet.GetUInt16();
        s_activityGroups.push_back(std::move(group));
    }

    uint32_t activityCount = packet.GetUInt32();

    for (uint32_t i = 0; i < activityCount && packet.CanRead(1); i++) {
        Activity activity;
        activity.m_id = packet.GetUInt16();
        activity.m_categoryId = packet.GetUInt8();
        activity.m_groupId = packet.GetUInt16();
        activity.m_fullName = ReadString(packet);
        activity.m_shortName = ReadString(packet);
        activity.m_minLevel = packet.GetUInt8();
        activity.m_maxPlayers = packet.GetUInt8();
        activity.m_itemLevelSuggestion = packet.GetUInt16();
        activity.m_mapId = packet.GetUInt16();
        activity.m_difficultyId = packet.GetUInt8();
        activity.m_orderIndex = packet.GetUInt16();
        activity.m_flags = packet.GetUInt32();
        s_activities.push_back(std::move(activity));
    }

    s_hasActivities = true;
    Lua::SignalEvent("HOT_LFG_LIST_ACTIVITIES_UPDATE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_ACTIVE_ENTRY(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    s_activeEntry = ActiveEntry();

    if (packet.GetUInt8()) {
        s_activeEntry.m_valid = true;
        s_activeEntry.m_listingId = packet.GetUInt32();
        s_activeEntry.m_activityId = packet.GetUInt16();
        s_activeEntry.m_title = ReadString(packet);
        s_activeEntry.m_comment = ReadString(packet);
        s_activeEntry.m_requiredItemLevel = packet.GetUInt16();
        s_activeEntry.m_autoAccept = packet.GetUInt8();
        s_activeEntry.m_ageSeconds = packet.GetUInt32();
        s_activeEntry.m_numApplications = packet.GetUInt32();
    } else {
        // Our listing is gone, so the applicant list it fed goes with it.
        s_applicants.clear();
    }

    Lua::SignalEvent("HOT_LFG_LIST_ACTIVE_ENTRY_UPDATE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_ENTRY_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t result = packet.GetUInt8();
    Lua::SignalEvent("HOT_LFG_LIST_ENTRY_RESULT", "%u", result);
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_SEARCH_RESULTS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt32();
    s_searchResults.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        SearchResult result;
        ReadSearchResult(packet, result);
        s_searchResults.push_back(std::move(result));
    }

    Lua::SignalEvent("HOT_LFG_LIST_SEARCH_RESULTS_RECEIVED", "%u", count);
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_SEARCH_RESULT_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t delisted = packet.GetUInt8();

    SearchResult result;
    ReadSearchResult(packet, result);
    uint32_t searchResultId = result.m_searchResultId;

    for (size_t i = 0; i < s_searchResults.size(); i++) {
        if (s_searchResults[i].m_searchResultId != searchResultId) {
            continue;
        }

        if (delisted) {
            s_searchResults.erase(s_searchResults.begin() + i);
        } else {
            s_searchResults[i] = std::move(result);
        }

        Lua::SignalEvent("HOT_LFG_LIST_SEARCH_RESULT_UPDATED", "%u%u", searchResultId, static_cast<uint32_t>(delisted));
        return 1;
    }

    if (delisted) {
        return 1;
    }

    s_searchResults.push_back(std::move(result));
    Lua::SignalEvent("HOT_LFG_LIST_SEARCH_RESULT_UPDATED", "%u%u", searchResultId, 0u);
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_APPLICATION_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t searchResultId = packet.GetUInt32();
    uint8_t status = packet.GetUInt8();
    uint8_t listingRemoved = packet.GetUInt8();

    SetApplicationStatus(searchResultId, status);

    // Keep the browser row in step so the button state matches without a refresh.
    for (size_t i = 0; i < s_searchResults.size(); i++) {
        if (s_searchResults[i].m_searchResultId != searchResultId) {
            continue;
        }

        if (listingRemoved) {
            s_searchResults.erase(s_searchResults.begin() + i);
        } else {
            s_searchResults[i].m_applicationStatus = status;
        }

        break;
    }

    Lua::SignalEvent("HOT_LFG_LIST_APPLICATION_STATUS_UPDATED", "%u%u%u", searchResultId, static_cast<uint32_t>(status), static_cast<uint32_t>(listingRemoved));
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_APPLICANT_LIST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt32();
    s_applicants.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        Applicant applicant;
        ReadApplicant(packet, applicant);
        s_applicants.push_back(std::move(applicant));
    }

    Lua::SignalEvent("HOT_LFG_LIST_APPLICANT_LIST_UPDATED", "%u", count);
    return 1;
}

static int32_t Packet_SMSG_LFG_LIST_APPLICANT_UPDATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);

    Applicant applicant;
    ReadApplicant(packet, applicant);
    uint32_t applicantId = applicant.m_applicantId;
    uint32_t status = applicant.m_status;

    bool found = false;

    for (Applicant& existing : s_applicants) {
        if (existing.m_applicantId != applicantId) {
            continue;
        }

        existing = std::move(applicant);
        found = true;
        break;
    }

    if (!found) {
        s_applicants.push_back(std::move(applicant));
    }

    Lua::SignalEvent("HOT_LFG_LIST_APPLICANT_UPDATED", "%u%u", applicantId, status);
    return 1;
}

static int32_t PushActivity(lua_State* L, const Activity& activity) {
    lua_pushnumber(L, activity.m_id);
    lua_pushnumber(L, activity.m_categoryId);
    lua_pushnumber(L, activity.m_groupId);
    lua_pushstring(L, activity.m_fullName.c_str());
    lua_pushstring(L, activity.m_shortName.c_str());
    lua_pushnumber(L, activity.m_minLevel);
    lua_pushnumber(L, activity.m_maxPlayers);
    lua_pushnumber(L, activity.m_itemLevelSuggestion);
    lua_pushnumber(L, activity.m_mapId);
    lua_pushnumber(L, activity.m_difficultyId);
    lua_pushnumber(L, activity.m_orderIndex);
    lua_pushnumber(L, activity.m_flags);
    return 12;
}

// The server skips the payload when our version already matches.
static int32_t Script_LFGListRequestActivities(lua_State* L) {
    Net::Packet(Net::CMSG_LFG_LIST_REQUEST_ACTIVITIES).PutUInt32(s_activityVersion).Send();
    return 0;
}

static int32_t Script_LFGListGetActivityVersion(lua_State* L) {
    lua_pushnumber(L, s_activityVersion);
    return 1;
}

static int32_t Script_LFGListGetNumActivityGroups(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_activityGroups.size()));
    return 1;
}

static int32_t Script_LFGListGetActivityGroupInfo(lua_State* L) {
    uint32_t index = LuaOptNumber(L, 1);

    if (index == 0 || index > s_activityGroups.size()) {
        lua_pushnil(L);
        return 1;
    }

    const ActivityGroup& group = s_activityGroups[index - 1];
    lua_pushnumber(L, group.m_id);
    lua_pushnumber(L, group.m_categoryId);
    lua_pushstring(L, group.m_name.c_str());
    lua_pushnumber(L, group.m_orderIndex);
    return 4;
}

static int32_t Script_LFGListGetNumActivities(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_activities.size()));
    return 1;
}

static int32_t Script_LFGListGetActivityInfo(lua_State* L) {
    uint32_t index = LuaOptNumber(L, 1);

    if (index == 0 || index > s_activities.size()) {
        lua_pushnil(L);
        return 1;
    }

    return PushActivity(L, s_activities[index - 1]);
}

static int32_t Script_LFGListGetActivityInfoByID(lua_State* L) {
    const Activity* activity = FindActivity(static_cast<uint16_t>(LuaOptNumber(L, 1)));

    if (!activity) {
        lua_pushnil(L);
        return 1;
    }

    return PushActivity(L, *activity);
}

static void SendListing(lua_State* L, uint32_t opcode) {
    uint16_t activityId = static_cast<uint16_t>(LuaOptNumber(L, 1));
    const char* title = LuaOptString(L, 2);
    const char* comment = LuaOptString(L, 3);
    uint16_t requiredItemLevel = static_cast<uint16_t>(LuaOptNumber(L, 4));
    uint8_t autoAccept = lua_toboolean(L, 5) ? 1 : 0;

    Net::Packet(opcode)
        .PutUInt16(activityId)
        .PutString(title)
        .PutString(comment)
        .PutUInt16(requiredItemLevel)
        .PutUInt8(autoAccept)
        .Send();
}

static int32_t Script_LFGListCreateListing(lua_State* L) {
    SendListing(L, Net::CMSG_LFG_LIST_CREATE_LISTING);
    return 0;
}

static int32_t Script_LFGListUpdateListing(lua_State* L) {
    SendListing(L, Net::CMSG_LFG_LIST_UPDATE_LISTING);
    return 0;
}

static int32_t Script_LFGListRemoveListing(lua_State* L) {
    Net::Packet(Net::CMSG_LFG_LIST_REMOVE_LISTING).Send();
    return 0;
}

static int32_t Script_LFGListGetActiveEntryInfo(lua_State* L) {
    if (!s_activeEntry.m_valid) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, s_activeEntry.m_listingId);
    lua_pushnumber(L, s_activeEntry.m_activityId);
    lua_pushstring(L, s_activeEntry.m_title.c_str());
    lua_pushstring(L, s_activeEntry.m_comment.c_str());
    lua_pushnumber(L, s_activeEntry.m_requiredItemLevel);
    lua_pushboolean(L, s_activeEntry.m_autoAccept != 0);
    lua_pushnumber(L, s_activeEntry.m_ageSeconds);
    lua_pushnumber(L, s_activeEntry.m_numApplications);
    return 8;
}

static int32_t Script_LFGListSearch(lua_State* L) {
    uint8_t categoryId = static_cast<uint8_t>(LuaOptNumber(L, 1));
    uint16_t activityId = static_cast<uint16_t>(LuaOptNumber(L, 2));
    const char* text = LuaOptString(L, 3);
    uint8_t filterMask = static_cast<uint8_t>(LuaOptNumber(L, 4));

    Net::Packet(Net::CMSG_LFG_LIST_SEARCH)
        .PutUInt8(categoryId)
        .PutUInt16(activityId)
        .PutString(text)
        .PutUInt8(filterMask)
        .Send();
    return 0;
}

static int32_t Script_LFGListGetNumSearchResults(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_searchResults.size()));
    return 1;
}

static const SearchResult* GetSearchResult(lua_State* L) {
    uint32_t index = LuaOptNumber(L, 1);

    if (index == 0 || index > s_searchResults.size()) {
        return nullptr;
    }

    return &s_searchResults[index - 1];
}

static int32_t Script_LFGListGetSearchResultInfo(lua_State* L) {
    const SearchResult* result = GetSearchResult(L);

    if (!result) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, result->m_searchResultId);
    lua_pushnumber(L, result->m_activityId);
    lua_pushstring(L, result->m_name.c_str());
    lua_pushstring(L, result->m_comment.c_str());
    lua_pushstring(L, result->m_leaderName.c_str());
    lua_pushnumber(L, result->m_leaderSubClass);
    lua_pushnumber(L, result->m_requiredItemLevel);
    lua_pushnumber(L, result->m_ageSeconds);
    lua_pushboolean(L, result->m_autoAccept != 0);
    lua_pushboolean(L, result->m_hasSelf != 0);
    lua_pushnumber(L, result->m_applicationStatus);
    return 11;
}

static int32_t Script_LFGListGetSearchResultMemberCounts(lua_State* L) {
    const SearchResult* result = GetSearchResult(L);

    if (!result) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, result->m_numMembers);
    lua_pushnumber(L, result->m_maxMembers);
    lua_pushnumber(L, result->m_numTanks);
    lua_pushnumber(L, result->m_numHealers);
    lua_pushnumber(L, result->m_numDamage);
    return 5;
}

static int32_t Script_LFGListGetNumSearchResultSubClasses(lua_State* L) {
    const SearchResult* result = GetSearchResult(L);
    lua_pushnumber(L, result ? static_cast<lua_Number>(result->m_subClassTally.size()) : 0);
    return 1;
}

static int32_t Script_LFGListGetSearchResultSubClassInfo(lua_State* L) {
    const SearchResult* result = GetSearchResult(L);
    uint32_t tallyIndex = LuaOptNumber(L, 2);

    if (!result || tallyIndex == 0 || tallyIndex > result->m_subClassTally.size()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, result->m_subClassTally[tallyIndex - 1].first);
    lua_pushnumber(L, result->m_subClassTally[tallyIndex - 1].second);
    return 2;
}

static int32_t Script_LFGListApplyToGroup(lua_State* L) {
    uint32_t searchResultId = LuaOptNumber(L, 1);
    const char* comment = LuaOptString(L, 2);
    uint8_t roleMask = static_cast<uint8_t>(LuaOptNumber(L, 3));

    Net::Packet(Net::CMSG_LFG_LIST_APPLY)
        .PutUInt32(searchResultId)
        .PutString(comment)
        .PutUInt8(roleMask)
        .Send();
    return 0;
}

static int32_t Script_LFGListCancelApplication(lua_State* L) {
    Net::Packet(Net::CMSG_LFG_LIST_CANCEL_APPLICATION).PutUInt32(LuaOptNumber(L, 1)).Send();
    return 0;
}

static int32_t Script_LFGListGetApplicationStatus(lua_State* L) {
    uint32_t searchResultId = LuaOptNumber(L, 1);

    for (const Application& application : s_applications) {
        if (application.m_searchResultId == searchResultId) {
            lua_pushnumber(L, application.m_status);
            return 1;
        }
    }

    lua_pushnumber(L, 0);
    return 1;
}

static int32_t Script_LFGListGetNumApplications(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_applications.size()));
    return 1;
}

static int32_t Script_LFGListGetApplicationInfo(lua_State* L) {
    uint32_t index = LuaOptNumber(L, 1);

    if (index == 0 || index > s_applications.size()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, s_applications[index - 1].m_searchResultId);
    lua_pushnumber(L, s_applications[index - 1].m_status);
    return 2;
}

static int32_t Script_LFGListRefreshApplicants(lua_State* L) {
    Net::Packet(Net::CMSG_LFG_LIST_REFRESH_APPLICANTS).Send();
    return 0;
}

static int32_t Script_LFGListGetNumApplicants(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_applicants.size()));
    return 1;
}

static int32_t Script_LFGListGetApplicantInfo(lua_State* L) {
    uint32_t index = LuaOptNumber(L, 1);

    if (index == 0 || index > s_applicants.size()) {
        lua_pushnil(L);
        return 1;
    }

    const Applicant& applicant = s_applicants[index - 1];
    lua_pushnumber(L, applicant.m_applicantId);
    lua_pushnumber(L, applicant.m_status);
    lua_pushstring(L, applicant.m_comment.c_str());
    lua_pushnumber(L, static_cast<lua_Number>(applicant.m_members.size()));
    return 4;
}

static int32_t Script_LFGListGetApplicantMemberInfo(lua_State* L) {
    uint32_t index = LuaOptNumber(L, 1);
    uint32_t memberIndex = LuaOptNumber(L, 2);

    if (index == 0 || index > s_applicants.size()) {
        lua_pushnil(L);
        return 1;
    }

    const Applicant& applicant = s_applicants[index - 1];

    if (memberIndex == 0 || memberIndex > applicant.m_members.size()) {
        lua_pushnil(L);
        return 1;
    }

    const ApplicantMember& member = applicant.m_members[memberIndex - 1];
    lua_pushstring(L, member.m_name.c_str());
    lua_pushnumber(L, member.m_level);
    lua_pushnumber(L, member.m_subClass);
    lua_pushnumber(L, member.m_roleMask);
    lua_pushnumber(L, member.m_assignedRole);
    lua_pushnumber(L, member.m_itemLevel);
    return 6;
}

// LFGListInviteApplicant(applicantID [, role1, role2, ...]) takes one role per member, omitted roles keep what they applied as.
static int32_t Script_LFGListInviteApplicant(lua_State* L) {
    uint32_t applicantId = LuaOptNumber(L, 1);
    int32_t top = lua_gettop(L);
    uint8_t roleCount = top > 1 ? static_cast<uint8_t>(top - 1) : 0;

    Net::Packet packet(Net::CMSG_LFG_LIST_INVITE_APPLICANT);
    packet.PutUInt32(applicantId);
    packet.PutUInt8(roleCount);

    for (uint8_t i = 0; i < roleCount; i++) {
        packet.PutUInt8(static_cast<uint8_t>(LuaOptNumber(L, 2 + i)));
    }

    packet.Send();
    return 0;
}

static int32_t Script_LFGListDeclineApplicant(lua_State* L) {
    Net::Packet(Net::CMSG_LFG_LIST_DECLINE_APPLICANT).PutUInt32(LuaOptNumber(L, 1)).Send();
    return 0;
}

// Accepting is what joins the group, there is no party invite.
static int32_t Script_LFGListAnswerInvite(lua_State* L) {
    Net::Packet(Net::CMSG_LFG_LIST_ANSWER_INVITE)
        .PutUInt32(LuaOptNumber(L, 1))
        .PutUInt8(lua_toboolean(L, 2) ? 1 : 0)
        .Send();
    return 0;
}

class GroupFinderModule : public Module {
    public:
    const char* Name() const override {
        return "GroupFinder";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_ACTIVITY_CACHE, Packet_SMSG_LFG_LIST_ACTIVITY_CACHE);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_ACTIVE_ENTRY, Packet_SMSG_LFG_LIST_ACTIVE_ENTRY);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_ENTRY_RESULT, Packet_SMSG_LFG_LIST_ENTRY_RESULT);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_SEARCH_RESULTS, Packet_SMSG_LFG_LIST_SEARCH_RESULTS);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_SEARCH_RESULT_UPDATE, Packet_SMSG_LFG_LIST_SEARCH_RESULT_UPDATE);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_APPLICATION_UPDATE, Packet_SMSG_LFG_LIST_APPLICATION_UPDATE);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_APPLICANT_LIST, Packet_SMSG_LFG_LIST_APPLICANT_LIST);
        Net::RegisterWorldHandler(Net::SMSG_LFG_LIST_APPLICANT_UPDATE, Packet_SMSG_LFG_LIST_APPLICANT_UPDATE);

        Lua::RegisterFunction("LFGListRequestActivities", Script_LFGListRequestActivities);
        Lua::RegisterFunction("LFGListGetActivityVersion", Script_LFGListGetActivityVersion);
        Lua::RegisterFunction("LFGListGetNumActivityGroups", Script_LFGListGetNumActivityGroups);
        Lua::RegisterFunction("LFGListGetActivityGroupInfo", Script_LFGListGetActivityGroupInfo);
        Lua::RegisterFunction("LFGListGetNumActivities", Script_LFGListGetNumActivities);
        Lua::RegisterFunction("LFGListGetActivityInfo", Script_LFGListGetActivityInfo);
        Lua::RegisterFunction("LFGListGetActivityInfoByID", Script_LFGListGetActivityInfoByID);

        Lua::RegisterFunction("LFGListCreateListing", Script_LFGListCreateListing);
        Lua::RegisterFunction("LFGListUpdateListing", Script_LFGListUpdateListing);
        Lua::RegisterFunction("LFGListRemoveListing", Script_LFGListRemoveListing);
        Lua::RegisterFunction("LFGListGetActiveEntryInfo", Script_LFGListGetActiveEntryInfo);

        Lua::RegisterFunction("LFGListSearch", Script_LFGListSearch);
        Lua::RegisterFunction("LFGListGetNumSearchResults", Script_LFGListGetNumSearchResults);
        Lua::RegisterFunction("LFGListGetSearchResultInfo", Script_LFGListGetSearchResultInfo);
        Lua::RegisterFunction("LFGListGetSearchResultMemberCounts", Script_LFGListGetSearchResultMemberCounts);
        Lua::RegisterFunction("LFGListGetNumSearchResultSubClasses", Script_LFGListGetNumSearchResultSubClasses);
        Lua::RegisterFunction("LFGListGetSearchResultSubClassInfo", Script_LFGListGetSearchResultSubClassInfo);

        Lua::RegisterFunction("LFGListApplyToGroup", Script_LFGListApplyToGroup);
        Lua::RegisterFunction("LFGListCancelApplication", Script_LFGListCancelApplication);
        Lua::RegisterFunction("LFGListGetApplicationStatus", Script_LFGListGetApplicationStatus);
        Lua::RegisterFunction("LFGListGetNumApplications", Script_LFGListGetNumApplications);
        Lua::RegisterFunction("LFGListGetApplicationInfo", Script_LFGListGetApplicationInfo);

        Lua::RegisterFunction("LFGListRefreshApplicants", Script_LFGListRefreshApplicants);
        Lua::RegisterFunction("LFGListGetNumApplicants", Script_LFGListGetNumApplicants);
        Lua::RegisterFunction("LFGListGetApplicantInfo", Script_LFGListGetApplicantInfo);
        Lua::RegisterFunction("LFGListGetApplicantMemberInfo", Script_LFGListGetApplicantMemberInfo);
        Lua::RegisterFunction("LFGListInviteApplicant", Script_LFGListInviteApplicant);
        Lua::RegisterFunction("LFGListDeclineApplicant", Script_LFGListDeclineApplicant);
        Lua::RegisterFunction("LFGListAnswerInvite", Script_LFGListAnswerInvite);
    }

    // Back at character select, so anything cached for the world session is stale now.
    void OnScriptsLoaded(Lua::State state) override {
        if (state == Lua::State::Glue) {
            Reset();
        }
    }
};

Module* GetModule() {
    static GroupFinderModule module;
    return &module;
}

}
