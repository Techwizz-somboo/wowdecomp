#ifndef HOT_EDITOR_MAP_MAPCOORDS_H
#define HOT_EDITOR_MAP_MAPCOORDS_H

#include <cmath>
#include <cstdint>
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Coords {

// World x is the stride axis and world y the fast axis at every level, as CMap::QueryAreaId reads them.
constexpr float kMapHalf = 17066.666f;
constexpr float kTileSize = 533.33333f;
constexpr float kChunkSize = kTileSize / 16.0f;
constexpr float kUnitSize = kChunkSize / 8.0f;

inline float TileCoordX(const C3Vector& world) {
    return (kMapHalf - world.y) / kTileSize;
}

inline float TileCoordY(const C3Vector& world) {
    return (kMapHalf - world.x) / kTileSize;
}

inline int32_t TileX(const C3Vector& world) {
    return static_cast<int32_t>(std::floor(TileCoordX(world)));
}

inline int32_t TileY(const C3Vector& world) {
    return static_cast<int32_t>(std::floor(TileCoordY(world)));
}

// MDDF and MODF positions are in the ADT frame, CMap::CreateDoodadDef swaps and negates them into world.
inline C3Vector AdtToWorld(const C3Vector& adt) {
    return { kMapHalf - adt.z, kMapHalf - adt.x, adt.y };
}

inline C3Vector WorldToAdt(const C3Vector& world) {
    return { kMapHalf - world.y, world.z, kMapHalf - world.x };
}

inline int32_t ChunkX(const C3Vector& world) {
    float tile = TileCoordX(world);
    return static_cast<int32_t>((tile - std::floor(tile)) * 16.0f);
}

inline int32_t ChunkY(const C3Vector& world) {
    float tile = TileCoordY(world);
    return static_cast<int32_t>((tile - std::floor(tile)) * 16.0f);
}

inline int32_t GlobalChunkX(const C3Vector& world) {
    return static_cast<int32_t>(std::floor((kMapHalf - world.y) / kChunkSize));
}

inline int32_t GlobalChunkY(const C3Vector& world) {
    return static_cast<int32_t>(std::floor((kMapHalf - world.x) / kChunkSize));
}

// MCVT is 9 outer rows of 9 interleaved with 8 inner rows of 8, so every 17 entries is one row pair.
struct VertexCoord {
    int32_t m_row = 0;
    int32_t m_col = 0;
    bool m_inner = false;
};

inline VertexCoord DecodeVertex(int32_t index) {
    VertexCoord out;
    int32_t block = index / 17;
    int32_t rem = index % 17;

    out.m_inner = rem >= 9;
    out.m_row = block;
    out.m_col = out.m_inner ? rem - 9 : rem;
    return out;
}

inline int32_t EncodeOuterVertex(int32_t row, int32_t col) {
    return row * 17 + col;
}

inline int32_t EncodeInnerVertex(int32_t row, int32_t col) {
    return row * 17 + 9 + col;
}

inline C3Vector VertexWorldPos(const CMapChunk* chunk, int32_t index) {
    C3Vector out{};
    if (!chunk || !chunk->m_height || index < 0 || index >= Access::kVertsPerChunk) {
        return out;
    }

    VertexCoord coord = DecodeVertex(index);
    float colOffset = static_cast<float>(coord.m_col) + (coord.m_inner ? 0.5f : 0.0f);
    float rowOffset = static_cast<float>(coord.m_row) + (coord.m_inner ? 0.5f : 0.0f);

    out.x = chunk->m_topLeftCoords.x - rowOffset * kUnitSize;
    out.y = chunk->m_topLeftCoords.y - colOffset * kUnitSize;
    out.z = chunk->m_topLeftCoords.z + chunk->m_height[index];
    return out;
}

inline float VertexHeight(const CMapChunk* chunk, int32_t index) {
    if (!chunk || !chunk->m_height || index < 0 || index >= Access::kVertsPerChunk) {
        return 0.0f;
    }

    return chunk->m_topLeftCoords.z + chunk->m_height[index];
}

inline float Distance2D(const C3Vector& a, const C3Vector& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// MCAL masks are 64x64 over one chunk on the same axes as the vertex grid.
constexpr int32_t kAlphaSide = 64;
constexpr float kTexelSize = kChunkSize / static_cast<float>(kAlphaSide);

inline int32_t EncodeTexel(int32_t row, int32_t col) {
    return row * kAlphaSide + col;
}

inline C3Vector TexelWorldPos(const CMapChunk* chunk, int32_t row, int32_t col) {
    C3Vector out{};
    if (!chunk) {
        return out;
    }

    out.x = chunk->m_topLeftCoords.x - (static_cast<float>(row) + 0.5f) * kTexelSize;
    out.y = chunk->m_topLeftCoords.y - (static_cast<float>(col) + 0.5f) * kTexelSize;
    out.z = chunk->m_topLeftCoords.z;
    return out;
}

inline bool CircleTouchesChunk(const CMapChunk* chunk, const C3Vector& center, float radius) {
    if (!chunk) {
        return false;
    }

    float maxX = chunk->m_topLeftCoords.x;
    float maxY = chunk->m_topLeftCoords.y;
    float minX = maxX - kChunkSize;
    float minY = maxY - kChunkSize;

    float nearestX = center.x < minX ? minX : (center.x > maxX ? maxX : center.x);
    float nearestY = center.y < minY ? minY : (center.y > maxY ? maxY : center.y);
    float dx = center.x - nearestX;
    float dy = center.y - nearestY;

    return dx * dx + dy * dy <= radius * radius;
}

}

#endif
