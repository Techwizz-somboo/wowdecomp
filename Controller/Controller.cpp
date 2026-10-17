#include "Controller/Controller.h"
#include "Controller/Bindings.h"
#include "Controller/Buttons.h"
#include "Controller/CVars.h"
#include "Controller/Config.h"
#include "Controller/Devices.h"
#include "Controller/Frames.h"
#include "Controller/Input.h"
#include "Controller/Lua.h"
#include "Engine/Source/Event/Event.h"

namespace Controller {

static bool s_initialized = false;

static int32_t OnIdle(const void* data, void* param) {
    Input::OnIdle(static_cast<const EVENT_DATA_IDLE*>(data)->m_elapsedSec);
    return 1;
}

void Initialize() {
    if (s_initialized) {
        return;
    }

    s_initialized = true;
    CVars::Register();
    Configs::Load();
    Devices::Initialize();
    Input::Initialize();
    EventRegisterEx(EVENT_ID_IDLE, OnIdle, nullptr, 5.0f);
}

void Shutdown() {
    if (!s_initialized) {
        return;
    }

    EventUnregisterEx(EVENT_ID_IDLE, OnIdle, nullptr, 0xFFFFFFFF);
    Input::Release();
    Input::Shutdown();
    Devices::Shutdown();
    s_initialized = false;
}

void Poll(int32_t appActive) {
    if (!s_initialized) {
        return;
    }

    bool activity = Devices::Update();
    Input::OnPoll(appActive != 0, activity);
}

const char* KeyName(KEY key) {
    int32_t button = KeyButton(key);
    return button >= 0 ? ButtonBinding(button) : nullptr;
}

bool FindBinding(const char* key, KEYBINDING*& binding) {
    return Bindings::Find(key, binding);
}

const char** ScriptEvents(bool game, const char** stockEvents, uint32_t stockCount, uint32_t* count) {
    return Lua::ScriptEvents(game, stockEvents, stockCount, count);
}

void LoadGameUI(CStatus* status) {
    Bindings::Load();
    Lua::LoadGameUI(status);
}

FrameScript_Object::ScriptIx* GetFrameScript(CSimpleFrame* frame, const char* name, FrameScript_Object::ScriptData& data) {
    return Frames::GetScript(frame, name, data);
}

void RegisterFrameMethods(lua_State* L) {
    Frames::RegisterMethods(L);
}

void OnFrameDestroyed(CSimpleFrame* frame) {
    Frames::OnDestroyed(frame);
}

}
