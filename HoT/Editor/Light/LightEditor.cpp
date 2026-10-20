#include "HoT/Editor/Light/LightEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Editor/GizmoDraw.h"
#include "HoT/Editor/Map/EditorWorld.h"
#include "HoT/Editor/Mode.h"
#include "HoT/Editor/VectorMath.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/LightRec.h"
#include "WoW/Source/Game/GameTime.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Camera.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"

namespace HoT::Editor::LightEditor {

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr int32_t kParamSlots = 8;
static constexpr double kUnitsPerYard = 36.0;
static constexpr double kMapOffset = 51200.0 / 3.0;
static constexpr float kMarkerRange = 2000.0f;

// The file row and the world values it produced, so an unchanged light goes back to the exact file values.
struct Coords {
    float raw[5];
    float world[5];
};

static std::unordered_map<int32_t, Coords> s_coords;

static bool s_drawEnabled = false;
static bool s_drawOthers = true;
static int32_t s_selectedId = 0;
static bool s_previewSlot = false;

static void LoadRecord(const uint8_t* record, LightRec& rec) {
    memcpy(&rec, record, sizeof(rec));
}

static void StoreRecord(uint8_t* record, const LightRec& rec) {
    memcpy(record, &rec, sizeof(rec));
}

static bool IsDefault(const LightRec& rec) {
    return rec.m_gameCoords[0] == 0.0f && rec.m_gameCoords[1] == 0.0f && rec.m_gameCoords[2] == 0.0f;
}

static void Values(const LightRec& rec, float* out) {
    out[0] = rec.m_gameCoords[0];
    out[1] = rec.m_gameCoords[1];
    out[2] = rec.m_gameCoords[2];
    out[3] = rec.m_gameFalloffStart;
    out[4] = rec.m_gameFalloffEnd;
}

static void SetValues(LightRec& rec, const float* values) {
    rec.m_gameCoords[0] = values[0];
    rec.m_gameCoords[1] = values[1];
    rec.m_gameCoords[2] = values[2];
    rec.m_gameFalloffStart = values[3];
    rec.m_gameFalloffEnd = values[4];
}

// Same mapping as TransformMapCoord with the non dungeon offset, which every loaded light got.
static void DbcToWorld(const float* raw, float* world) {
    world[0] = static_cast<float>(kMapOffset - raw[2] / kUnitsPerYard);
    world[1] = static_cast<float>(kMapOffset - raw[0] / kUnitsPerYard);
    world[2] = static_cast<float>(raw[1] / kUnitsPerYard);
    world[3] = static_cast<float>(raw[3] / kUnitsPerYard);
    world[4] = static_cast<float>(raw[4] / kUnitsPerYard);
}

static void WorldToDbc(const float* world, float* raw) {
    raw[0] = static_cast<float>((kMapOffset - world[1]) * kUnitsPerYard);
    raw[1] = static_cast<float>(world[2] * kUnitsPerYard);
    raw[2] = static_cast<float>((kMapOffset - world[0]) * kUnitsPerYard);
    raw[3] = static_cast<float>(world[3] * kUnitsPerYard);
    raw[4] = static_cast<float>(world[4] * kUnitsPerYard);
}

void RecordToDbc(uint8_t* record) {
    LightRec rec;
    LoadRecord(record, rec);

    if (IsDefault(rec)) {
        return;
    }

    float world[5];
    float raw[5];
    Values(rec, world);
    auto it = s_coords.find(rec.m_id);

    if (it != s_coords.end() && memcmp(it->second.world, world, sizeof(world)) == 0) {
        memcpy(raw, it->second.raw, sizeof(raw));
    } else {
        WorldToDbc(world, raw);
    }

    SetValues(rec, raw);
    StoreRecord(record, rec);
}

void RecordToWorld(uint8_t* record) {
    LightRec rec;
    LoadRecord(record, rec);

    if (IsDefault(rec)) {
        return;
    }

    Coords coords;
    Values(rec, coords.raw);
    DbcToWorld(coords.raw, coords.world);
    s_coords[rec.m_id] = coords;

    SetValues(rec, coords.world);
    StoreRecord(record, rec);
}

void OnLightWritten() {
    if (!DayNight::g_areaLights.m_count) {
        return;
    }

    DayNight::LightLoad(DayNight::g_areaLights, DayNight::s_mapId);
}

static bool HasAccess() {
    return Session::HasClearance(kRequiredSecurityLevel);
}

static float Distance(const C3Vector& a, const LightRec& light) {
    float dx = a.x - light.m_gameCoords[0];
    float dy = a.y - light.m_gameCoords[1];
    float dz = a.z - light.m_gameCoords[2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// The fade BlendAreaLight gives a light at that distance, 0 outside the falloff end.
static float Weight(const LightRec& light, float distance) {
    if (IsDefault(light) || distance * distance >= light.m_gameFalloffEnd * light.m_gameFalloffEnd) {
        return 0.0f;
    }

    if (distance <= light.m_gameFalloffStart) {
        return 1.0f;
    }

    float range = light.m_gameFalloffEnd - light.m_gameFalloffStart;
    float fade = range > 0.0f ? 1.0f - (distance - light.m_gameFalloffStart) / range : 0.0f;
    return std::clamp(fade, 0.0f, 1.0f);
}

static int32_t CurrentMap() {
    return DayNight::s_mapId;
}

static const LightRec* DefaultLight() {
    return DayNight::g_areaLights.m_count ? DayNight::g_areaLights.m_data[0] : nullptr;
}

static int32_t HalfMinutes(float progression) {
    int32_t time = static_cast<int32_t>(std::clamp(progression, 0.0f, 1.0f) * 2880.0f + 0.5f);
    return time % 2880;
}

static void PushLight(lua_State* L, const LightRec& light, const C3Vector& reference) {
    bool isDefault = IsDefault(light);
    float distance = isDefault ? 0.0f : Distance(reference, light);

    lua_createtable(L, 0, 12);
    lua_pushnumber(L, light.m_id);
    lua_setfield(L, -2, "id");
    lua_pushnumber(L, light.m_continentID);
    lua_setfield(L, -2, "map");
    lua_pushnumber(L, light.m_gameCoords[0]);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, light.m_gameCoords[1]);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, light.m_gameCoords[2]);
    lua_setfield(L, -2, "z");
    lua_pushnumber(L, isDefault ? light.m_gameFalloffStart / kUnitsPerYard : light.m_gameFalloffStart);
    lua_setfield(L, -2, "falloffStart");
    lua_pushnumber(L, isDefault ? light.m_gameFalloffEnd / kUnitsPerYard : light.m_gameFalloffEnd);
    lua_setfield(L, -2, "falloffEnd");
    lua_pushboolean(L, isDefault);
    lua_setfield(L, -2, "isDefault");
    lua_pushnumber(L, distance);
    lua_setfield(L, -2, "distance");
    lua_pushnumber(L, Weight(light, distance));
    lua_setfield(L, -2, "weight");

    lua_createtable(L, kParamSlots, 0);

    for (int32_t i = 0; i < kParamSlots; i++) {
        lua_pushnumber(L, light.m_lightParamsID[i]);
        lua_rawseti(L, -2, i + 1);
    }

    lua_setfield(L, -2, "params");
}

// LightEditor_GetLights([mapId]) -> { { id, map, x, y, z, falloffStart, falloffEnd, isDefault, distance, weight, params = { 8 ids } }, ... } in world yards.
static int32_t Script_GetLights(lua_State* L) {
    int32_t mapId = lua_type(L, 1) == LUA_TNUMBER ? static_cast<int32_t>(lua_tonumber(L, 1)) : CurrentMap();
    const C3Vector& reference = DayNight::g_dnInfo.m_lightRefPos;

    lua_newtable(L);
    int result = lua_gettop(L);
    int n = 0;

    for (int32_t i = 0; i < g_lightDB.m_numRecords && g_lightDB.m_records; i++) {
        const LightRec& light = g_lightDB.m_records[i];

        if (light.m_continentID != mapId) {
            continue;
        }

        PushLight(L, light, reference);
        lua_rawseti(L, result, ++n);
    }

    return 1;
}

// LightEditor_GetLight(id) -> the same fields as one GetLights entry, or nil.
static int32_t Script_GetLight(lua_State* L) {
    const LightRec* light = lua_type(L, 1) == LUA_TNUMBER ? g_lightDB.GetRecord(static_cast<int32_t>(lua_tonumber(L, 1))) : nullptr;

    if (!light) {
        lua_pushnil(L);
        return 1;
    }

    PushLight(L, *light, DayNight::g_dnInfo.m_lightRefPos);
    return 1;
}

// LightEditor_GetState() -> { mapId, defaultId, refX, refY, refZ, storm, overrideSlot, time, serverTime, active = { { id, weight, distance }, ... } }.
static int32_t Script_GetState(lua_State* L) {
    const DayNight::DNInfo& info = DayNight::g_dnInfo;
    const C3Vector& reference = info.m_lightRefPos;
    const LightRec* fallback = DefaultLight();

    lua_createtable(L, 0, 11);
    lua_pushnumber(L, CurrentMap());
    lua_setfield(L, -2, "mapId");
    lua_pushnumber(L, fallback ? fallback->m_id : 0);
    lua_setfield(L, -2, "defaultId");
    lua_pushnumber(L, reference.x);
    lua_setfield(L, -2, "refX");
    lua_pushnumber(L, reference.y);
    lua_setfield(L, -2, "refY");
    lua_pushnumber(L, reference.z);
    lua_setfield(L, -2, "refZ");
    lua_pushnumber(L, info.m_stormBlend);
    lua_setfield(L, -2, "storm");
    lua_pushnumber(L, static_cast<int32_t>(info.m_overrideLightParamsId));
    lua_setfield(L, -2, "overrideSlot");
    lua_pushnumber(L, HalfMinutes(info.m_dayProgression));
    lua_setfield(L, -2, "time");
    lua_pushnumber(L, HalfMinutes(g_clientGameTime.GameTimeGetDayProgression()));
    lua_setfield(L, -2, "serverTime");
    lua_pushboolean(L, Map::EditorWorld::HasTimeOfDay() ? 1 : 0);
    lua_setfield(L, -2, "frozen");

    struct Active {
        int32_t id;
        float weight;
        float distance;
    };

    std::vector<Active> active;

    for (uint32_t i = 1; i < DayNight::g_areaLights.m_count; i++) {
        const LightRec* light = DayNight::g_areaLights.m_data[i];

        if (!light) {
            continue;
        }

        float distance = Distance(reference, *light);
        float weight = Weight(*light, distance);

        if (weight > 0.0f) {
            active.push_back({ light->m_id, weight, distance });
        }
    }

    std::sort(active.begin(), active.end(), [](const Active& a, const Active& b) { return a.distance < b.distance; });

    lua_createtable(L, static_cast<int>(active.size()), 0);

    for (size_t i = 0; i < active.size(); i++) {
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, active[i].id);
        lua_setfield(L, -2, "id");
        lua_pushnumber(L, active[i].weight);
        lua_setfield(L, -2, "weight");
        lua_pushnumber(L, active[i].distance);
        lua_setfield(L, -2, "distance");
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    lua_setfield(L, -2, "active");
    return 1;
}

// LightEditor_SetTime(halfMinutes) freezes the time of day at that time until LightEditor_ClearTime or the editor closes.
static int32_t Script_SetTime(lua_State* L) {
    if (lua_type(L, 1) != LUA_TNUMBER) {
        return 0;
    }

    int32_t time = static_cast<int32_t>(lua_tonumber(L, 1));
    time = ((time % 2880) + 2880) % 2880;
    Map::EditorWorld::SetTimeOfDay(static_cast<float>(time) * 0.5f);
    return 0;
}

// LightEditor_ClearTime() goes back to the server time.
static int32_t Script_ClearTime(lua_State* L) {
    Map::EditorWorld::ClearTimeOfDay();
    return 0;
}

// LightEditor_SetPreviewSlot(slot 0-7 or nil) shows that params slot of the map default light, the way a screen effect does.
static int32_t Script_SetPreviewSlot(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER && lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) < kParamSlots) {
        DayNight::SetOverrideLightParamsID(static_cast<uint32_t>(lua_tonumber(L, 1)));
        s_previewSlot = true;
    } else if (s_previewSlot) {
        DayNight::ClearOverrideLightParamsID();
        s_previewSlot = false;
    }

    return 0;
}

// LightEditor_SetDisplay(enabled, selectedId, showOthers) controls the world markers.
static int32_t Script_SetDisplay(lua_State* L) {
    s_drawEnabled = lua_toboolean(L, 1) != 0;
    s_selectedId = lua_type(L, 2) == LUA_TNUMBER ? static_cast<int32_t>(lua_tonumber(L, 2)) : 0;
    s_drawOthers = lua_isnoneornil(L, 3) || lua_toboolean(L, 3) != 0;
    return 0;
}

// LightEditor_WorldToDbc(x, y, z [, falloffStart, falloffEnd]) -> the DBC values, zero coordinates stay zero.
static int32_t Script_WorldToDbc(lua_State* L) {
    float world[5];
    float raw[5];

    for (int32_t i = 0; i < 5; i++) {
        world[i] = static_cast<float>(lua_tonumber(L, i + 1));
    }

    if (world[0] == 0.0f && world[1] == 0.0f && world[2] == 0.0f) {
        memcpy(raw, world, sizeof(raw));
    } else {
        WorldToDbc(world, raw);
    }

    for (float value : raw) {
        lua_pushnumber(L, value);
    }

    return 5;
}

// LightEditor_DbcToWorld(x, y, z [, falloffStart, falloffEnd]) -> world yards, zero coordinates stay zero.
static int32_t Script_DbcToWorld(lua_State* L) {
    float raw[5];
    float world[5];

    for (int32_t i = 0; i < 5; i++) {
        raw[i] = static_cast<float>(lua_tonumber(L, i + 1));
    }

    if (raw[0] == 0.0f && raw[1] == 0.0f && raw[2] == 0.0f) {
        memcpy(world, raw, sizeof(world));
    } else {
        DbcToWorld(raw, world);
    }

    for (float value : world) {
        lua_pushnumber(L, value);
    }

    return 5;
}

// LightEditor_GetPlayerPosition() -> x, y, z of the character, nil out of the world.
static int32_t Script_GetPlayerPosition(lua_State* L) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return 0;
    }

    C3Vector position = player->GetPosition();
    lua_pushnumber(L, position.x);
    lua_pushnumber(L, position.y);
    lua_pushnumber(L, position.z);
    return 3;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!HasAccess()) {
        return 0;
    }

    return Function(L);
}

void Apply() {
    Lua::RegisterFunction("LightEditor_GetLights", Gated<Script_GetLights>);
    Lua::RegisterFunction("LightEditor_GetLight", Gated<Script_GetLight>);
    Lua::RegisterFunction("LightEditor_GetState", Gated<Script_GetState>);
    Lua::RegisterFunction("LightEditor_SetTime", Gated<Script_SetTime>);
    Lua::RegisterFunction("LightEditor_ClearTime", Gated<Script_ClearTime>);
    Lua::RegisterFunction("LightEditor_SetPreviewSlot", Gated<Script_SetPreviewSlot>);
    Lua::RegisterFunction("LightEditor_SetDisplay", Gated<Script_SetDisplay>);
    Lua::RegisterFunction("LightEditor_WorldToDbc", Gated<Script_WorldToDbc>);
    Lua::RegisterFunction("LightEditor_DbcToWorld", Gated<Script_DbcToWorld>);
    Lua::RegisterFunction("LightEditor_GetPlayerPosition", Gated<Script_GetPlayerPosition>);
}

void OnGameDestroy() {
    s_drawEnabled = false;
    s_selectedId = 0;

    if (s_previewSlot) {
        DayNight::ClearOverrideLightParamsID();
        s_previewSlot = false;
    }
}

void OnWorldRender(CGWorldFrame* frame) {
    if (!s_drawEnabled || !Mode::IsActive() || !frame || !frame->m_camera || !HasAccess() || !g_lightDB.m_records) {
        return;
    }

    CGxDevice* device = g_theGxDevicePtr;

    if (!device) {
        return;
    }

    device->RsPush();
    GxRsSet(GxRs_VertexShader, nullptr);
    GxRsSet(GxRs_PixelShader, nullptr);
    GxRsSet(GxRs_Culling, 0);
    GxRsSet(GxRs_Fog, 0);
    GxRsSet(GxRs_Lighting, 0);

    CImVector transparent;
    GxSceneClear(2, transparent);
    GxRsSet(GxRs_DepthTest, 1);

    const C3Vector& camera = frame->m_camera->m_position;
    int32_t mapId = CurrentMap();
    CImVector otherColor(60, 200, 255, 255);
    CImVector centerColor(255, 220, 40, 255);
    CImVector startColor(80, 255, 255, 255);
    CImVector endColor(40, 140, 255, 255);

    for (int32_t i = 0; i < g_lightDB.m_numRecords; i++) {
        const LightRec& light = g_lightDB.m_records[i];

        if (light.m_continentID != mapId || IsDefault(light)) {
            continue;
        }

        C3Vector center(light.m_gameCoords[0], light.m_gameCoords[1], light.m_gameCoords[2]);
        C3Vector drawPosition = VectorMath::Subtract(center, camera);

        if (light.m_id == s_selectedId) {
            GizmoDraw::DrawSphere(drawPosition, 1.0f, centerColor);
            GizmoDraw::DrawLine(drawPosition, VectorMath::Add(drawPosition, C3Vector(0.0f, 0.0f, 20.0f)), centerColor);

            if (light.m_gameFalloffStart > 0.0f) {
                GizmoDraw::DrawWireSphere(drawPosition, light.m_gameFalloffStart, startColor);
            }

            if (light.m_gameFalloffEnd > 0.0f) {
                GizmoDraw::DrawWireSphere(drawPosition, light.m_gameFalloffEnd, endColor);
            }
        } else if (s_drawOthers && VectorMath::Length(drawPosition) < kMarkerRange) {
            GizmoDraw::DrawSphere(drawPosition, 0.6f, otherColor);
        }
    }

    device->RsPop();
}

}
