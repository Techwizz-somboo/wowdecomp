#include "Controller/Lua.h"
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>
#include "Controller/Bindings.h"
#include "Controller/Buttons.h"
#include "Controller/CVars.h"
#include "Controller/Config.h"
#include "Controller/Devices.h"
#include "Controller/Input.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/Ui/UIBindings.h"

namespace Controller::Lua {

static const char* s_eventNames[EVENT_COUNT] = {
    "GAME_PAD_ACTIVE_CHANGED",
    "GAME_PAD_ALLOW_HOVER_EVENTS_WITH_FREE_LOOK_CHANGED",
    "GAME_PAD_CONFIGS_CHANGED",
    "GAME_PAD_CONNECTED",
    "GAME_PAD_DISCONNECTED",
    "GAME_PAD_POWER_CHANGED",
};

static std::vector<const char*> s_events[2];
static uint32_t s_eventBase = 0;
static bool s_eventsLoaded = false;

void Signal(Event event, const char* format, ...) {
    if (!s_eventsLoaded || event < 0 || event >= EVENT_COUNT) {
        return;
    }

    va_list args;
    va_start(args, format);
    vFrameScript_SignalEvent(s_eventBase + event, format, args);
    va_end(args);
}

static std::optional<float> OptNumber(lua_State* L, int32_t index, const char* field) {
    lua_getfield(L, index, field);
    std::optional<float> value = lua_isnumber(L, -1) ? std::optional<float>(static_cast<float>(lua_tonumber(L, -1))) : std::nullopt;
    lua_pop(L, 1);
    return value;
}

static std::optional<int32_t> OptInt(lua_State* L, int32_t index, const char* field) {
    std::optional<float> value = OptNumber(L, index, field);
    return value ? std::optional<int32_t>(static_cast<int32_t>(*value)) : std::nullopt;
}

static std::string OptString(lua_State* L, int32_t index, const char* field) {
    lua_getfield(L, index, field);
    std::string value = lua_isstring(L, -1) ? lua_tolstring(L, -1, nullptr) : "";
    lua_pop(L, 1);
    return value;
}

static void SetNumber(lua_State* L, const char* field, double value) {
    lua_pushnumber(L, value);
    lua_setfield(L, -2, field);
}

static void SetOptNumber(lua_State* L, const char* field, const std::optional<float>& value) {
    if (value) {
        SetNumber(L, field, *value);
    }
}

static void SetString(lua_State* L, const char* field, const std::string& value) {
    if (!value.empty()) {
        lua_pushstring(L, value.c_str());
        lua_setfield(L, -2, field);
    }
}

static void SetBool(lua_State* L, const char* field, bool value) {
    lua_pushboolean(L, value);
    lua_setfield(L, -2, field);
}

static void ReadConfigID(lua_State* L, int32_t index, std::optional<int32_t>& vendorID, std::optional<int32_t>& productID) {
    vendorID.reset();
    productID.reset();

    if (lua_istable(L, index)) {
        vendorID = OptInt(L, index, "vendorID");
        productID = OptInt(L, index, "productID");
    }
}

static void PushConfigID(lua_State* L, const std::optional<int32_t>& vendorID, const std::optional<int32_t>& productID) {
    lua_newtable(L);

    if (vendorID) {
        SetNumber(L, "vendorID", *vendorID);
    }

    if (productID) {
        SetNumber(L, "productID", *productID);
    }
}

template <typename T, typename Push>
static void PushArray(lua_State* L, const char* field, const std::vector<T>& values, Push push) {
    lua_newtable(L);

    for (size_t i = 0; i < values.size(); i++) {
        lua_newtable(L);
        push(values[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    lua_setfield(L, -2, field);
}

template <typename T, typename Read>
static void ReadArray(lua_State* L, int32_t index, const char* field, std::vector<T>& values, Read read) {
    values.clear();
    lua_getfield(L, index, field);

    if (lua_istable(L, -1)) {
        int32_t table = lua_gettop(L);

        for (int i = 1;; i++) {
            lua_rawgeti(L, table, i);

            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                break;
            }

            values.push_back(read(lua_gettop(L)));
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
}

static void PushConfig(lua_State* L, const Config& config) {
    lua_newtable(L);
    SetString(L, "name", config.m_name);
    SetString(L, "comment", config.m_comment);
    SetString(L, "labelStyle", config.m_labelStyle);

    PushConfigID(L, config.m_vendorID, config.m_productID);
    lua_setfield(L, -2, "configID");

    PushArray(L, "rawButtonMappings", config.m_rawButtonMappings, [L](const RawButtonMapping& m) {
        SetNumber(L, "rawIndex", m.m_rawIndex);
        SetString(L, "button", m.m_button);
        SetString(L, "axis", m.m_axis);
        SetOptNumber(L, "axisValue", m.m_axisValue);
        SetString(L, "comment", m.m_comment);
    });

    PushArray(L, "rawAxisMappings", config.m_rawAxisMappings, [L](const RawAxisMapping& m) {
        SetNumber(L, "rawIndex", m.m_rawIndex);
        SetString(L, "axis", m.m_axis);
        SetString(L, "comment", m.m_comment);
    });

    PushArray(L, "axisConfigs", config.m_axisConfigs, [L](const AxisConfig& a) {
        SetString(L, "axis", a.m_axis);
        SetOptNumber(L, "shift", a.m_shift);
        SetOptNumber(L, "scale", a.m_scale);
        SetOptNumber(L, "deadzone", a.m_deadzone);
        SetOptNumber(L, "buttonThreshold", a.m_buttonThreshold);
        SetString(L, "buttonPos", a.m_buttonPos);
        SetString(L, "buttonNeg", a.m_buttonNeg);
        SetString(L, "comment", a.m_comment);
    });

    PushArray(L, "stickConfigs", config.m_stickConfigs, [L](const StickConfig& s) {
        SetString(L, "stick", s.m_stick);
        SetString(L, "axisX", s.m_axisX);
        SetString(L, "axisY", s.m_axisY);
        SetOptNumber(L, "deadzone", s.m_deadzone);
        SetOptNumber(L, "deadzoneX", s.m_deadzoneX);
        SetOptNumber(L, "deadzoneY", s.m_deadzoneY);
        SetString(L, "comment", s.m_comment);
    });
}

static Config ReadConfig(lua_State* L, int32_t index) {
    Config config;
    config.m_name = OptString(L, index, "name");
    config.m_comment = OptString(L, index, "comment");
    config.m_labelStyle = OptString(L, index, "labelStyle");

    lua_getfield(L, index, "configID");
    ReadConfigID(L, lua_gettop(L), config.m_vendorID, config.m_productID);
    lua_pop(L, 1);

    ReadArray(L, index, "rawButtonMappings", config.m_rawButtonMappings, [L](int32_t t) {
        RawButtonMapping m;
        m.m_rawIndex = OptInt(L, t, "rawIndex").value_or(0);
        m.m_button = OptString(L, t, "button");
        m.m_axis = OptString(L, t, "axis");
        m.m_axisValue = OptNumber(L, t, "axisValue");
        m.m_comment = OptString(L, t, "comment");
        return m;
    });

    ReadArray(L, index, "rawAxisMappings", config.m_rawAxisMappings, [L](int32_t t) {
        RawAxisMapping m;
        m.m_rawIndex = OptInt(L, t, "rawIndex").value_or(0);
        m.m_axis = OptString(L, t, "axis");
        m.m_comment = OptString(L, t, "comment");
        return m;
    });

    ReadArray(L, index, "axisConfigs", config.m_axisConfigs, [L](int32_t t) {
        AxisConfig a;
        a.m_axis = OptString(L, t, "axis");
        a.m_shift = OptNumber(L, t, "shift");
        a.m_scale = OptNumber(L, t, "scale");
        a.m_deadzone = OptNumber(L, t, "deadzone");
        a.m_buttonThreshold = OptNumber(L, t, "buttonThreshold");
        a.m_buttonPos = OptString(L, t, "buttonPos");
        a.m_buttonNeg = OptString(L, t, "buttonNeg");
        a.m_comment = OptString(L, t, "comment");
        return a;
    });

    ReadArray(L, index, "stickConfigs", config.m_stickConfigs, [L](int32_t t) {
        StickConfig s;
        s.m_stick = OptString(L, t, "stick");
        s.m_axisX = OptString(L, t, "axisX");
        s.m_axisY = OptString(L, t, "axisY");
        s.m_deadzone = OptNumber(L, t, "deadzone");
        s.m_deadzoneX = OptNumber(L, t, "deadzoneX");
        s.m_deadzoneY = OptNumber(L, t, "deadzoneY");
        s.m_comment = OptString(L, t, "comment");
        return s;
    });

    return config;
}

static Device* OptDevice(lua_State* L, int32_t index, bool& combined) {
    combined = !lua_isnumber(L, index) || static_cast<int32_t>(lua_tonumber(L, index)) == Devices::COMBINED_DEVICE_ID;
    return combined ? Devices::Active() : Devices::Find(static_cast<int32_t>(lua_tonumber(L, index)));
}

static void PushStick(lua_State* L, const StickState& stick) {
    lua_newtable(L);
    SetNumber(L, "x", stick.m_x);
    SetNumber(L, "y", stick.m_y);
    SetNumber(L, "len", stick.m_len);
}

static int32_t Script_AddSDLMapping(lua_State* L) {
    int32_t platform = static_cast<int32_t>(luaL_checknumber(L, 1));
    const char* mapping = luaL_checklstring(L, 2, nullptr);
    lua_pushboolean(L, platform == 0 && Devices::AddMapping(mapping));
    return 1;
}

static int32_t Script_ApplyConfigs(lua_State* L) {
    Configs::Save();
    Devices::ReloadConfigs();
    Signal(EVENT_CONFIGS_CHANGED, nullptr);
    return 0;
}

static int32_t Script_AxisIndexToConfigName(lua_State* L) {
    const char* name = AxisConfigName(static_cast<int32_t>(luaL_checknumber(L, 1)));
    name ? lua_pushstring(L, name) : lua_pushnil(L);
    return 1;
}

static int32_t Script_ButtonBindingToIndex(lua_State* L) {
    int32_t index = ButtonFromBinding(luaL_checklstring(L, 1, nullptr));
    index >= 0 ? lua_pushnumber(L, index) : lua_pushnil(L);
    return 1;
}

static int32_t Script_ButtonIndexToBinding(lua_State* L) {
    const char* name = ButtonBinding(static_cast<int32_t>(luaL_checknumber(L, 1)));
    name ? lua_pushstring(L, name) : lua_pushnil(L);
    return 1;
}

static int32_t Script_ButtonIndexToConfigName(lua_State* L) {
    const char* name = ButtonConfigName(static_cast<int32_t>(luaL_checknumber(L, 1)));
    name ? lua_pushstring(L, name) : lua_pushnil(L);
    return 1;
}

static int32_t Script_StickIndexToConfigName(lua_State* L) {
    const char* name = StickConfigName(static_cast<int32_t>(luaL_checknumber(L, 1)));
    name ? lua_pushstring(L, name) : lua_pushnil(L);
    return 1;
}

static int32_t Script_ClearLedColor(lua_State* L) {
    Devices::ClearLed();
    return 0;
}

static int32_t Script_DeleteConfig(lua_State* L) {
    std::optional<int32_t> vendorID;
    std::optional<int32_t> productID;
    ReadConfigID(L, 1, vendorID, productID);
    Configs::Delete(vendorID, productID);
    return 0;
}

static int32_t Script_GetActiveDeviceID(lua_State* L) {
    Device* device = Devices::Active();
    lua_pushnumber(L, device ? device->m_id : -1);
    return 1;
}

static int32_t Script_GetAllConfigIDs(lua_State* L) {
    lua_newtable(L);
    int i = 1;

    for (const Config& config : Configs::All()) {
        PushConfigID(L, config.m_vendorID, config.m_productID);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

static int32_t Script_GetAllDeviceIDs(lua_State* L) {
    lua_newtable(L);
    int i = 1;

    for (auto& device : Devices::All()) {
        lua_pushnumber(L, device->m_id);
        lua_rawseti(L, -2, i++);
    }

    return 1;
}

static int32_t Script_GetAllowHoverEventsWithFreeLook(lua_State* L) {
    lua_pushboolean(L, Input::GetAllowHoverEventsWithFreeLook());
    return 1;
}

static int32_t Script_GetCombinedDeviceID(lua_State* L) {
    lua_pushnumber(L, Devices::COMBINED_DEVICE_ID);
    return 1;
}

static int32_t Script_GetConfig(lua_State* L) {
    std::optional<int32_t> vendorID;
    std::optional<int32_t> productID;
    ReadConfigID(L, 1, vendorID, productID);
    const Config* config = Configs::Get(vendorID, productID);

    if (config) {
        PushConfig(L, *config);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int32_t Script_GetDeviceMappedState(lua_State* L) {
    bool combined;
    Device* device = OptDevice(L, 1, combined);

    if (!device) {
        lua_pushnil(L);
        return 1;
    }

    const MappedState& state = combined ? Devices::Combined() : device->m_state;
    bool cursor = Input::IsCursorControlEnabled();

    lua_newtable(L);
    lua_pushstring(L, combined ? "Combined" : device->m_name.c_str());
    lua_setfield(L, -2, "name");
    lua_pushstring(L, device->m_labelStyle.c_str());
    lua_setfield(L, -2, "labelStyle");
    SetNumber(L, "buttonCount", BUTTON_COUNT);
    SetNumber(L, "axisCount", AXIS_COUNT);
    SetNumber(L, "stickCount", STICK_COUNT);

    lua_newtable(L);

    for (int32_t i = 0; i < BUTTON_COUNT; i++) {
        lua_pushboolean(L, state.m_buttons[i]);
        lua_rawseti(L, -2, i + 1);
    }

    lua_setfield(L, -2, "buttons");
    lua_newtable(L);

    for (int32_t i = 0; i < AXIS_COUNT; i++) {
        lua_pushnumber(L, state.m_axes[i]);
        lua_rawseti(L, -2, i + 1);
    }

    lua_setfield(L, -2, "axes");

    StickState none;
    const StickState& left = state.m_sticks[STICK_LEFT];
    const StickState& right = state.m_sticks[STICK_RIGHT];
    const StickState* sticks[STICK_COUNT] = { &left, &right, cursor ? &none : &left, cursor ? &left : &none, &right, &right };

    lua_newtable(L);

    for (int32_t i = 0; i < STICK_COUNT; i++) {
        PushStick(L, *sticks[i]);
        lua_rawseti(L, -2, i + 1);
    }

    lua_setfield(L, -2, "sticks");
    return 1;
}

static int32_t Script_GetDeviceRawState(lua_State* L) {
    Device* device = Devices::Find(static_cast<int32_t>(luaL_checknumber(L, 1)));

    if (!device) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    lua_pushstring(L, device->m_name.c_str());
    lua_setfield(L, -2, "name");
    SetNumber(L, "vendorID", device->m_vendorID);
    SetNumber(L, "productID", device->m_productID);
    SetNumber(L, "rawButtonCount", static_cast<double>(device->m_rawButtons.size()));
    SetNumber(L, "rawAxisCount", static_cast<double>(device->m_rawAxes.size()));

    lua_newtable(L);

    for (size_t i = 0; i < device->m_rawButtons.size(); i++) {
        lua_pushboolean(L, device->m_rawButtons[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    lua_setfield(L, -2, "rawButtons");
    lua_newtable(L);

    for (size_t i = 0; i < device->m_rawAxes.size(); i++) {
        lua_pushnumber(L, device->m_rawAxes[i]);
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }

    lua_setfield(L, -2, "rawAxes");
    return 1;
}

static void PushColor(lua_State* L, float r, float g, float b) {
    lua_getfield(L, LUA_GLOBALSINDEX, "CreateColor");

    if (lua_isfunction(L, -1)) {
        lua_pushnumber(L, r);
        lua_pushnumber(L, g);
        lua_pushnumber(L, b);
        lua_pushnumber(L, 1.0);

        if (!lua_pcall(L, 4, 1, 0)) {
            return;
        }
    }

    lua_pop(L, 1);
    lua_newtable(L);
    SetNumber(L, "r", r);
    SetNumber(L, "g", g);
    SetNumber(L, "b", b);
    SetNumber(L, "a", 1.0);
}

static int32_t Script_GetLedColor(lua_State* L) {
    float r;
    float g;
    float b;
    Devices::GetLed(r, g, b);
    PushColor(L, r, g, b);
    return 1;
}

static int32_t Script_GetPowerLevel(lua_State* L) {
    bool combined;
    Device* device = OptDevice(L, 1, combined);
    lua_pushnumber(L, device ? device->m_powerLevel : POWER_UNKNOWN);
    return 1;
}

static int32_t Script_IsEnabled(lua_State* L) {
    lua_pushboolean(L, CVars::Enabled() && Devices::Available());
    return 1;
}

static int32_t Script_SetAllowHoverEventsWithFreeLook(lua_State* L) {
    Input::SetAllowHoverEventsWithFreeLook(lua_toboolean(L, 1) != 0);
    return 0;
}

static int32_t Script_SetConfig(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    Configs::Set(ReadConfig(L, 1));
    return 0;
}

static float ColorField(lua_State* L, const char* field) {
    return OptNumber(L, 1, field).value_or(0.0f);
}

static int32_t Script_SetLedColor(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    Devices::SetLed(ColorField(L, "r"), ColorField(L, "g"), ColorField(L, "b"));
    return 0;
}

static int32_t Script_SetVibration(lua_State* L) {
    const char* type = luaL_checklstring(L, 1, nullptr);
    Devices::SetVibration(type, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

static int32_t Script_StopVibration(lua_State* L) {
    Devices::StopVibration();
    return 0;
}

static int32_t Script_IsGamePadCursorControlEnabled(lua_State* L) {
    lua_pushboolean(L, Input::IsCursorControlEnabled());
    return 1;
}

static int32_t Script_SetGamePadCursorControl(lua_State* L) {
    Input::SetCursorControl(lua_toboolean(L, 1) != 0);
    return 0;
}

static int32_t Script_IsGamePadFreelookEnabled(lua_State* L) {
    lua_pushboolean(L, Input::IsFreeLookEnabled());
    return 1;
}

static int32_t Script_SetGamePadFreeLook(lua_State* L) {
    Input::SetCursorControl(lua_toboolean(L, 1) == 0);
    return 0;
}

static int32_t Script_SetGamePadAutoCursor(lua_State* L) {
    Input::SetAutoCursor(lua_toboolean(L, 1) != 0);
    return 0;
}

static int32_t Script_GamePadInteract(lua_State* L) {
    lua_pushboolean(L, Input::Interact());
    return 1;
}

static int32_t Script_IsBindingForGamePad(lua_State* L) {
    const char* binding = luaL_checklstring(L, 1, nullptr);
    const char* key = strrchr(binding, '-');
    lua_pushboolean(L, ButtonFromBinding(key ? key + 1 : binding) >= 0);
    return 1;
}

static int32_t Script_GetConvertedKeyOrButton(lua_State* L) {
    const char* key = luaL_checklstring(L, 1, nullptr);

    if (!strcmp(key, "LeftButton")) {
        lua_pushstring(L, "BUTTON1");
    } else if (!strcmp(key, "RightButton")) {
        lua_pushstring(L, "BUTTON2");
    } else if (!strcmp(key, "MiddleButton")) {
        lua_pushstring(L, "BUTTON3");
    } else if (!strncmp(key, "Button", 6) && key[6] >= '0' && key[6] <= '9') {
        lua_pushfstring(L, "BUTTON%s", key + 6);
    } else {
        lua_pushstring(L, key);
    }

    return 1;
}

static int32_t Script_GetBindingKey(lua_State* L) {
    const char* command = luaL_checklstring(L, 1, nullptr);
    int32_t count = 0;

    for (const char* key = Bindings::GetKey(command, 0); key; key = Bindings::GetKey(command, count)) {
        count++;
        lua_pushstring(L, key);
    }

    return count;
}

static int32_t Script_GetBindingAction(lua_State* L) {
    const char* action = Bindings::GetAction(luaL_checklstring(L, 1, nullptr));
    lua_pushstring(L, action ? action : "");
    return 1;
}

static int32_t Script_SetBinding(lua_State* L) {
    const char* key = luaL_checklstring(L, 1, nullptr);
    const char* command = lua_isstring(L, 2) ? lua_tolstring(L, 2, nullptr) : nullptr;

    if (!CGGameUI::CanPerformAction(13) || !Bindings::Set(key, command)) {
        lua_pushnil(L);
        return 1;
    }

    FrameScript_SignalEvent(SCRIPT_EVENT_UPDATE_BINDINGS, nullptr);
    lua_pushnumber(L, 1.0);
    return 1;
}

static int32_t Script_ResetBindings(lua_State* L) {
    if (CGGameUI::CanPerformAction(13)) {
        Bindings::Reset();
        FrameScript_SignalEvent(SCRIPT_EVENT_UPDATE_BINDINGS, nullptr);
    }

    return 0;
}

static const luaL_Reg s_gamePadFunctions[] = {
    { "AddSDLMapping", Script_AddSDLMapping },
    { "ApplyConfigs", Script_ApplyConfigs },
    { "AxisIndexToConfigName", Script_AxisIndexToConfigName },
    { "ButtonBindingToIndex", Script_ButtonBindingToIndex },
    { "ButtonIndexToBinding", Script_ButtonIndexToBinding },
    { "ButtonIndexToConfigName", Script_ButtonIndexToConfigName },
    { "ClearLedColor", Script_ClearLedColor },
    { "DeleteConfig", Script_DeleteConfig },
    { "GetActiveDeviceID", Script_GetActiveDeviceID },
    { "GetAllConfigIDs", Script_GetAllConfigIDs },
    { "GetAllDeviceIDs", Script_GetAllDeviceIDs },
    { "GetAllowHoverEventsWithFreeLook", Script_GetAllowHoverEventsWithFreeLook },
    { "GetBindingAction", Script_GetBindingAction },
    { "GetBindingKey", Script_GetBindingKey },
    { "GetCombinedDeviceID", Script_GetCombinedDeviceID },
    { "GetConfig", Script_GetConfig },
    { "GetDeviceMappedState", Script_GetDeviceMappedState },
    { "GetDeviceRawState", Script_GetDeviceRawState },
    { "GetLedColor", Script_GetLedColor },
    { "GetPowerLevel", Script_GetPowerLevel },
    { "IsEnabled", Script_IsEnabled },
    { "ResetBindings", Script_ResetBindings },
    { "SetAllowHoverEventsWithFreeLook", Script_SetAllowHoverEventsWithFreeLook },
    { "SetBinding", Script_SetBinding },
    { "SetConfig", Script_SetConfig },
    { "SetLedColor", Script_SetLedColor },
    { "SetVibration", Script_SetVibration },
    { "StickIndexToConfigName", Script_StickIndexToConfigName },
    { "StopVibration", Script_StopVibration },
};

static const luaL_Reg s_globalFunctions[] = {
    { "IsGamePadCursorControlEnabled", Script_IsGamePadCursorControlEnabled },
    { "SetGamePadCursorControl", Script_SetGamePadCursorControl },
    { "IsGamePadFreelookEnabled", Script_IsGamePadFreelookEnabled },
    { "SetGamePadFreeLook", Script_SetGamePadFreeLook },
    { "SetGamePadAutoCursor", Script_SetGamePadAutoCursor },
    { "GamePadInteract", Script_GamePadInteract },
    { "IsBindingForGamePad", Script_IsBindingForGamePad },
    { "GetConvertedKeyOrButton", Script_GetConvertedKeyOrButton },
};

static void SetEnum(lua_State* L, const char* name, const char* const* keys, const int32_t* values, int32_t count) {
    lua_getfield(L, LUA_GLOBALSINDEX, "Enum");

    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_GLOBALSINDEX, "Enum");
    }

    lua_newtable(L);

    for (int32_t i = 0; i < count; i++) {
        SetNumber(L, keys[i], values[i]);
    }

    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

static void RegisterFunctions() {
    lua_State* L = FrameScript_GetContext();

    for (const luaL_Reg& reg : s_globalFunctions) {
        FrameScript_RegisterFunction(reg.name, reg.func);
    }

    lua_newtable(L);

    for (const luaL_Reg& reg : s_gamePadFunctions) {
        lua_pushcfunction(L, reg.func);
        lua_setfield(L, -2, reg.name);
    }

    lua_setfield(L, LUA_GLOBALSINDEX, "C_GamePad");

    static const char* powerKeys[] = { "Critical", "Low", "Medium", "High", "Wired", "Unknown" };
    static const int32_t powerValues[] = { POWER_CRITICAL, POWER_LOW, POWER_MEDIUM, POWER_HIGH, POWER_WIRED, POWER_UNKNOWN };
    SetEnum(L, "GamePadPowerLevel", powerKeys, powerValues, 6);

    static const char* platformKeys[] = { "Windows", "Macintosh" };
    static const int32_t platformValues[] = { 0, 1 };
    SetEnum(L, "ClientPlatformType", platformKeys, platformValues, 2);
}

const char** ScriptEvents(bool game, const char** stockEvents, uint32_t stockCount, uint32_t* count) {
    RegisterFunctions();

    std::vector<const char*>& events = s_events[game ? 1 : 0];
    events.assign(stockEvents, stockEvents + stockCount);

    for (const char* name : s_eventNames) {
        events.push_back(name);
    }

    s_eventBase = stockCount;
    s_eventsLoaded = true;
    *count = static_cast<uint32_t>(events.size());
    return events.data();
}

static const char* s_bindingCommands =
    "<Bindings>"
    "<Binding name=\"TOGGLEGAMEPADCURSOR\" header=\"GAMEPAD\">SetGamePadCursorControl(not IsGamePadCursorControlEnabled())</Binding>"
    "<Binding name=\"GAMEPADINTERACT\">GamePadInteract()</Binding>"
    "</Bindings>";

static const char* s_gameUIScript = R"LUA(
local LABELS = {
    Letters = { PAD1 = "A", PAD2 = "B", PAD3 = "X", PAD4 = "Y", PADLSHOULDER = "LB", PADRSHOULDER = "RB", PADLTRIGGER = "LT", PADRTRIGGER = "RT", PADLSTICK = "LS", PADRSTICK = "RS", PADFORWARD = "Menu", PADBACK = "View", PADSYSTEM = "Guide", PADSOCIAL = "Share" },
    Shapes = { PAD1 = "Cross", PAD2 = "Circle", PAD3 = "Square", PAD4 = "Triangle", PAD5 = "Touchpad", PADLSHOULDER = "L1", PADRSHOULDER = "R1", PADLTRIGGER = "L2", PADRTRIGGER = "R2", PADLSTICK = "L3", PADRSTICK = "R3", PADFORWARD = "Options", PADBACK = "Share", PADSYSTEM = "PS", PADSOCIAL = "Mute" },
    Reverse = { PAD1 = "B", PAD2 = "A", PAD3 = "Y", PAD4 = "X", PADLSHOULDER = "L", PADRSHOULDER = "R", PADLTRIGGER = "ZL", PADRTRIGGER = "ZR", PADLSTICK = "LS", PADRSTICK = "RS", PADFORWARD = "Plus", PADBACK = "Minus", PADSYSTEM = "Home", PADSOCIAL = "Capture" },
}

local GENERIC = {
    PAD1 = "1", PAD2 = "2", PAD3 = "3", PAD4 = "4", PAD5 = "5", PAD6 = "6",
    PADLSTICK = "L Stick", PADRSTICK = "R Stick", PADLSHOULDER = "L Shoulder", PADRSHOULDER = "R Shoulder",
    PADLTRIGGER = "L Trigger", PADRTRIGGER = "R Trigger", PADDUP = "D-Pad Up", PADDDOWN = "D-Pad Down",
    PADDLEFT = "D-Pad Left", PADDRIGHT = "D-Pad Right", PADFORWARD = "Start", PADBACK = "Back",
    PADSYSTEM = "System", PADSOCIAL = "Social", PADPADDLE1 = "Paddle 1", PADPADDLE2 = "Paddle 2",
    PADPADDLE3 = "Paddle 3", PADPADDLE4 = "Paddle 4", PADLSTICKUP = "L Stick Up", PADLSTICKDOWN = "L Stick Down",
    PADLSTICKLEFT = "L Stick Left", PADLSTICKRIGHT = "L Stick Right", PADRSTICKUP = "R Stick Up",
    PADRSTICKDOWN = "R Stick Down", PADRSTICKLEFT = "R Stick Left", PADRSTICKRIGHT = "R Stick Right",
}

local owned = {}

local function SetDefault(name, value)
    if _G[name] == nil or _G[name] == owned[name] then
        _G[name] = value
        owned[name] = value
    end
end

local function ApplyLabels()
    local state = C_GamePad.GetDeviceMappedState()
    local style = LABELS[state and state.labelStyle or ""] or {}

    for i = 0, 31 do
        local binding = C_GamePad.ButtonIndexToBinding(i)

        if binding then
            SetDefault("KEY_" .. binding, "Pad " .. (style[binding] or GENERIC[binding] or binding))
        end
    end
end

SetDefault("BINDING_HEADER_GAMEPAD", "Gamepad")
SetDefault("BINDING_NAME_TOGGLEGAMEPADCURSOR", "Toggle Gamepad Cursor")
SetDefault("BINDING_NAME_GAMEPADINTERACT", "Interact With Nearest")

local function HookBindingUI()
    if not KeyBindingFrame or KeyBindingFrame.gamePadHooked then
        return
    end

    KeyBindingFrame.gamePadHooked = true

    -- Pad keys belong to the gamepad set, letting them through would clear the keyboard keys being replaced.
    local onKeyDown = KeyBindingFrame:GetScript("OnKeyDown")

    if onKeyDown then
        KeyBindingFrame:SetScript("OnKeyDown", function(self, key, ...)
            if not IsBindingForGamePad(key) then
                return onKeyDown(self, key, ...)
            end
        end)
    end
end

local PANELS = { "left", "center", "right", "doublewide", "fullscreen" }

local function AnyShown(list, prefix)
    if not list then
        return false
    end

    for _, name in ipairs(list) do
        local frame = _G[(prefix or "") .. name]

        if frame and frame.IsShown and frame:IsShown() then
            return true
        end
    end

    return false
end

local function AnyNumberedShown(prefix, count)
    for i = 1, count do
        local frame = _G[prefix .. i]

        if frame and frame:IsShown() then
            return true
        end
    end

    return false
end

local function InterfaceOpen()
    if GetUIPanel then
        for _, key in ipairs(PANELS) do
            if GetUIPanel(key) then
                return true
            end
        end
    end

    return AnyShown(UISpecialFrames) or AnyShown(UIMenus)
        or AnyNumberedShown("StaticPopup", STATICPOPUP_NUMDIALOGS or 4)
        or AnyNumberedShown("ContainerFrame", NUM_CONTAINER_FRAMES or 13)
        or (LootFrame and LootFrame:IsShown()) or (GameMenuFrame and GameMenuFrame:IsShown())
        or (SpellIsTargeting and SpellIsTargeting()) or (CursorHasItem and CursorHasItem())
        or (CursorHasSpell and CursorHasSpell()) or false
end

local watcher = CreateFrame("Frame")
local elapsedTotal = 0

watcher:SetScript("OnUpdate", function(self, elapsed)
    elapsedTotal = elapsedTotal + elapsed

    if elapsedTotal >= 0.1 then
        elapsedTotal = 0
        SetGamePadAutoCursor(InterfaceOpen())
    end
end)

watcher:SetScript("OnEvent", function(self, event, arg1)
    if event == "ADDON_LOADED" then
        HookBindingUI()
    else
        ApplyLabels()
    end
end)

watcher:RegisterEvent("ADDON_LOADED")
watcher:RegisterEvent("GAME_PAD_CONNECTED")
watcher:RegisterEvent("GAME_PAD_DISCONNECTED")
watcher:RegisterEvent("GAME_PAD_ACTIVE_CHANGED")

ApplyLabels()
HookBindingUI()
)LUA";

void LoadGameUI(CStatus* status) {
    CGUIBindings* bindings = CGUIBindings::s_bindings;

    if (bindings) {
        XMLTree* tree = XMLTree_Load(s_bindingCommands, static_cast<uint32_t>(strlen(s_bindingCommands)));

        if (tree) {
            for (XMLNode* node = XMLTree_GetRoot(tree)->m_child; node; node = node->m_next) {
                bindings->LoadBinding("Controller", node, status);
            }

            XMLTree_Free(tree);
        }
    }

    FrameScript_Execute(s_gameUIScript, "Controller", nullptr);
}

}
