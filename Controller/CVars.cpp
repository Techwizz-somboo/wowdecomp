#include "Controller/CVars.h"
#include <cstdlib>
#include "Controller/Buttons.h"
#include "Controller/Input.h"
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"

namespace Controller::CVars {

static CVar* s_enable;
static CVar* s_emulateShift;
static CVar* s_emulateCtrl;
static CVar* s_emulateAlt;
static CVar* s_cursorLeftClick;
static CVar* s_cursorRightClick;
static CVar* s_cursorAutoEnable;
static CVar* s_cursorCentering;
static CVar* s_cursorSpeedStart;
static CVar* s_cursorSpeedAccel;
static CVar* s_cursorSpeedMax;
static CVar* s_cameraYawSpeed;
static CVar* s_cameraPitchSpeed;
static CVar* s_runThreshold;
static CVar* s_stickAxisButtons;
static CVar* s_vibrationStrength;
static CVar* s_interactRange;

static bool OnEnableChanged(CVar* cvar, const char* previous, const char* value, void* arg) {
    if (!value || atoi(value) == 0) {
        Input::Release();
    }

    return true;
}

static CVar* Add(const char* name, const char* help, const char* value, bool (*callback)(CVar*, const char*, const char*, void*) = nullptr) {
    return CVar::Register(name, help, 1, value, callback, CATEGORY_DEFAULT, false, nullptr, false);
}

void Register() {
    s_enable = Add("GamePadEnable", "Enable gamepad input", "1", OnEnableChanged);
    s_emulateShift = Add("GamePadEmulateShift", "Gamepad button that acts as SHIFT, or NONE", "PADLTRIGGER");
    s_emulateCtrl = Add("GamePadEmulateCtrl", "Gamepad button that acts as CTRL, or NONE", "PADRTRIGGER");
    s_emulateAlt = Add("GamePadEmulateAlt", "Gamepad button that acts as ALT, or NONE", "NONE");
    s_cursorLeftClick = Add("GamePadCursorLeftClick", "Gamepad button that left clicks while the gamepad cursor is on", "PAD1");
    s_cursorRightClick = Add("GamePadCursorRightClick", "Gamepad button that right clicks while the gamepad cursor is on", "PAD2");
    s_cursorAutoEnable = Add("GamePadCursorAutoEnable", "Turn the gamepad cursor on while an interface window is open", "1");
    s_cursorCentering = Add("GamePadCursorCentering", "Park the hidden cursor in the screen center so mouseover acts as a crosshair", "1");
    s_cursorSpeedStart = Add("GamePadCursorSpeedStart", "Gamepad cursor start speed in screen heights per second", "0.4");
    s_cursorSpeedAccel = Add("GamePadCursorSpeedAccel", "Gamepad cursor acceleration in screen heights per second squared", "1.5");
    s_cursorSpeedMax = Add("GamePadCursorSpeedMax", "Gamepad cursor top speed in screen heights per second", "1.4");
    s_cameraYawSpeed = Add("GamePadCameraYawSpeed", "Gamepad camera turn speed multiplier", "1");
    s_cameraPitchSpeed = Add("GamePadCameraPitchSpeed", "Gamepad camera pitch speed multiplier, negative inverts", "1");
    s_runThreshold = Add("GamePadWalkThreshold", "Stick deflection below which the character walks, 0 leaves it to the walk/run toggle", "0");
    s_stickAxisButtons = Add("GamePadStickAxisButtons", "Stick directions also press the PADLSTICKUP style buttons", "0");
    s_vibrationStrength = Add("GamePadVibrationStrength", "Gamepad vibration strength from 0 to 1", "1");
    s_interactRange = Add("GamePadInteractRange", "Range in yards for the gamepad interact binding", "6");
}

static int32_t ButtonValue(CVar* cvar) {
    return cvar && cvar->m_stringValue.m_str ? ButtonFromBinding(cvar->m_stringValue.m_str) : -1;
}

static float FloatValue(CVar* cvar, float fallback) {
    return cvar ? cvar->m_floatValue : fallback;
}

bool Enabled() {
    return s_enable && s_enable->m_intValue != 0;
}

int32_t EmulateShift() {
    return ButtonValue(s_emulateShift);
}

int32_t EmulateCtrl() {
    return ButtonValue(s_emulateCtrl);
}

int32_t EmulateAlt() {
    return ButtonValue(s_emulateAlt);
}

int32_t CursorLeftClick() {
    return ButtonValue(s_cursorLeftClick);
}

int32_t CursorRightClick() {
    return ButtonValue(s_cursorRightClick);
}

bool CursorAutoEnable() {
    return s_cursorAutoEnable && s_cursorAutoEnable->m_intValue != 0;
}

bool CursorCentering() {
    return s_cursorCentering && s_cursorCentering->m_intValue != 0;
}

float CursorSpeedStart() {
    return FloatValue(s_cursorSpeedStart, 0.4f);
}

float CursorSpeedAccel() {
    return FloatValue(s_cursorSpeedAccel, 1.5f);
}

float CursorSpeedMax() {
    return FloatValue(s_cursorSpeedMax, 1.4f);
}

float CameraYawSpeed() {
    return FloatValue(s_cameraYawSpeed, 1.0f);
}

float CameraPitchSpeed() {
    return FloatValue(s_cameraPitchSpeed, 1.0f);
}

float RunThreshold() {
    return FloatValue(s_runThreshold, 0.5f);
}

bool StickAxisButtons() {
    return s_stickAxisButtons && s_stickAxisButtons->m_intValue != 0;
}

float VibrationStrength() {
    float value = FloatValue(s_vibrationStrength, 1.0f);
    return value < 0.0f ? 0.0f : value > 1.0f ? 1.0f : value;
}

float InteractRange() {
    return FloatValue(s_interactRange, 6.0f);
}

}
