#include "Controller/Buttons.h"
#include "Engine/Source/Storm/H/storm.h"

namespace Controller {

static const char* s_bindingNames[BUTTON_COUNT] = {
    "PAD1",
    "PAD2",
    "PAD3",
    "PAD4",
    "PAD5",
    "PAD6",
    "PADLSTICK",
    "PADRSTICK",
    "PADLSHOULDER",
    "PADRSHOULDER",
    "PADLTRIGGER",
    "PADRTRIGGER",
    "PADDUP",
    "PADDDOWN",
    "PADDLEFT",
    "PADDRIGHT",
    "PADFORWARD",
    "PADBACK",
    "PADSYSTEM",
    "PADSOCIAL",
    "PADPADDLE1",
    "PADPADDLE2",
    "PADPADDLE3",
    "PADPADDLE4",
    "PADLSTICKUP",
    "PADLSTICKDOWN",
    "PADLSTICKLEFT",
    "PADLSTICKRIGHT",
    "PADRSTICKUP",
    "PADRSTICKDOWN",
    "PADRSTICKLEFT",
    "PADRSTICKRIGHT",
};

static const char* s_configNames[BUTTON_COUNT] = {
    "Face1",
    "Face2",
    "Face3",
    "Face4",
    "Face5",
    "Face6",
    "LStickIn",
    "RStickIn",
    "LShoulder",
    "RShoulder",
    "LTrigger",
    "RTrigger",
    "DPadUp",
    "DPadDown",
    "DPadLeft",
    "DPadRight",
    "Forward",
    "Back",
    "System",
    "Social",
    "Paddle1",
    "Paddle2",
    "Paddle3",
    "Paddle4",
    "LStickUp",
    "LStickDown",
    "LStickLeft",
    "LStickRight",
    "RStickUp",
    "RStickDown",
    "RStickLeft",
    "RStickRight",
};

static const char* s_axisNames[AXIS_COUNT] = {
    "LStickX",
    "LStickY",
    "RStickX",
    "RStickY",
    "LTrigger",
    "RTrigger",
};

static const char* s_stickNames[STICK_COUNT] = {
    "Left",
    "Right",
    "Movement",
    "Cursor",
    "Camera",
    "Look",
};

static int32_t Find(const char* const* names, int32_t count, const char* name) {
    if (!name || !*name) {
        return -1;
    }

    for (int32_t i = 0; i < count; i++) {
        if (!SStrCmpI(names[i], name, STORM_MAX_STR)) {
            return i;
        }
    }

    return -1;
}

const char* ButtonBinding(int32_t button) {
    return button >= 0 && button < BUTTON_COUNT ? s_bindingNames[button] : nullptr;
}

const char* ButtonConfigName(int32_t button) {
    return button >= 0 && button < BUTTON_COUNT ? s_configNames[button] : nullptr;
}

const char* AxisConfigName(int32_t axis) {
    return axis >= 0 && axis < AXIS_COUNT ? s_axisNames[axis] : nullptr;
}

const char* StickConfigName(int32_t stick) {
    return stick >= 0 && stick < STICK_COUNT ? s_stickNames[stick] : nullptr;
}

int32_t ButtonFromBinding(const char* name) {
    return Find(s_bindingNames, BUTTON_COUNT, name);
}

int32_t ButtonFromName(const char* name) {
    int32_t button = Find(s_configNames, BUTTON_COUNT, name);
    return button >= 0 ? button : Find(s_bindingNames, BUTTON_COUNT, name);
}

int32_t AxisFromName(const char* name) {
    return Find(s_axisNames, AXIS_COUNT, name);
}

int32_t StickFromName(const char* name) {
    return Find(s_stickNames, STICK_COUNT, name);
}

KEY ButtonKey(int32_t button) {
    return static_cast<KEY>(KEY_PAD_FIRST + button);
}

int32_t KeyButton(KEY key) {
    uint32_t value = static_cast<uint32_t>(key);
    return value >= KEY_PAD_FIRST && value < KEY_PAD_FIRST + BUTTON_COUNT ? static_cast<int32_t>(value - KEY_PAD_FIRST) : -1;
}

}
