#ifndef HOT_EDITOR_MAP_ADT_LIQUIDCHUNK_H
#define HOT_EDITOR_MAP_ADT_LIQUIDCHUNK_H

#include <cstdint>
#include <string>
#include <vector>
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Adt {

constexpr int32_t kLiquidCellSide = 8;
constexpr int32_t kLiquidCells = kLiquidCellSide * kLiquidCellSide;
constexpr int32_t kLiquidVertSide = 9;
constexpr int32_t kLiquidVerts = kLiquidVertSide * kLiquidVertSide;

constexpr uint16_t kLiquidHeightDepth = 0;
constexpr uint16_t kLiquidHeightUv = 1;
constexpr uint16_t kLiquidDepthOnly = 2;

inline int32_t LiquidCell(int32_t x, int32_t y) {
    return y * kLiquidCellSide + x;
}

inline int32_t LiquidVert(int32_t x, int32_t y) {
    return y * kLiquidVertSide + x;
}

// One MH2O instance spread over the full 8x8 cell and 9x9 vertex grid of its chunk.
struct LiquidLayer {
    uint16_t m_type = 0;
    uint16_t m_format = kLiquidHeightDepth;

    bool m_exists[kLiquidCells] = {};
    float m_height[kLiquidVerts] = {};
    uint8_t m_depth[kLiquidVerts] = {};
    uint16_t m_uv[kLiquidVerts * 2] = {};

    int32_t CellCount() const;

    bool Any() const {
        return this->CellCount() != 0;
    }
};

struct ChunkLiquid {
    std::vector<LiquidLayer> m_layers;
    uint64_t m_fishable = 0;
    uint64_t m_fatigue = 0;
};

struct TileLiquid {
    ChunkLiquid m_chunks[256];
    bool m_present = false;
};

bool ReadTileLiquid(const AdtDocument& doc, TileLiquid& out, std::string& error);

bool WriteTileLiquid(const TileLiquid& in, AdtDocument& doc, std::string& error);

}

#endif
