#include "HoT/Editor/Map/TextureLayers.h"
#include <cstring>
#include <deque>
#include <map>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Editor/Map/Adt/AlphaMap.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/MapCoords.h"
#include "HoT/Editor/Map/TextureBrush.h"
#include "HoT/Editor/Map/TileSession.h"

namespace HoT::MapEditor::TextureLayers {

namespace {
constexpr int32_t kSide = Access::kChunksPerTileSide;

constexpr int32_t kTextureNoSpecular = 0x1;

// The tile texture list only borrows names, so they live here until the map changes.
std::deque<std::string>& OwnedNames() {
    static std::deque<std::string> names;
    return names;
}

std::map<std::pair<int32_t, int32_t>, std::vector<int32_t>>& OwnedTextureFlags() {
    static std::map<std::pair<int32_t, int32_t>, std::vector<int32_t>> flags;
    return flags;
}

char Normalize(char c) {
    if (c == '/') {
        return '\\';
    }

    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string ToGamePath(const char* text) {
    std::string out = text;
    for (char& c : out) {
        if (c == '/') {
            c = '\\';
        }
    }

    return out;
}

bool LooksMangled(const std::string& path) {
    for (char c : path) {
        if (static_cast<unsigned char>(c) < 0x20) {
            return true;
        }
    }

    return false;
}

bool SamePath(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }

    for (size_t i = 0; i < a.size(); ++i) {
        if (Normalize(a[i]) != Normalize(b[i])) {
            return false;
        }
    }

    return true;
}

std::vector<std::string> SplitMtex(const std::vector<uint8_t>& blob) {
    std::vector<std::string> out;

    size_t at = 0;
    while (at < blob.size()) {
        size_t length = 0;
        while (at + length < blob.size() && blob[at + length]) {
            ++length;
        }

        out.emplace_back(reinterpret_cast<const char*>(blob.data()) + at, length);
        at += length + 1;
    }

    return out;
}

void JoinMtex(const std::vector<std::string>& names, std::vector<uint8_t>& out) {
    out.clear();
    for (const std::string& name : names) {
        out.insert(out.end(), name.begin(), name.end());
        out.push_back(0);
    }
}

Adt::TopChunk* FindTop(Adt::AdtDocument& doc, uint32_t id) {
    for (Adt::TopChunk& top : doc.m_order) {
        if (top.m_id == id) {
            return &top;
        }
    }

    return nullptr;
}

Adt::TopChunk* EnsureTop(Adt::AdtDocument& doc, uint32_t id, uint32_t after) {
    if (Adt::TopChunk* existing = FindTop(doc, id)) {
        return existing;
    }

    size_t at = doc.m_order.size();
    for (size_t i = 0; i < doc.m_order.size(); ++i) {
        if (doc.m_order[i].m_id == after) {
            at = i + 1;
            break;
        }

        if (doc.m_order[i].m_mcnkIndex >= 0) {
            at = i;
            break;
        }
    }

    Adt::TopChunk chunk;
    chunk.m_id = id;
    return &*doc.m_order.insert(doc.m_order.begin() + static_cast<ptrdiff_t>(at), chunk);
}

std::string SpecularPath(const std::string& path) {
    size_t dot = path.find_last_of('.');
    return (dot == std::string::npos ? path : path.substr(0, dot)) + "_s.blp";
}

bool FileExists(const std::string& path) {
    SFileImpl* handle = nullptr;

    if (!SFile::OpenEx(nullptr, path.c_str(), 0, &handle) || !handle) {
        return false;
    }

    SFileCloseFile(handle);

    return true;
}

bool EnsureTexture(Session::OpenTile& tile, CMapArea* area, const char* texture,
    uint32_t& textureId, std::string& error) {
    Adt::TopChunk* mtex = FindTop(tile.m_doc, Adt::kMTEX);
    if (!mtex) {
        error = "tile has no MTEX";
        return false;
    }

    std::vector<std::string> names = SplitMtex(mtex->m_data);

    if (names.size() != Access::TextureCount(area)) {
        error = "tile texture list does not match MTEX";
        return false;
    }

    std::string wanted = ToGamePath(texture);
    for (size_t i = 0; i < names.size(); ++i) {
        if (SamePath(names[i], wanted)) {
            textureId = static_cast<uint32_t>(i);
            return true;
        }
    }

    OwnedNames().push_back(wanted);
    const char* stable = OwnedNames().back().c_str();

    uint32_t added = 0;
    if (!Access::AppendTerrainTexture(area, stable, added)) {
        OwnedNames().pop_back();
        error = "could not grow the tile texture list";
        return false;
    }

    names.push_back(wanted);
    JoinMtex(names, mtex->m_data);

    bool noSpecular = !FileExists(SpecularPath(wanted));

    Adt::TopChunk* mtxf = FindTop(tile.m_doc, Adt::kMTXF);
    if (!mtxf && noSpecular) {
        mtxf = EnsureTop(tile.m_doc, Adt::kMTXF, Adt::kMH2O);
    }

    if (mtxf) {
        mtxf->m_data.resize(names.size() * sizeof(int32_t), 0);

        if (noSpecular) {
            int32_t flags = kTextureNoSpecular;
            std::memcpy(mtxf->m_data.data() + added * sizeof(int32_t), &flags, sizeof(flags));
        }

        std::vector<int32_t>& owned = OwnedTextureFlags()[{ tile.m_tileX, tile.m_tileY }];
        owned.assign(names.size(), 0);
        std::memcpy(owned.data(), mtxf->m_data.data(), mtxf->m_data.size());
        // The live MTXF pointer is into the file image, which has no room for the new entry.
        area->m_textureFlags = owned.data();
    }

    tile.m_dirty = true;
    textureId = added;
    return true;
}

template <typename Edit>
bool EditChunk(Session::OpenTile& tile, CMapChunk* chunk, int32_t chunkX, int32_t chunkY,
    Edit&& edit) {
    Adt::Mcnk* mcnk = tile.m_doc.ChunkAt(chunkX, chunkY);
    if (!mcnk) {
        return false;
    }

    Adt::AlphaFormat format = Adt::FormatFor(*mcnk, Access::MapFlags());

    Adt::ChunkAlpha alpha;
    std::string error;
    if (!Adt::ReadChunkAlpha(*mcnk, format, alpha, error)) {
        return false;
    }

    if (!edit(alpha)) {
        return false;
    }

    if (!Adt::WriteChunkAlpha(alpha, format, *mcnk, error)) {
        return false;
    }

    tile.m_dirty = true;
    TextureBrush::PushChunk(tile.m_tileX, tile.m_tileY, chunkX, chunkY, chunk, *mcnk);
    return true;
}

bool CircleTouchesTile(CMapArea* area, const C3Vector& center, float radius) {
    for (int32_t i = 0; i < Access::kChunksPerTile; ++i) {
        if (Coords::CircleTouchesChunk(area->m_mapChunks[i], center, radius)) {
            return true;
        }
    }

    return false;
}

template <typename Visit>
void ForEachChunk(const C3Vector& center, float radius, Visit&& visit) {
    C3Vector lo{ center.x + radius, center.y + radius, 0.0f };
    C3Vector hi{ center.x - radius, center.y - radius, 0.0f };

    for (int32_t tileY = Coords::TileY(lo); tileY <= Coords::TileY(hi); ++tileY) {
        for (int32_t tileX = Coords::TileX(lo); tileX <= Coords::TileX(hi); ++tileX) {
            CMapArea* area = Access::GetArea(tileX, tileY);
            if (!Access::IsAreaReady(area) || !CircleTouchesTile(area, center, radius)) {
                continue;
            }

            std::string error;
            Session::OpenTile* tile = Session::Open(tileX, tileY, error);
            if (!tile) {
                continue;
            }

            for (int32_t chunkY = 0; chunkY < kSide; ++chunkY) {
                for (int32_t chunkX = 0; chunkX < kSide; ++chunkX) {
                    CMapChunk* chunk = Access::GetChunk(area, chunkX, chunkY);
                    if (!Coords::CircleTouchesChunk(chunk, center, radius)) {
                        continue;
                    }

                    visit(area, *tile, chunk, chunkX, chunkY);
                }
            }
        }
    }
}
}

void Reset() {
    OwnedNames().clear();
    OwnedTextureFlags().clear();
}

bool ListTileTextures(int32_t tileX, int32_t tileY, std::vector<std::string>& out,
    std::string& error) {
    out.clear();

    Session::OpenTile* tile = Session::Open(tileX, tileY, error);
    if (!tile) {
        return false;
    }

    Adt::TopChunk* mtex = FindTop(tile->m_doc, Adt::kMTEX);
    if (!mtex) {
        error = "tile has no MTEX";
        return false;
    }

    out = SplitMtex(mtex->m_data);
    return true;
}

int32_t Add(const C3Vector& center, float radius, const char* texture, int32_t& skipped,
    std::string& error) {
    skipped = 0;

    if (!texture || !*texture || radius <= 0.0f) {
        error = "no texture given";
        return 0;
    }

    if (LooksMangled(texture)) {
        error = "that path has a control character in it, so Lua ate the backslashes - "
                "double them or use forward slashes";
        return 0;
    }

    int32_t added = 0;

    ForEachChunk(center, radius,
        [&](CMapArea* area, Session::OpenTile& tile, CMapChunk* chunk, int32_t chunkX,
            int32_t chunkY) {
            uint32_t textureId = 0;
            if (!EnsureTexture(tile, area, texture, textureId, error)) {
                ++skipped;
                return;
            }

            bool changed = EditChunk(tile, chunk, chunkX, chunkY,
                [&](Adt::ChunkAlpha& alpha) {
                    if (alpha.m_layers.size() >= kMaxLayers) {
                        return false;
                    }

                    for (const SMLayer& layer : alpha.m_layers) {
                        if (layer.m_textureId == textureId) {
                            return false;
                        }
                    }

                    SMLayer fresh{};
                    fresh.m_textureId = textureId;
                    fresh.m_flags = Adt::kLayerUseAlpha;
                    fresh.m_offectId = 0;
                    alpha.m_layers.push_back(fresh);

                    // A new layer starts fully transparent so nothing changes until it is painted.
                    alpha.m_maps.emplace_back();
                    return true;
                });

            if (changed) {
                ++added;
            } else {
                ++skipped;
            }
        });

    if (!added && error.empty()) {
        error = "every chunk already had it, or was already at four layers";
    }

    return added;
}

int32_t Remove(const C3Vector& center, float radius, int32_t layer, int32_t& skipped,
    std::string& error) {
    skipped = 0;

    if (layer <= 0 || layer >= kMaxLayers) {
        error = "layer 0 is the base and cannot be removed";
        return 0;
    }

    int32_t removed = 0;

    ForEachChunk(center, radius,
        [&](CMapArea*, Session::OpenTile& tile, CMapChunk* chunk, int32_t chunkX,
            int32_t chunkY) {
            bool changed = EditChunk(tile, chunk, chunkX, chunkY,
                [&](Adt::ChunkAlpha& alpha) {
                    if (layer >= static_cast<int32_t>(alpha.m_layers.size())) {
                        return false;
                    }

                    alpha.m_layers.erase(alpha.m_layers.begin() + layer);
                    alpha.m_maps.erase(alpha.m_maps.begin() + layer);
                    return true;
                });

            if (changed) {
                ++removed;
            } else {
                ++skipped;
            }
        });

    if (!removed && error.empty()) {
        error = "no chunk under the brush had that layer";
    }

    return removed;
}

}
