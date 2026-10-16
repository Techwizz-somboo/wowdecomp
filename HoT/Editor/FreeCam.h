#ifndef HOT_EDITOR_FREECAM_H
#define HOT_EDITOR_FREECAM_H

class CGCamera;

namespace HoT::Editor::FreeCam {

void RegisterScripts();
void OnGameInitialize();
void OnGameDestroy();

// Runs after CGCamera::UpdateCallback has placed the camera for this frame.
void OnCameraUpdated(CGCamera* camera);

}

#endif
