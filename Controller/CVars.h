#ifndef CONTROLLER_CVARS_H
#define CONTROLLER_CVARS_H

#include <cstdint>

namespace Controller::CVars {

void Register();

bool Enabled();
int32_t EmulateShift();
int32_t EmulateCtrl();
int32_t EmulateAlt();
int32_t CursorLeftClick();
int32_t CursorRightClick();
bool CursorAutoEnable();
bool CursorCentering();
float CursorSpeedStart();
float CursorSpeedAccel();
float CursorSpeedMax();
float CameraYawSpeed();
float CameraPitchSpeed();
float RunThreshold();
bool StickAxisButtons();
float VibrationStrength();
float InteractRange();

}

#endif
