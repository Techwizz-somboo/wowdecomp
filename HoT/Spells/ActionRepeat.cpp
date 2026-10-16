#include "HoT/Spells/ActionRepeat.h"
#include <cstdlib>
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Event/Types.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Ui/ActionBarFrame.h"

namespace HoT::ActionRepeat {

enum Mode {
    MODE_OFF = 0,
    MODE_UNTIL_CAST = 1,
    MODE_CONTINUOUS = 2,
};

struct Held {
    const void* m_key;
    int32_t m_actionSlot;
    uint32_t m_downTimeMs;
    uint32_t m_lastRepeatMs;
    uint32_t m_readySinceMs;
    int32_t m_cooldownStartAtPress;
    bool m_fired;
    bool m_done;
    bool m_active;
};

static const uint32_t MAX_HELD = 8;
static const uint32_t NUM_BUTTONS = 12;
static const uint32_t NUM_PAGES = 6;
static const uint32_t CASTABLE_GRACE_MS = 100;

static Held s_held[MAX_HELD];
static int32_t s_mode = MODE_OFF;
static uint32_t s_delayMs = 500;
static uint32_t s_intervalMs = 100;
static bool s_eventRegistered;
static bool s_suppressUse;

static void ClearAll() {
    for (Held& held : s_held) {
        held.m_active = false;
    }
}

// This reads the targeting spell like the DLL did, so a press that opened a ground target counts as casting.
static bool IsCasting() {
    return s_targetingSpell != nullptr;
}

static int32_t ResolveActionSlot(const char* command) {
    if (!command || SStrCmpI(command, "ACTIONBUTTON", 12)) {
        return -1;
    }

    int32_t button = atoi(command + 12);

    if (button < 1 || button > static_cast<int32_t>(NUM_BUTTONS)) {
        return -1;
    }

    int32_t page = CGActionBar::m_tempPageActiveFlags ? 1 : static_cast<int32_t>(CGActionBar::m_currentPage) + 1;

    if (page == 1 && CGActionBar::m_bonusBarOffset) {
        page = NUM_PAGES + CGActionBar::m_bonusBarOffset;
    }

    return (button - 1) + (page - 1) * NUM_BUTTONS;
}

static int32_t ReadCooldownStart(int32_t slot) {
    uint32_t start = 0;
    uint32_t duration = 0;
    uint32_t enable = 0;
    CGActionBar::GetCooldown(slot, start, duration, enable);
    return static_cast<int32_t>(start);
}

static Held* Find(const void* key) {
    for (Held& held : s_held) {
        if (held.m_active && held.m_key == key) {
            return &held;
        }
    }

    return nullptr;
}

static Held* Allocate(const void* key) {
    if (Held* existing = Find(key)) {
        return existing;
    }

    for (Held& held : s_held) {
        if (!held.m_active) {
            return &held;
        }
    }

    return nullptr;
}

// Alt tabbing away drops the key up, so every hold ends with the focus change.
static int32_t OnFocus(const void* data, void* param) {
    ClearAll();
    return 1;
}

static void OnBindingDown(const void* key, int32_t actionSlot) {
    Held* held = Allocate(key);

    if (!held) {
        return;
    }

    held->m_key = key;
    held->m_actionSlot = actionSlot;
    held->m_downTimeMs = OsGetAsyncTimeMs();
    held->m_lastRepeatMs = held->m_downTimeMs;
    held->m_readySinceMs = 0;
    held->m_cooldownStartAtPress = ReadCooldownStart(actionSlot);
    held->m_fired = false;
    held->m_done = false;
    held->m_active = true;

    if (!s_eventRegistered) {
        EventRegisterEx(EVENT_ID_FOCUS, OnFocus, nullptr, 0.0f);
        s_eventRegistered = true;
    }
}

static bool OnBindingUp(const void* key) {
    Held* held = Find(key);

    if (!held) {
        return false;
    }

    held->m_active = false;
    return held->m_fired;
}

static void Update() {
    if (s_mode == MODE_OFF) {
        return;
    }

    uint32_t now = OsGetAsyncTimeMs();

    for (Held& held : s_held) {
        if (!held.m_active || (s_mode == MODE_UNTIL_CAST && held.m_done) || now - held.m_downTimeMs < s_delayMs) {
            continue;
        }

        if (s_mode == MODE_UNTIL_CAST && (ReadCooldownStart(held.m_actionSlot) > held.m_cooldownStartAtPress || IsCasting())) {
            held.m_done = true;
            continue;
        }

        if (IsCasting()) {
            held.m_readySinceMs = 0;
            continue;
        }

        uint32_t start = 0;
        uint32_t duration = 0;
        uint32_t enable = 0;
        CGActionBar::GetCooldown(held.m_actionSlot, start, duration, enable);

        if (duration > 0 && now - start < duration) {
            held.m_readySinceMs = 0;
            continue;
        }

        if (!held.m_readySinceMs) {
            held.m_readySinceMs = now;
        }

        if (now - held.m_readySinceMs < CASTABLE_GRACE_MS || now - held.m_lastRepeatMs < s_intervalMs) {
            continue;
        }

        CGActionBar::UseAction(held.m_actionSlot, WOWGUID(), "");
        held.m_lastRepeatMs = now;
        held.m_fired = true;
    }
}

KeyScope::KeyScope(const char* key, const char* command, int32_t isDown) : m_suppressing(false) {
    if (s_mode == MODE_OFF) {
        return;
    }

    int32_t actionSlot = ResolveActionSlot(command);

    if (actionSlot < 0) {
        return;
    }

    if (isDown) {
        OnBindingDown(key, actionSlot);
    } else if (OnBindingUp(key)) {
        s_suppressUse = true;
        m_suppressing = true;
    }
}

KeyScope::~KeyScope() {
    if (m_suppressing) {
        s_suppressUse = false;
    }
}

bool ConsumeSuppressUse() {
    bool suppress = s_suppressUse;
    s_suppressUse = false;
    return suppress;
}

static void OnModeChanged(const char* value) {
    int32_t mode = CVars::Int(value);
    s_mode = mode < MODE_OFF || mode > MODE_CONTINUOUS ? MODE_OFF : mode;

    if (s_mode == MODE_OFF) {
        ClearAll();
    }
}

static void OnDelayChanged(const char* value) {
    int32_t ms = CVars::Int(value);
    s_delayMs = ms < 0 ? 0 : static_cast<uint32_t>(ms);
}

static void OnIntervalChanged(const char* value) {
    int32_t ms = CVars::Int(value);
    s_intervalMs = ms < 1 ? 1 : static_cast<uint32_t>(ms);
}

class ActionRepeatModule : public Module {
    public:
    const char* Name() const override {
        return "ActionRepeat";
    }

    void OnInitializeGlobal() override {
        CVars::Register("actionButtonHoldRepeat", "Auto-repeat a held action bar keybind. 0 off, 1 until it casts, 2 continuous.", "0", OnModeChanged);
        CVars::Register("actionButtonHoldRepeatDelay", "Grace period in milliseconds before a held action bar keybind starts repeating.", "500", OnDelayChanged);
        CVars::Register("actionButtonHoldRepeatInterval", "Milliseconds between repeats of a held action bar keybind.", "100", OnIntervalChanged);
    }

    void OnDestroyGame() override {
        ClearAll();
    }

    void OnIdle() override {
        Update();
    }
};

Module* GetModule() {
    static ActionRepeatModule module;
    return &module;
}

}
