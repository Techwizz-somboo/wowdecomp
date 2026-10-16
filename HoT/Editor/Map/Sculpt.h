#ifndef HOT_EDITOR_MAP_SCULPT_H
#define HOT_EDITOR_MAP_SCULPT_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"

namespace HoT::MapEditor::Sculpt {

enum class Falloff {
    Smooth,
    Linear,
    Flat,
};

enum class Mode {
    Raise,
    Flatten,
    Smooth,
    Noise,
};

struct Stroke {
    C3Vector m_center{};
    float m_radius = 10.0f;
    Falloff m_falloff = Falloff::Smooth;
    Mode m_mode = Mode::Raise;

    // Yards a full weight vertex moves for raise and noise.
    float m_amount = 0.0f;

    // Fraction of the way to the goal a full weight vertex moves for flatten and smooth.
    float m_blend = 0.0f;

    float m_targetHeight = 0.0f;
};

// Moves MCVT in place on every loaded chunk the brush touches and returns how many vertices moved.
int32_t Apply(const Stroke& stroke);

}

#endif
