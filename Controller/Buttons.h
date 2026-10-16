#ifndef CONTROLLER_BUTTONS_H
#define CONTROLLER_BUTTONS_H

#include <cstdint>
#include "Engine/Source/Event/Types.h"

namespace Controller {

enum Button : int32_t {
    BUTTON_PAD1 = 0,
    BUTTON_PAD2,
    BUTTON_PAD3,
    BUTTON_PAD4,
    BUTTON_PAD5,
    BUTTON_PAD6,
    BUTTON_LSTICK,
    BUTTON_RSTICK,
    BUTTON_LSHOULDER,
    BUTTON_RSHOULDER,
    BUTTON_LTRIGGER,
    BUTTON_RTRIGGER,
    BUTTON_DPAD_UP,
    BUTTON_DPAD_DOWN,
    BUTTON_DPAD_LEFT,
    BUTTON_DPAD_RIGHT,
    BUTTON_FORWARD,
    BUTTON_BACK,
    BUTTON_SYSTEM,
    BUTTON_SOCIAL,
    BUTTON_PADDLE1,
    BUTTON_PADDLE2,
    BUTTON_PADDLE3,
    BUTTON_PADDLE4,
    BUTTON_LSTICK_UP,
    BUTTON_LSTICK_DOWN,
    BUTTON_LSTICK_LEFT,
    BUTTON_LSTICK_RIGHT,
    BUTTON_RSTICK_UP,
    BUTTON_RSTICK_DOWN,
    BUTTON_RSTICK_LEFT,
    BUTTON_RSTICK_RIGHT,
    BUTTON_COUNT,
};

enum Axis : int32_t {
    AXIS_LSTICK_X = 0,
    AXIS_LSTICK_Y,
    AXIS_RSTICK_X,
    AXIS_RSTICK_Y,
    AXIS_LTRIGGER,
    AXIS_RTRIGGER,
    AXIS_COUNT,
};

// Left and Right are physical, the rest report what each stick is driving right now.
enum Stick : int32_t {
    STICK_LEFT = 0,
    STICK_RIGHT,
    STICK_MOVEMENT,
    STICK_CURSOR,
    STICK_CAMERA,
    STICK_LOOK,
    STICK_COUNT,
    STICK_PHYSICAL_COUNT = 2,
};

// Pad buttons are delivered as keys in the unused KEY range between the specials and F1.
const uint32_t KEY_PAD_FIRST = 0x240;

const char* ButtonBinding(int32_t button);
const char* ButtonConfigName(int32_t button);
const char* AxisConfigName(int32_t axis);
const char* StickConfigName(int32_t stick);

int32_t ButtonFromBinding(const char* name);
int32_t ButtonFromName(const char* name);
int32_t AxisFromName(const char* name);
int32_t StickFromName(const char* name);

KEY ButtonKey(int32_t button);
int32_t KeyButton(KEY key);

}

#endif
