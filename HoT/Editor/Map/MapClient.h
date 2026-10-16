#ifndef HOT_EDITOR_MAP_MAPCLIENT_H
#define HOT_EDITOR_MAP_MAPCLIENT_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Streaming/LooseFiles.h"
#include "WoW/Source/LiquidSystem/Types.h"
#include "WoW/Source/WorldClient/CMapBaseObjLink.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "WoW/Source/WorldClient/MapChunk.h"
#include "WoW/Source/WorldClient/MapDoodadDef.h"
#include "WoW/Source/WorldClient/MapObjDef.h"
#include "WoW/Source/WorldClient/MapRenderChunk.h"
#include "WoW/Source/WorldClient/Types.h"

namespace HoT::MapEditor {

static_assert(sizeof(SMChunk) == 128, "SMChunk must be 128 bytes");
static_assert(sizeof(SMChunkInfo) == 16, "SMChunkInfo must be 16 bytes");
static_assert(sizeof(SMAreaHeader) == 64, "SMAreaHeader must be 64 bytes");
static_assert(sizeof(SMDoodadDef) == 36, "SMDoodadDef must be 36 bytes");
static_assert(sizeof(SMMapObjDef) == 64, "SMMapObjDef must be 64 bytes");
static_assert(sizeof(SMLayer) == 16, "SMLayer must be 16 bytes");
static_assert(sizeof(SMLiquidChunkDesc) == 12, "SMLiquidChunkDesc must be 12 bytes");
static_assert(sizeof(SMLiquidInstance) == 24, "SMLiquidInstance must be 24 bytes");

namespace Access {

// WDT MPHD bit 2, whether MCAL holds 8 bit or 4 bit masks.
constexpr uint32_t kMapFlagBigAlpha = 0x4;

// CMap::VectorIntersectSubChunks tests are opt in, the client passes 0x100 for terrain.
constexpr uint32_t kIntersectTerrain = 0x100;

constexpr int32_t kTilesPerSide = 64;
constexpr int32_t kChunksPerTileSide = 16;
constexpr int32_t kChunksPerTile = kChunksPerTileSide * kChunksPerTileSide;
constexpr int32_t kVertsPerChunk = 145;

inline const char* MapName() {
    return CMap::mapName;
}

inline uint32_t MapFlags() {
    return CMap::header.m_flags;
}

inline bool UsesBigAlpha() {
    return (MapFlags() & kMapFlagBigAlpha) != 0;
}

inline bool IsActive() {
    return CMap::bActive != 0;
}

inline bool IsValidTile(int32_t tileX, int32_t tileY) {
    return tileX >= 0 && tileX < kTilesPerSide && tileY >= 0 && tileY < kTilesPerSide;
}

// CMap::PrepareArea stores tiles at index.y * 64 + index.x.
inline CMapArea* GetArea(int32_t tileX, int32_t tileY) {
    if (!IsValidTile(tileX, tileY)) {
        return nullptr;
    }

    return CMap::m_areaTable[tileY * kTilesPerSide + tileX];
}

// A tile is only safe to read or edit once its async load finished and the header resolved.
inline bool IsAreaReady(CMapArea* area) {
    return area && !area->m_asyncObject && area->m_fileBuffer && area->m_header;
}

inline CMapChunk* GetChunk(CMapArea* area, int32_t chunkX, int32_t chunkY) {
    if (!area || chunkX < 0 || chunkX >= kChunksPerTileSide || chunkY < 0 || chunkY >= kChunksPerTileSide) {
        return nullptr;
    }

    return area->m_mapChunks[chunkY * kChunksPerTileSide + chunkX];
}

inline SMChunkInfo* GetChunkInfo(CMapArea* area, int32_t chunkX, int32_t chunkY) {
    if (!area || !area->m_chunkInfo) {
        return nullptr;
    }

    if (chunkX < 0 || chunkX >= kChunksPerTileSide || chunkY < 0 || chunkY >= kChunksPerTileSide) {
        return nullptr;
    }

    return &area->m_chunkInfo[chunkY * kChunksPerTileSide + chunkX];
}

inline uint32_t TextureCount(CMapArea* area) {
    return area ? area->m_textures.m_count : 0;
}

// The entry only borrows name, and a null texture makes CMapRenderChunk::CreateLayer load it on use.
inline bool AppendTerrainTexture(CMapArea* area, const char* name, uint32_t& textureId) {
    if (!area || !name) {
        return false;
    }

    CMapAreaTexture* entry = area->m_textures.New();

    if (!entry) {
        return false;
    }

    entry->m_textureName = const_cast<char*>(name);
    entry->m_texture = nullptr;
    textureId = area->m_textures.m_count - 1;

    return true;
}

inline const char* DoodadFileName(CMapArea* area, uint32_t nameId) {
    if (!area || !area->m_m2FileNames || !area->m_modelFilenamesOffsets) {
        return nullptr;
    }

    return area->m_m2FileNames + area->m_modelFilenamesOffsets[nameId];
}

inline const char* TerrainTextureName(CMapArea* area, uint32_t textureId) {
    if (!area || !area->m_textures.m_data || textureId >= area->m_textures.m_count) {
        return nullptr;
    }

    return area->m_textures.m_data[textureId].m_textureName;
}

inline const char* MapObjFileName(CMapArea* area, uint32_t nameId) {
    if (!area || !area->m_wmoFileNames || !area->m_wmoFilenamesOffsets) {
        return nullptr;
    }

    return area->m_wmoFileNames + area->m_wmoFilenamesOffsets[nameId];
}

// At load time two chunks with identical layer sets share one render chunk with four layer slots
// between them, so an edit that makes them diverge has to split them first.
inline void SplitRenderChunk(CMapChunk* chunk) {
    if (!chunk || !chunk->m_renderChunk) {
        return;
    }

    CMapRenderChunk* renderChunk = chunk->m_renderChunk;
    CMapChunk* a = renderChunk->m_mapChunks[0];
    CMapChunk* b = renderChunk->m_mapChunks[1];

    if (!a || !b || a == b) {
        return;
    }

    CMap::FreeRenderChunk(renderChunk);

    a->AllocRenderChunkAndBatch();
    a->m_bLoaded = 1;
    b->AllocRenderChunkAndBatch();
    b->m_bLoaded = 1;
}

// Calls fn with the owner of every link, reading the next link first since fn may unlink.
template <typename Fn>
void ForEachLinkOwner(TSList<CMapBaseObjLink, TSGetExplicitLink<CMapBaseObjLink> >& list, Fn&& fn) {
    CMapBaseObjLink* link = list.Head();

    while (link) {
        CMapBaseObjLink* next = list.Next(link);
        fn(link->m_owner);
        link = next;
    }
}

inline CMapDoodadDef* FindDoodadDef(uint32_t uniqueId) {
    HASHKEY_DWORD key = { 0 };

    return CMap::doodadDefHash.Ptr(uniqueId, key);
}

inline CMapObjDef* FindMapObjDef(uint32_t uniqueId) {
    return CMap::mapObjDefHash.Ptr(uniqueId, CMap::s_mapObjDefHashKey);
}

// Purging is enough, CMap::PreUpdateAreas re-prepares the empty slot from disk next frame.
inline void ReloadTile(int32_t tileX, int32_t tileY) {
    HoT::Streaming::LooseFiles::Invalidate();

    if (CMapArea* area = GetArea(tileX, tileY)) {
        CMap::PurgeArea(area);
    }
}

inline bool RaycastTerrain(const C3Vector& start, const C3Vector& end, C3Vector& hit, float* fraction = nullptr) {
    C3Vector rayStart = start;
    C3Vector rayEnd = end;
    float distance = 1.0f;
    CMapChunk* hitChunk = nullptr;

    bool didHit = CMap::VectorIntersectTerrain(&rayStart, &rayEnd, &distance, kIntersectTerrain, &hitChunk);

    if (fraction) {
        *fraction = distance;
    }

    if (!didHit) {
        return false;
    }

    hit.x = start.x + (end.x - start.x) * distance;
    hit.y = start.y + (end.y - start.y) * distance;
    hit.z = start.z + (end.z - start.z) * distance;

    return true;
}

}

}

#endif
