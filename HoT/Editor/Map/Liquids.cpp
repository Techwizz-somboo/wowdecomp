#include "HoT/Editor/Map/Liquids.h"
#include <cstring>
#include "HoT/Editor/Map/Adt/LiquidChunk.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/MapCoords.h"
#include "HoT/Editor/Map/TileSession.h"
#include "WoW/Source/DB/DBClient/DBClient.h"

namespace HoT::MapEditor::Liquids {

namespace {
constexpr int32_t kSide = Access::kChunksPerTileSide;

const LiquidTypeRec* Row(uint32_t id) {
    return g_liquidTypeDB.GetRecord(static_cast<int32_t>(id));
}

// The depth fade the client applies per sound bank, ocean fades over a longer distance.
float FadeFactor(uint32_t soundBank) {
    return soundBank == kBankOcean ? 0.007f : 0.0337f;
}

uint16_t FormatFor(uint32_t soundBank) {
    if (soundBank == kBankMagma || soundBank == kBankSlime) {
        return Adt::kLiquidHeightUv;
    }

    return Adt::kLiquidHeightDepth;
}

struct Terrain {
    bool m_valid = false;
    float m_height[Adt::kMcvtHeights] = {};
};

Terrain ReadTerrain(const Adt::Mcnk& mcnk) {
    Terrain out;

    const Adt::SubChunk* mcvt = mcnk.Find(Adt::kMCVT);
    if (!mcvt || mcvt->m_data.size() < sizeof(out.m_height)) {
        return out;
    }

    std::memcpy(out.m_height, mcvt->m_data.data(), sizeof(out.m_height));
    for (float& z : out.m_height) {
        z += mcnk.m_header.m_position.z;
    }

    out.m_valid = true;
    return out;
}

void RefreshDepth(Adt::LiquidLayer& layer, const Terrain& terrain, uint32_t soundBank) {
    float factor = FadeFactor(soundBank);

    for (int32_t y = 0; y < Adt::kLiquidVertSide; ++y) {
        for (int32_t x = 0; x < Adt::kLiquidVertSide; ++x) {
            int32_t v = Adt::LiquidVert(x, y);
            if (!terrain.m_valid) {
                layer.m_depth[v] = 255;
                continue;
            }

            float diff = layer.m_height[v] - terrain.m_height[Coords::EncodeOuterVertex(y, x)];
            float depth = diff < 0.0f ? 0.0f : (diff + 1.0f) * factor;
            depth = depth > 1.0f ? 1.0f : depth;
            layer.m_depth[v] = static_cast<uint8_t>(depth * 255.0f);
        }
    }
}

void RefreshUv(Adt::LiquidLayer& layer) {
    for (int32_t y = 0; y < Adt::kLiquidVertSide; ++y) {
        for (int32_t x = 0; x < Adt::kLiquidVertSide; ++x) {
            int32_t v = Adt::LiquidVert(x, y) * 2;
            layer.m_uv[v] = static_cast<uint16_t>(x * 255 / 4);
            layer.m_uv[v + 1] = static_cast<uint16_t>(y * 255 / 4);
        }
    }
}

void Finish(Adt::LiquidLayer& layer, const Adt::Mcnk& mcnk, uint32_t soundBank) {
    if (layer.m_format == Adt::kLiquidHeightUv) {
        RefreshUv(layer);
    } else {
        RefreshDepth(layer, ReadTerrain(mcnk), soundBank);
    }
}

uint32_t BankOf(uint32_t type) {
    const LiquidTypeRec* row = Row(type);
    return row ? static_cast<uint32_t>(row->m_soundBank) : kBankWater;
}

uint64_t CoveredCells(const CMapChunk* chunk, const C3Vector& center, float radius) {
    uint64_t mask = 0;
    if (!chunk) {
        return mask;
    }

    for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy) {
        for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx) {
            float worldX = chunk->m_topLeftCoords.x - (static_cast<float>(cy) + 0.5f) * Coords::kUnitSize;
            float worldY = chunk->m_topLeftCoords.y - (static_cast<float>(cx) + 0.5f) * Coords::kUnitSize;

            float dx = center.x - worldX;
            float dy = center.y - worldY;
            if (dx * dx + dy * dy <= radius * radius) {
                mask |= 1ull << Adt::LiquidCell(cx, cy);
            }
        }
    }

    return mask;
}

bool CircleTouchesTile(CMapArea* area, const C3Vector& center, float radius) {
    for (int32_t i = 0; i < Access::kChunksPerTile; ++i) {
        if (Coords::CircleTouchesChunk(area->m_mapChunks[i], center, radius)) {
            return true;
        }
    }

    return false;
}

uint64_t OccupiedCells(const Adt::ChunkLiquid& liquid) {
    uint64_t mask = 0;
    for (const Adt::LiquidLayer& layer : liquid.m_layers) {
        for (int32_t cell = 0; cell < Adt::kLiquidCells; ++cell) {
            if (layer.m_exists[cell]) {
                mask |= 1ull << cell;
            }
        }
    }

    return mask;
}

template <typename Visit>
int32_t EditTiles(const C3Vector& center, float radius, std::vector<TileRef>& touched,
    std::string& error, Visit&& visit) {
    int32_t changed = 0;

    C3Vector lo{ center.x + radius, center.y + radius, 0.0f };
    C3Vector hi{ center.x - radius, center.y - radius, 0.0f };

    for (int32_t tileY = Coords::TileY(lo); tileY <= Coords::TileY(hi); ++tileY) {
        for (int32_t tileX = Coords::TileX(lo); tileX <= Coords::TileX(hi); ++tileX) {
            CMapArea* area = Access::GetArea(tileX, tileY);
            if (!Access::IsAreaReady(area) || !CircleTouchesTile(area, center, radius)) {
                continue;
            }

            Session::OpenTile* tile = Session::Open(tileX, tileY, error);
            if (!tile) {
                continue;
            }

            Adt::TileLiquid liquid;
            if (!Adt::ReadTileLiquid(tile->m_doc, liquid, error)) {
                continue;
            }

            int32_t here = 0;

            for (int32_t chunkY = 0; chunkY < kSide; ++chunkY) {
                for (int32_t chunkX = 0; chunkX < kSide; ++chunkX) {
                    CMapChunk* chunk = Access::GetChunk(area, chunkX, chunkY);
                    if (!Coords::CircleTouchesChunk(chunk, center, radius)) {
                        continue;
                    }

                    Adt::Mcnk* mcnk = tile->m_doc.ChunkAt(chunkX, chunkY);
                    if (!mcnk) {
                        continue;
                    }

                    uint64_t mask = CoveredCells(chunk, center, radius);
                    if (!mask) {
                        continue;
                    }

                    if (visit(liquid.m_chunks[chunkY * kSide + chunkX], *mcnk, mask)) {
                        ++here;
                    }
                }
            }

            if (!here) {
                continue;
            }

            if (!Adt::WriteTileLiquid(liquid, tile->m_doc, error)) {
                continue;
            }

            tile->m_dirty = true;
            touched.push_back({ tileX, tileY });
            changed += here;
        }
    }

    return changed;
}

bool Targeted(int32_t layer, size_t index) {
    return layer < 0 || static_cast<size_t>(layer) == index;
}
}

bool GetType(uint32_t id, TypeInfo& out) {
    const LiquidTypeRec* row = Row(id);
    if (!row) {
        return false;
    }

    out.m_id = static_cast<uint32_t>(row->m_id);
    out.m_name = row->m_name ? row->m_name : "";
    out.m_soundBank = static_cast<uint32_t>(row->m_soundBank);
    out.m_materialId = static_cast<uint32_t>(row->m_materialID);
    return true;
}

void ListTypes(std::vector<TypeInfo>& out) {
    out.clear();

    if (!g_liquidTypeDB.m_loaded) {
        return;
    }

    int32_t first = g_liquidTypeDB.m_minID;
    int32_t last = g_liquidTypeDB.m_maxID;
    if (first < 0 || last < first || last - first > 100000) {
        return;
    }

    for (int32_t id = first; id <= last; ++id) {
        TypeInfo info;
        if (GetType(static_cast<uint32_t>(id), info)) {
            out.push_back(std::move(info));
        }
    }
}

int32_t Add(const C3Vector& center, float radius, uint32_t type, float height,
    std::vector<TileRef>& touched, std::string& error) {
    TypeInfo info;
    if (!GetType(type, info)) {
        error = "no LiquidType.dbc row with that id";
        return 0;
    }

    if (radius <= 0.0f) {
        error = "the brush has no radius";
        return 0;
    }

    int32_t full = 0;

    int32_t changed = EditTiles(center, radius, touched, error,
        [&](Adt::ChunkLiquid& liquid, const Adt::Mcnk& mcnk, uint64_t mask) {
            Adt::LiquidLayer* layer = nullptr;
            for (Adt::LiquidLayer& candidate : liquid.m_layers) {
                if (candidate.m_type == type) {
                    layer = &candidate;
                    break;
                }
            }

            if (!layer) {
                if (static_cast<int32_t>(liquid.m_layers.size()) >= kMaxLayers) {
                    ++full;
                    return false;
                }

                Adt::LiquidLayer fresh;
                fresh.m_type = static_cast<uint16_t>(type);
                fresh.m_format = FormatFor(info.m_soundBank);

                for (int32_t v = 0; v < Adt::kLiquidVerts; ++v) {
                    fresh.m_height[v] = height;
                }

                liquid.m_layers.push_back(fresh);
                layer = &liquid.m_layers.back();
            }

            for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy) {
                for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx) {
                    int32_t cell = Adt::LiquidCell(cx, cy);
                    if (!(mask & (1ull << cell))) {
                        continue;
                    }

                    layer->m_exists[cell] = true;
                    liquid.m_fishable |= 1ull << cell;

                    for (int32_t dy = 0; dy <= 1; ++dy) {
                        for (int32_t dx = 0; dx <= 1; ++dx) {
                            layer->m_height[Adt::LiquidVert(cx + dx, cy + dy)] = height;
                        }
                    }
                }
            }

            Finish(*layer, mcnk, info.m_soundBank);
            return true;
        });

    if (!changed && error.empty()) {
        error = full ? "every chunk under the brush is already at four liquid layers"
                     : "nothing under the brush to put liquid on";
    }

    return changed;
}

int32_t Remove(const C3Vector& center, float radius, int32_t layer,
    std::vector<TileRef>& touched, std::string& error) {
    int32_t changed = EditTiles(center, radius, touched, error,
        [&](Adt::ChunkLiquid& liquid, const Adt::Mcnk&, uint64_t mask) {
            bool any = false;

            for (size_t i = 0; i < liquid.m_layers.size(); ++i) {
                if (!Targeted(layer, i)) {
                    continue;
                }

                for (int32_t cell = 0; cell < Adt::kLiquidCells; ++cell) {
                    if (!(mask & (1ull << cell)) || !liquid.m_layers[i].m_exists[cell]) {
                        continue;
                    }

                    liquid.m_layers[i].m_exists[cell] = false;
                    any = true;
                }
            }

            if (!any) {
                return false;
            }

            uint64_t left = OccupiedCells(liquid);
            liquid.m_fishable &= left;
            liquid.m_fatigue &= left;
            return true;
        });

    if (!changed && error.empty()) {
        error = "no liquid under the brush";
    }

    return changed;
}

int32_t SetType(const C3Vector& center, float radius, int32_t layer, uint32_t type,
    std::vector<TileRef>& touched, std::string& error) {
    TypeInfo info;
    if (!GetType(type, info)) {
        error = "no LiquidType.dbc row with that id";
        return 0;
    }

    int32_t changed = EditTiles(center, radius, touched, error,
        [&](Adt::ChunkLiquid& liquid, const Adt::Mcnk& mcnk, uint64_t mask) {
            bool any = false;

            for (size_t i = 0; i < liquid.m_layers.size(); ++i) {
                if (!Targeted(layer, i) || liquid.m_layers[i].m_type == type) {
                    continue;
                }

                bool touching = false;
                for (int32_t cell = 0; cell < Adt::kLiquidCells && !touching; ++cell) {
                    touching = (mask & (1ull << cell)) && liquid.m_layers[i].m_exists[cell];
                }

                if (!touching) {
                    continue;
                }

                liquid.m_layers[i].m_type = static_cast<uint16_t>(type);
                liquid.m_layers[i].m_format = FormatFor(info.m_soundBank);
                Finish(liquid.m_layers[i], mcnk, info.m_soundBank);
                any = true;
            }

            return any;
        });

    if (!changed && error.empty()) {
        error = "no liquid under the brush was a different type";
    }

    return changed;
}

int32_t SetHeight(const C3Vector& center, float radius, int32_t layer, float height,
    bool relative, std::vector<TileRef>& touched, std::string& error) {
    int32_t changed = EditTiles(center, radius, touched, error,
        [&](Adt::ChunkLiquid& liquid, const Adt::Mcnk& mcnk, uint64_t mask) {
            bool any = false;

            for (size_t i = 0; i < liquid.m_layers.size(); ++i) {
                if (!Targeted(layer, i)) {
                    continue;
                }

                Adt::LiquidLayer& target = liquid.m_layers[i];

                bool moved[Adt::kLiquidVerts] = {};
                for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy) {
                    for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx) {
                        int32_t cell = Adt::LiquidCell(cx, cy);
                        if (!(mask & (1ull << cell)) || !target.m_exists[cell]) {
                            continue;
                        }

                        for (int32_t dy = 0; dy <= 1; ++dy) {
                            for (int32_t dx = 0; dx <= 1; ++dx) {
                                moved[Adt::LiquidVert(cx + dx, cy + dy)] = true;
                            }
                        }
                    }
                }

                bool here = false;
                for (int32_t v = 0; v < Adt::kLiquidVerts; ++v) {
                    if (!moved[v]) {
                        continue;
                    }

                    target.m_height[v] = relative ? target.m_height[v] + height : height;
                    here = true;
                }

                if (!here) {
                    continue;
                }

                Finish(target, mcnk, BankOf(target.m_type));
                any = true;
            }

            return any;
        });

    if (!changed && error.empty()) {
        error = "no liquid under the brush";
    }

    return changed;
}

int32_t SetFlags(const C3Vector& center, float radius, int32_t fishable, int32_t fatigue,
    std::vector<TileRef>& touched, std::string& error) {
    if (fishable < 0 && fatigue < 0) {
        error = "nothing to change";
        return 0;
    }

    int32_t changed = EditTiles(center, radius, touched, error,
        [&](Adt::ChunkLiquid& liquid, const Adt::Mcnk&, uint64_t mask) {
            uint64_t live = mask & OccupiedCells(liquid);
            if (!live) {
                return false;
            }

            uint64_t wasFishable = liquid.m_fishable;
            uint64_t wasFatigue = liquid.m_fatigue;

            if (fishable == 0) {
                liquid.m_fishable &= ~live;
            } else if (fishable > 0) {
                liquid.m_fishable |= live;
            }

            if (fatigue == 0) {
                liquid.m_fatigue &= ~live;
            } else if (fatigue > 0) {
                liquid.m_fatigue |= live;
            }

            return liquid.m_fishable != wasFishable || liquid.m_fatigue != wasFatigue;
        });

    if (!changed && error.empty()) {
        error = "no liquid under the brush, or it was already set that way";
    }

    return changed;
}

bool Describe(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, ChunkInfo& out,
    std::string& error) {
    out = ChunkInfo();

    if (chunkX < 0 || chunkX >= kSide || chunkY < 0 || chunkY >= kSide) {
        error = "chunk is off the tile";
        return false;
    }

    Session::OpenTile* tile = Session::Open(tileX, tileY, error);
    if (!tile) {
        return false;
    }

    Adt::TileLiquid liquid;
    if (!Adt::ReadTileLiquid(tile->m_doc, liquid, error)) {
        return false;
    }

    const Adt::ChunkLiquid& chunk = liquid.m_chunks[chunkY * kSide + chunkX];
    out.m_fishable = chunk.m_fishable;
    out.m_fatigue = chunk.m_fatigue;

    for (const Adt::LiquidLayer& layer : chunk.m_layers) {
        LayerInfo info;
        info.m_type = layer.m_type;
        info.m_format = layer.m_format;
        info.m_cells = layer.CellCount();

        bool first = true;
        for (int32_t cy = 0; cy < Adt::kLiquidCellSide; ++cy) {
            for (int32_t cx = 0; cx < Adt::kLiquidCellSide; ++cx) {
                if (!layer.m_exists[Adt::LiquidCell(cx, cy)]) {
                    continue;
                }

                for (int32_t dy = 0; dy <= 1; ++dy) {
                    for (int32_t dx = 0; dx <= 1; ++dx) {
                        float z = layer.m_height[Adt::LiquidVert(cx + dx, cy + dy)];
                        if (first) {
                            info.m_minHeight = z;
                            info.m_maxHeight = z;
                            first = false;
                        }

                        info.m_minHeight = z < info.m_minHeight ? z : info.m_minHeight;
                        info.m_maxHeight = z > info.m_maxHeight ? z : info.m_maxHeight;
                    }
                }
            }
        }

        out.m_layers.push_back(std::move(info));
    }

    return true;
}

}
