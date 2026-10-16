#ifndef HOT_EDITOR_MAP_TEXTUREBRUSH_H
#define HOT_EDITOR_MAP_TEXTUREBRUSH_H

#include <cstdint>
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/Sculpt.h"

namespace HoT::MapEditor::TextureBrush {

struct Stroke {
    C3Vector m_center{};
    float m_radius = 10.0f;
    Sculpt::Falloff m_falloff = Sculpt::Falloff::Smooth;

    int32_t m_layer = 1;

    // Positive paints the layer in and negative paints it out, the size is the rate.
    float m_blend = 0.0f;
};

int32_t Apply(const Stroke& stroke);

// Points the live chunk at a freshly written MCLY and MCAL and drops any cached painting state.
void PushChunk(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, CMapChunk* chunk,
    const Adt::Mcnk& mcnk);

void Reset();

}

#endif
