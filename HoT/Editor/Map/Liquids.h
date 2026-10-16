#ifndef HOT_EDITOR_MAP_LIQUIDS_H
#define HOT_EDITOR_MAP_LIQUIDS_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Tempest/C3Vector.h"

namespace HoT::MapEditor::Liquids {

// LiquidType.dbc sound banks, which decide the vertex format a layer uses.
constexpr uint32_t kBankWater = 0;
constexpr uint32_t kBankOcean = 1;
constexpr uint32_t kBankMagma = 2;
constexpr uint32_t kBankSlime = 3;

constexpr int32_t kMaxLayers = 4;

struct TileRef {
    int32_t x = 0;
    int32_t y = 0;
};

inline int32_t CountBits(uint64_t mask) {
    int32_t count = 0;
    for (; mask; mask &= mask - 1) {
        ++count;
    }

    return count;
}

struct TypeInfo {
    uint32_t m_id = 0;
    std::string m_name;
    uint32_t m_soundBank = 0;
    uint32_t m_materialId = 0;
};

bool GetType(uint32_t id, TypeInfo& out);
void ListTypes(std::vector<TypeInfo>& out);

struct LayerInfo {
    uint32_t m_type = 0;
    uint32_t m_format = 0;
    int32_t m_cells = 0;
    float m_minHeight = 0.0f;
    float m_maxHeight = 0.0f;
};

struct ChunkInfo {
    std::vector<LayerInfo> m_layers;
    uint64_t m_fishable = 0;
    uint64_t m_fatigue = 0;
};

int32_t Add(const C3Vector& center, float radius, uint32_t type, float height,
    std::vector<TileRef>& touched, std::string& error);

int32_t Remove(const C3Vector& center, float radius, int32_t layer,
    std::vector<TileRef>& touched, std::string& error);

int32_t SetType(const C3Vector& center, float radius, int32_t layer, uint32_t type,
    std::vector<TileRef>& touched, std::string& error);

int32_t SetHeight(const C3Vector& center, float radius, int32_t layer, float height,
    bool relative, std::vector<TileRef>& touched, std::string& error);

int32_t SetFlags(const C3Vector& center, float radius, int32_t fishable, int32_t fatigue,
    std::vector<TileRef>& touched, std::string& error);

bool Describe(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, ChunkInfo& out,
    std::string& error);

}

#endif
