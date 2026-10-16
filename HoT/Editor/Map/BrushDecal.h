#ifndef HOT_EDITOR_MAP_BRUSHDECAL_H
#define HOT_EDITOR_MAP_BRUSHDECAL_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

// The brush rides on the spell cursor shadow decal CGWorldFrame::OnWorldRender draws.
namespace HoT::MapEditor::BrushDecal {

// Points the decal at the world frame's shadow statics for this frame.
void Bind(C3Vector* position, float* size, int32_t* style);

void Show(const C3Vector& center, float radius, bool valid);
void Hide();

}

#endif
