#ifndef HOT_EDITOR_FREECAM_H
#define HOT_EDITOR_FREECAM_H

#include "Engine/Source/Tempest/C3Vector.h"

class CGCamera;

namespace HoT::Editor::FreeCam {

bool IsActive();
void SetActive(bool active, bool announce);

C3Vector GetPosition();
void SetPosition(const C3Vector& position);

float GetSpeed();
void SetSpeed(float speed);

void RegisterScripts();
void OnGameInitialize();
void OnGameDestroy();

// Runs after CGCamera::UpdateCallback has placed the camera for this frame.
void OnCameraUpdated(CGCamera* camera);

}

#endif
