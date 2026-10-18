#include "HoT/Core/Hotkeys.h"
#include <cstdlib>
#include <vector>
#include "Engine/Source/Event/Event.h"
#include "HoT/Core/CVars.h"

namespace HoT::Hotkeys {

struct Hotkey {
    const char* m_cvar;
    KEY m_key;
    PressFn m_onPress;
};

static std::vector<Hotkey> s_hotkeys;
static bool s_installed = false;

static KEY ParseKey(const char* value) {
    if (!value || (value[0] != 'F' && value[0] != 'f')) {
        return KEY_NONE;
    }

    int32_t number = atoi(value + 1);

    if (number < 1 || number > 12) {
        return KEY_NONE;
    }

    return static_cast<KEY>(KEY_F1 + number - 1);
}

template <size_t N>
static void OnSlotChanged(const char* value) {
    if (N < s_hotkeys.size()) {
        s_hotkeys[N].m_key = ParseKey(value);
    }
}

static const CVars::ChangeFn s_slotHandlers[] = {
    OnSlotChanged<0>, OnSlotChanged<1>, OnSlotChanged<2>, OnSlotChanged<3>,
    OnSlotChanged<4>, OnSlotChanged<5>, OnSlotChanged<6>, OnSlotChanged<7>,
};

static int32_t OnKeyDown(const void* data, void* param) {
    if (!data) {
        return 1;
    }

    const EVENT_DATA_KEY* key = static_cast<const EVENT_DATA_KEY*>(data);

    if (key->m_metaKeyState & 0x3F) {
        return 1;
    }

    for (const Hotkey& hotkey : s_hotkeys) {
        if (hotkey.m_key != KEY_NONE && hotkey.m_key == key->m_key) {
            hotkey.m_onPress();
            return 0;
        }
    }

    return 1;
}

void Register(const char* cvar, const char* help, const char* defaultKey, PressFn onPress) {
    size_t slot = s_hotkeys.size();

    if (slot >= sizeof(s_slotHandlers) / sizeof(s_slotHandlers[0])) {
        return;
    }

    s_hotkeys.push_back({ cvar, ParseKey(defaultKey), onPress });
    CVars::Register(cvar, help, defaultKey, s_slotHandlers[slot]);
}

void Install() {
    if (s_installed) {
        return;
    }

    // Above the loading screen, console and UI so the keys work everywhere.
    EventRegisterEx(EVENT_ID_KEYDOWN, OnKeyDown, nullptr, 20.0f);
    s_installed = true;
}

}
