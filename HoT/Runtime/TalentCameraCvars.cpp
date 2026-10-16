#include "HoT/Core/CVars.h"
#include "HoT/Runtime/Cvars.h"

namespace HoT::TalentCameraCvars {

// Storage for FrameXML/TalentUI_Wrangler.lua so the frame reopens as the player left it, nothing in HoT reads these.
void Register() {
    CVars::Register("talentCameraX", "Talent tree camera offset right of the tree origin, in pixels.", "0");
    CVars::Register("talentCameraY", "Talent tree camera offset above the tree origin, in pixels.", "0");
    CVars::Register("talentCameraZoom", "Talent tree camera zoom: 0.25 is fully out, 1.0 fully in.", "0.45");
    CVars::Register("talentFrameSizeMode", "Talent frame window size: 1 is maximised, 2 is the smaller window.", "1");
    CVars::Register("talentUIPanSeen", "Set once the talent tree's drag-to-pan hint has been shown.", "0");
}

}
