#include "HoT/Editor/MouseFunctions.h"
#include <cstdlib>
#include <fstream>
#include <string>
#include "HoT/Core/Log.h"
#include "HoT/Editor/GobEditor.h"
#include "HoT/Editor/Quat.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/GameObject_C.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Editor::MouseFunctions {

static GobEditor::DecodedGuid s_lastMouseGUID;
static float s_lastMouseHitX;
static float s_lastMouseHitY;
static float s_lastMouseHitZ;

void RecordHitTest(int32_t kind, const HITTESTRESULT* result) {
    if (!result) {
        return;
    }

    s_lastMouseGUID = GobEditor::DecodeGuid(kind >= 2 ? result->m_guid.m_guid : 0);
    s_lastMouseHitX = result->m_point.x;
    s_lastMouseHitY = result->m_point.y;
    s_lastMouseHitZ = result->m_point.z;
}

static CGGameObject_C* GameObjectByLuaGuid(lua_State* L, int32_t index) {
    const char* text = lua_tolstring(L, index, nullptr);

    if (!text) {
        return nullptr;
    }

    return GobEditor::GameObjectByGuid(strtoull(text, nullptr, 10));
}

static CGGameObject_C* GameObjectByMouse() {
    return GobEditor::GameObjectByGuid(s_lastMouseGUID.m_full);
}

static void PushGameObjectPosition(lua_State* L, CGGameObject_C* gameObject) {
    lua_pushnumber(L, gameObject->m_passenger.m_position.x);
    lua_pushnumber(L, gameObject->m_passenger.m_position.y);
    lua_pushnumber(L, gameObject->m_passenger.m_position.z);
}

static int32_t Script_GetMouseWorldPosition(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    lua_pushnumber(L, s_lastMouseHitX);
    lua_pushnumber(L, s_lastMouseHitY);
    lua_pushnumber(L, s_lastMouseHitZ);

    return 3;
}

static int32_t Script_GetLastMouseoverGUID(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    lua_pushstring(L, GobEditor::GuidToString(s_lastMouseGUID.m_full).c_str());
    lua_pushnumber(L, s_lastMouseGUID.m_entry);
    lua_pushnumber(L, s_lastMouseGUID.m_low);
    lua_pushnumber(L, s_lastMouseGUID.m_type);

    return 4;
}

static int32_t Script_LogMouseoverGobValues(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    CGGameObject_C* gameObject = GameObjectByMouse();

    if (!gameObject) {
        LOG_DEBUG << "MouseoverGobValues: no game object moused over";
        return 0;
    }

    uint32_t mapID = static_cast<uint32_t>(CGGameUI::m_iCurrentMapID);
    Quat q = UnpackQuat(GobEditor::PackedRotation(gameObject));

    float roll;
    float pitch;
    float yaw;
    QuatToEuler(q, roll, pitch, yaw);

    std::ofstream file("gobpositions", std::ios::app);

    if (file.is_open()) {
        const C3Vector& position = gameObject->m_passenger.m_position;
        file << s_lastMouseGUID.m_entry << "," << mapID << "," << position.x << "," << position.y << "," << position.z << "," << yaw << "," << q.x << "," << q.y << "," << q.z << "," << q.w << "\n";
        file.close();
    }

    return 0;
}

static int32_t Script_ClearSelectedGob(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    GobEditor::ClearSelection();

    return 0;
}

static int32_t Script_SelectEditorGobByMouse(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    CGGameObject_C* gameObject = GameObjectByMouse();

    if (!gameObject || !GobEditor::SelectGameObject(s_lastMouseGUID.m_full)) {
        return 0;
    }

    lua_pushstring(L, GobEditor::GuidToString(s_lastMouseGUID.m_full).c_str());
    PushGameObjectPosition(L, gameObject);

    return 4;
}

static int32_t Script_GetSelectedGobGUID(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    lua_pushstring(L, GobEditor::GuidToString(GobEditor::CurrentSelectedGobGUID()).c_str());

    return 1;
}

static int32_t Script_GetSelectedGobPosition(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    CGGameObject_C* gameObject = GobEditor::SelectedGameObject();

    if (!gameObject) {
        return 0;
    }

    PushGameObjectPosition(L, gameObject);

    return 3;
}

static int32_t Script_GetSelectedGobRotation(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    CGGameObject_C* gameObject = GobEditor::SelectedGameObject();

    if (!gameObject) {
        return 0;
    }

    Quat q = UnpackQuat(GobEditor::PackedRotation(gameObject));

    lua_pushnumber(L, q.x);
    lua_pushnumber(L, q.y);
    lua_pushnumber(L, q.z);
    lua_pushnumber(L, q.w);

    return 4;
}

static int32_t Script_SetGobPositionByGUID(lua_State* L) {
    if (!Session::HasClearance(3)) {
        lua_pushboolean(L, false);
        return 1;
    }

    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);

    if (!gameObject) {
        lua_pushboolean(L, false);
        return 1;
    }

    gameObject->m_passenger.m_position.x = static_cast<float>(lua_tonumber(L, 2));
    gameObject->m_passenger.m_position.y = static_cast<float>(lua_tonumber(L, 3));
    gameObject->m_passenger.m_position.z = static_cast<float>(lua_tonumber(L, 4));
    gameObject->UpdateWorldObject(0);

    lua_pushboolean(L, true);

    return 1;
}

static int32_t Script_SetGobRotationByGUID(lua_State* L) {
    if (!Session::HasClearance(3)) {
        lua_pushboolean(L, false);
        return 1;
    }

    CGGameObject_C* gameObject = GameObjectByLuaGuid(L, 1);

    if (!gameObject) {
        lua_pushboolean(L, false);
        return 1;
    }

    float x = static_cast<float>(lua_tonumber(L, 2));
    float y = static_cast<float>(lua_tonumber(L, 3));
    float z = static_cast<float>(lua_tonumber(L, 4));
    float w = static_cast<float>(lua_tonumber(L, 5));

    GobEditor::PackedRotation(gameObject) = PackQuat({ w, x, y, z });
    gameObject->UpdateWorldObject(0);

    lua_pushboolean(L, true);

    return 1;
}

static int32_t Script_CommitEditorGobToServer(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    CGGameObject_C* gameObject = GobEditor::SelectedGameObject();

    if (!gameObject) {
        return 0;
    }

    uint64_t guid = GobEditor::CurrentSelectedGobGUID();
    const C3Vector& position = gameObject->m_passenger.m_position;
    Quat q = UnpackQuat(GobEditor::PackedRotation(gameObject));

    Net::Packet(Net::CMSG_GOB_EDITOR_UPDATE)
        .PutInt64(static_cast<int64_t>(guid))
        .PutFloat(position.x)
        .PutFloat(position.y)
        .PutFloat(position.z)
        .PutFloat(q.x)
        .PutFloat(q.y)
        .PutFloat(q.z)
        .PutFloat(q.w)
        .Send();

    return 0;
}

static int32_t Script_GobEditorSelectNearest(lua_State* L) {
    if (!Session::HasClearance(3)) {
        return 0;
    }

    Net::Packet(Net::CMSG_GOB_EDITOR_SELECT_NEAREST).Send();

    return 0;
}

void RegisterScripts() {
    Lua::RegisterFunction("GetMouseWorldPosition", Script_GetMouseWorldPosition);
    Lua::RegisterFunction("GetLastMouseoverGUID", Script_GetLastMouseoverGUID);
    Lua::RegisterFunction("LogMouseoverGobValues", Script_LogMouseoverGobValues);
    Lua::RegisterFunction("SelectEditorGobByMouse", Script_SelectEditorGobByMouse);
    Lua::RegisterFunction("GetSelectedGobGUID", Script_GetSelectedGobGUID);
    Lua::RegisterFunction("GetSelectedGobPosition", Script_GetSelectedGobPosition);
    Lua::RegisterFunction("GetSelectedGobRotation", Script_GetSelectedGobRotation);
    Lua::RegisterFunction("SetGobPositionByGUID", Script_SetGobPositionByGUID);
    Lua::RegisterFunction("SetGobRotationByGUID", Script_SetGobRotationByGUID);
    Lua::RegisterFunction("ClearSelectedGob", Script_ClearSelectedGob);
    Lua::RegisterFunction("CommitEditorGobToServer", Script_CommitEditorGobToServer);
    Lua::RegisterFunction("GobEditorSelectNearest", Script_GobEditorSelectNearest);
}

}
