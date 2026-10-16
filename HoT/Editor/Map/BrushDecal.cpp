#include "HoT/Editor/Map/BrushDecal.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"

namespace HoT::MapEditor::BrushDecal {

static constexpr int32_t kModeAcceptable = 0;
static constexpr int32_t kModeUnacceptable = 1;
static constexpr int32_t kModeHidden = 3;

static C3Vector* s_position;
static float* s_size;
static int32_t* s_style;

void Bind(C3Vector* position, float* size, int32_t* style) {
    s_position = position;
    s_size = size;
    s_style = style;
}

void Show(const C3Vector& center, float radius, bool valid) {
    if (Spell_C_IsTargeting() || !s_position || !s_size || !s_style) {
        return;
    }

    *s_position = center;
    *s_size = radius > 0.0f ? radius : 1.0f;
    *s_style = valid ? kModeAcceptable : kModeUnacceptable;
}

void Hide() {
    if (Spell_C_IsTargeting() || !s_style) {
        return;
    }

    *s_style = kModeHidden;
}

}
