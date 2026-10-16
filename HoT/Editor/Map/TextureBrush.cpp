#include "HoT/Editor/Map/TextureBrush.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "HoT/Editor/Map/Adt/AlphaMap.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/MapCoords.h"
#include "HoT/Editor/Map/TileSession.h"

namespace HoT::MapEditor::TextureBrush {

namespace {
constexpr int32_t kSide = Access::kChunksPerTileSide;
constexpr int32_t kTexelSide = Coords::kAlphaSide;
constexpr int32_t kTexels = kTexelSide * kTexelSide;
constexpr int32_t kMaxLayers = 4;

float Weight(float distance, float radius, Sculpt::Falloff falloff) {
    if (radius <= 0.0f || distance >= radius) {
        return 0.0f;
    }

    float t = 1.0f - distance / radius;
    switch (falloff) {
        case Sculpt::Falloff::Flat:
            return 1.0f;
        case Sculpt::Falloff::Linear:
            return t;
        case Sculpt::Falloff::Smooth:
        default:
            return t * t * (3.0f - 2.0f * t);
    }
}

struct ChunkKey {
    int32_t m_tileX = 0;
    int32_t m_tileY = 0;
    int32_t m_chunkX = 0;
    int32_t m_chunkY = 0;

    bool operator<(const ChunkKey& other) const {
        if (this->m_tileX != other.m_tileX) {
            return this->m_tileX < other.m_tileX;
        }

        if (this->m_tileY != other.m_tileY) {
            return this->m_tileY < other.m_tileY;
        }

        if (this->m_chunkX != other.m_chunkX) {
            return this->m_chunkX < other.m_chunkX;
        }

        return this->m_chunkY < other.m_chunkY;
    }
};

// Coverage is kept in 8.8 fixed point so slow strokes still accumulate between whole alpha steps.
struct PaintedChunk {
    Adt::ChunkAlpha m_alpha;
    std::vector<uint16_t> m_coverage;
    std::vector<uint8_t> m_mcly;
    std::vector<uint8_t> m_mcal;
};

constexpr float kFixedOne = 256.0f;
constexpr float kFullCoverage = 255.0f;

uint16_t ToFixed(float value) {
    float scaled = value * kFixedOne + 0.5f;
    if (scaled <= 0.0f) {
        return 0;
    }
    if (scaled >= kFullCoverage * kFixedOne) {
        return static_cast<uint16_t>(kFullCoverage * kFixedOne);
    }

    return static_cast<uint16_t>(scaled);
}

std::map<ChunkKey, PaintedChunk>& Painted() {
    static std::map<ChunkKey, PaintedChunk> painted;
    return painted;
}

bool LayerSetChanged(CMapChunk* chunk, const Adt::Mcnk& mcnk, const SMLayer* fresh) {
    if (chunk->m_header->m_nLayers != mcnk.m_header.m_nLayers) {
        return true;
    }
    if (!chunk->m_layers) {
        return mcnk.m_header.m_nLayers != 0;
    }

    for (uint32_t i = 0; i < mcnk.m_header.m_nLayers; ++i) {
        if (chunk->m_layers[i].m_textureId != fresh[i].m_textureId || chunk->m_layers[i].m_flags != fresh[i].m_flags) {
            return true;
        }
    }
    return false;
}

// The live chunk reads MCLY and MCAL out of the file image, which cannot grow, so it is pointed at our copies.
void PushLive(PaintedChunk& state, CMapChunk* chunk, const Adt::Mcnk& mcnk) {
    if (!chunk || !chunk->m_header) {
        return;
    }

    const Adt::SubChunk* mcly = mcnk.Find(Adt::kMCLY);
    const Adt::SubChunk* mcal = mcnk.Find(Adt::kMCAL);
    if (!mcly || !mcal) {
        return;
    }

    size_t layerBytes = mcnk.m_header.m_nLayers * sizeof(SMLayer);
    if (!layerBytes || mcly->m_data.size() < layerBytes) {
        return;
    }

    bool relayered = LayerSetChanged(chunk, mcnk, reinterpret_cast<const SMLayer*>(mcly->m_data.data()));

    state.m_mcly.assign(mcly->m_data.begin(), mcly->m_data.begin() + layerBytes);
    chunk->m_layers = reinterpret_cast<SMLayer*>(state.m_mcly.data());

    state.m_mcal = mcal->m_data;
    chunk->m_additionalShadowmap = state.m_mcal.empty() ? nullptr : state.m_mcal.data();

    chunk->m_header->m_nLayers = mcnk.m_header.m_nLayers;

    if (relayered) {
        Access::SplitRenderChunk(chunk);
    }

    if (chunk->m_renderChunk) {
        chunk->m_renderChunk->FreeLayers();
    }
}

// Moves one layer toward target and takes the difference from the others in proportion, keeping the sum at 255.
bool PaintTexel(float* coverage, int32_t count, int32_t layer, float target, float t) {
    float total = 0.0f;
    for (int32_t i = 0; i < count; ++i) {
        total += coverage[i];
    }

    float change = (target - coverage[layer]) * t;
    if (change == 0.0f) {
        return false;
    }

    float others = total - coverage[layer];

    if (others > 0.001f) {
        for (int32_t i = 0; i < count; ++i) {
            if (i != layer) {
                coverage[i] -= change * coverage[i] / others;
            }
        }
    } else if (change > 0.0f) {
        for (int32_t i = 0; i < count; ++i) {
            if (i != layer) {
                coverage[i] = 0.0f;
            }
        }
    } else {
        int32_t sink = layer == 0 ? 1 : 0;
        if (sink >= count) {
            return false;
        }

        coverage[sink] -= change;
    }

    coverage[layer] += change;

    for (int32_t i = 0; i < count; ++i) {
        coverage[i] = coverage[i] < 0.0f ? 0.0f : (coverage[i] > 255.0f ? 255.0f : coverage[i]);
    }

    float after = 0.0f;
    for (int32_t i = 0; i < count; ++i) {
        after += coverage[i];
    }

    if (after <= 0.001f) {
        for (int32_t i = 0; i < count; ++i) {
            coverage[i] = 0.0f;
        }

        coverage[0] = 255.0f;
    } else if (std::fabs(after - 255.0f) > 0.001f) {
        for (int32_t i = 0; i < count; ++i) {
            coverage[i] *= 255.0f / after;
        }
    }

    return true;
}

int32_t PaintChunk(const Stroke& stroke, const ChunkKey& key, CMapChunk* chunk,
    Session::OpenTile& tile) {
    Adt::Mcnk* mcnk = tile.m_doc.ChunkAt(key.m_chunkX, key.m_chunkY);
    if (!mcnk) {
        return 0;
    }

    Adt::AlphaFormat format = Adt::FormatFor(*mcnk, Access::MapFlags());

    PaintedChunk& state = Painted()[key];
    Adt::ChunkAlpha& alpha = state.m_alpha;

    if (alpha.m_layers.size() != mcnk->m_header.m_nLayers) {
        std::string error;
        if (!Adt::ReadChunkAlpha(*mcnk, format, alpha, error)) {
            Painted().erase(key);
            return 0;
        }

        state.m_coverage.assign(alpha.m_layers.size() * kTexels, 0);
        for (size_t i = 0; i < alpha.m_layers.size(); ++i) {
            for (int32_t t = 0; t < kTexels; ++t) {
                state.m_coverage[i * kTexels + t] = ToFixed(alpha.m_maps[i].m_texel[t]);
            }
        }
    }

    int32_t count = static_cast<int32_t>(alpha.m_layers.size());
    if (count < 2 || stroke.m_layer < 0 || stroke.m_layer >= count || count > kMaxLayers) {
        return 0;
    }

    float target = stroke.m_blend >= 0.0f ? kFullCoverage : 0.0f;
    float rate = std::fabs(stroke.m_blend);
    int32_t changed = 0;

    for (int32_t row = 0; row < kTexelSide; ++row) {
        for (int32_t col = 0; col < kTexelSide; ++col) {
            C3Vector world = Coords::TexelWorldPos(chunk, row, col);
            float weight = Weight(Coords::Distance2D(world, stroke.m_center), stroke.m_radius,
                stroke.m_falloff);

            if (weight <= 0.0f) {
                continue;
            }

            int32_t texel = Coords::EncodeTexel(row, col);

            float coverage[kMaxLayers] = {};
            float base = kFullCoverage;
            for (int32_t i = 1; i < count; ++i) {
                coverage[i] = state.m_coverage[i * kTexels + texel] / kFixedOne;
                base -= coverage[i];
            }

            coverage[0] = base < 0.0f ? 0.0f : base;

            if (!PaintTexel(coverage, count, stroke.m_layer, target, weight * rate)) {
                continue;
            }

            for (int32_t i = 1; i < count; ++i) {
                state.m_coverage[i * kTexels + texel] = ToFixed(coverage[i]);

                uint8_t value = static_cast<uint8_t>(coverage[i] + 0.5f);
                if (alpha.m_maps[i].m_texel[texel] == value) {
                    continue;
                }

                alpha.m_maps[i].m_texel[texel] = value;
                ++changed;
            }
        }
    }

    if (!changed) {
        return 0;
    }

    std::string error;
    if (!Adt::WriteChunkAlpha(alpha, format, *mcnk, error)) {
        return 0;
    }

    tile.m_dirty = true;
    PushLive(state, chunk, *mcnk);
    return changed;
}
}

void PushChunk(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, CMapChunk* chunk,
    const Adt::Mcnk& mcnk) {
    ChunkKey key{ tileX, tileY, chunkX, chunkY };
    PaintedChunk& state = Painted()[key];

    state.m_alpha = Adt::ChunkAlpha{};
    state.m_coverage.clear();

    PushLive(state, chunk, mcnk);
}

void Reset() {
    Painted().clear();
}

int32_t Apply(const Stroke& stroke) {
    if (stroke.m_radius <= 0.0f || stroke.m_blend == 0.0f) {
        return 0;
    }

    C3Vector lo{ stroke.m_center.x - stroke.m_radius, stroke.m_center.y - stroke.m_radius, 0.0f };
    C3Vector hi{ stroke.m_center.x + stroke.m_radius, stroke.m_center.y + stroke.m_radius, 0.0f };

    int32_t tileX0 = std::min(Coords::TileX(lo), Coords::TileX(hi));
    int32_t tileX1 = std::max(Coords::TileX(lo), Coords::TileX(hi));
    int32_t tileY0 = std::min(Coords::TileY(lo), Coords::TileY(hi));
    int32_t tileY1 = std::max(Coords::TileY(lo), Coords::TileY(hi));

    int32_t changed = 0;

    for (int32_t tileY = tileY0; tileY <= tileY1; ++tileY) {
        for (int32_t tileX = tileX0; tileX <= tileX1; ++tileX) {
            CMapArea* area = Access::GetArea(tileX, tileY);
            if (!Access::IsAreaReady(area)) {
                continue;
            }

            Session::OpenTile* tile = nullptr;

            for (int32_t chunkY = 0; chunkY < kSide; ++chunkY) {
                for (int32_t chunkX = 0; chunkX < kSide; ++chunkX) {
                    CMapChunk* chunk = Access::GetChunk(area, chunkX, chunkY);
                    if (!Coords::CircleTouchesChunk(chunk, stroke.m_center, stroke.m_radius)) {
                        continue;
                    }

                    if (!tile) {
                        std::string error;
                        tile = Session::Open(tileX, tileY, error);
                        if (!tile) {
                            break;
                        }
                    }

                    ChunkKey key{ tileX, tileY, chunkX, chunkY };
                    changed += PaintChunk(stroke, key, chunk, *tile);
                }
            }
        }
    }

    return changed;
}

}
