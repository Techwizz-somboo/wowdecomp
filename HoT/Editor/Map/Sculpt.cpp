#include "HoT/Editor/Map/Sculpt.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/MapCoords.h"
#include "HoT/Editor/Map/TileSession.h"

namespace HoT::MapEditor::Sculpt {

namespace {
constexpr int32_t kVerts = Access::kVertsPerChunk;
constexpr int32_t kSide = Access::kChunksPerTileSide;

float Weight(float distance, float radius, Falloff falloff) {
    if (radius <= 0.0f || distance >= radius) {
        return 0.0f;
    }

    float t = 1.0f - distance / radius;
    switch (falloff) {
        case Falloff::Flat:
            return 1.0f;
        case Falloff::Linear:
            return t;
        case Falloff::Smooth:
        default:
            return t * t * (3.0f - 2.0f * t);
    }
}

struct ChunkRef {
    int32_t m_tileX = 0;
    int32_t m_tileY = 0;
    int32_t m_chunkX = 0;
    int32_t m_chunkY = 0;

    bool operator==(const ChunkRef& other) const {
        return this->m_tileX == other.m_tileX && this->m_tileY == other.m_tileY && this->m_chunkX == other.m_chunkX && this->m_chunkY == other.m_chunkY;
    }
};

CMapChunk* Resolve(const ChunkRef& ref) {
    CMapArea* area = Access::GetArea(ref.m_tileX, ref.m_tileY);
    if (!Access::IsAreaReady(area)) {
        return nullptr;
    }

    return Access::GetChunk(area, ref.m_chunkX, ref.m_chunkY);
}

ChunkRef Step(ChunkRef ref, int32_t dChunkX, int32_t dChunkY) {
    ref.m_chunkX += dChunkX;
    ref.m_chunkY += dChunkY;

    if (ref.m_chunkX < 0) {
        ref.m_chunkX += kSide;
        --ref.m_tileX;
    } else if (ref.m_chunkX >= kSide) {
        ref.m_chunkX -= kSide;
        ++ref.m_tileX;
    }

    if (ref.m_chunkY < 0) {
        ref.m_chunkY += kSide;
        --ref.m_tileY;
    } else if (ref.m_chunkY >= kSide) {
        ref.m_chunkY -= kSide;
        ++ref.m_tileY;
    }

    return ref;
}

struct Neighborhood {
    CMapChunk* m_chunk[3][3] = {};
};

Neighborhood Gather(const ChunkRef& center) {
    Neighborhood out;

    for (int32_t dy = -1; dy <= 1; ++dy) {
        for (int32_t dx = -1; dx <= 1; ++dx) {
            out.m_chunk[dy + 1][dx + 1] = Resolve(Step(center, dx, dy));
        }
    }

    return out;
}

bool OuterAt(const Neighborhood& hood, int32_t row, int32_t col, C3Vector& out) {
    int32_t dy = 0;
    int32_t dx = 0;

    if (row < 0) {
        dy = -1;
        row += 8;
    } else if (row > 8) {
        dy = 1;
        row -= 8;
    }

    if (col < 0) {
        dx = -1;
        col += 8;
    } else if (col > 8) {
        dx = 1;
        col -= 8;
    }

    CMapChunk* chunk = hood.m_chunk[dy + 1][dx + 1];
    if (!chunk || !chunk->m_height) {
        return false;
    }

    out = Coords::VertexWorldPos(chunk, Coords::EncodeOuterVertex(row, col));
    return true;
}

bool InnerAt(const Neighborhood& hood, int32_t row, int32_t col, C3Vector& out) {
    int32_t dy = 0;
    int32_t dx = 0;

    if (row < 0) {
        dy = -1;
        row += 8;
    } else if (row > 7) {
        dy = 1;
        row -= 8;
    }

    if (col < 0) {
        dx = -1;
        col += 8;
    } else if (col > 7) {
        dx = 1;
        col -= 8;
    }

    CMapChunk* chunk = hood.m_chunk[dy + 1][dx + 1];
    if (!chunk || !chunk->m_height) {
        return false;
    }

    out = Coords::VertexWorldPos(chunk, Coords::EncodeInnerVertex(row, col));
    return true;
}

int32_t OuterIndex(int32_t row, int32_t col) {
    if (row < 0 || row > 8 || col < 0 || col > 8) {
        return -1;
    }

    return Coords::EncodeOuterVertex(row, col);
}

int32_t InnerIndex(int32_t row, int32_t col) {
    if (row < 0 || row > 7 || col < 0 || col > 7) {
        return -1;
    }

    return Coords::EncodeInnerVertex(row, col);
}

void RecomputeNormals(const Neighborhood& hood, int8_t* out) {
    C3Vector sum[kVerts] = {};

    auto addFace = [&](const C3Vector& a, const C3Vector& b, const C3Vector& c, int32_t ia,
                       int32_t ib, int32_t ic) {
        C3Vector u{ b.x - a.x, b.y - a.y, b.z - a.z };
        C3Vector v{ c.x - a.x, c.y - a.y, c.z - a.z };
        C3Vector face{ u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };

        if (face.z < 0.0f) {
            face.x = -face.x;
            face.y = -face.y;
            face.z = -face.z;
        }

        for (int32_t index : { ia, ib, ic }) {
            if (index < 0) {
                continue;
            }

            sum[index].x += face.x;
            sum[index].y += face.y;
            sum[index].z += face.z;
        }
    };

    for (int32_t row = -1; row <= 8; ++row) {
        for (int32_t col = -1; col <= 8; ++col) {
            C3Vector inner{};
            C3Vector tl{};
            C3Vector tr{};
            C3Vector br{};
            C3Vector bl{};

            if (!InnerAt(hood, row, col, inner) || !OuterAt(hood, row, col, tl) || !OuterAt(hood, row, col + 1, tr) || !OuterAt(hood, row + 1, col + 1, br) || !OuterAt(hood, row + 1, col, bl)) {
                continue;
            }

            int32_t ii = InnerIndex(row, col);
            int32_t itl = OuterIndex(row, col);
            int32_t itr = OuterIndex(row, col + 1);
            int32_t ibr = OuterIndex(row + 1, col + 1);
            int32_t ibl = OuterIndex(row + 1, col);

            addFace(inner, tl, tr, ii, itl, itr);
            addFace(inner, tr, br, ii, itr, ibr);
            addFace(inner, br, bl, ii, ibr, ibl);
            addFace(inner, bl, tl, ii, ibl, itl);
        }
    }

    for (int32_t i = 0; i < kVerts; ++i) {
        C3Vector n = sum[i];
        float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (length <= 0.0f) {
            n = C3Vector{ 0.0f, 0.0f, 1.0f };
            length = 1.0f;
        }

        auto encode = [](float value) {
            float scaled = std::floor(value * 127.0f + 0.5f);
            return static_cast<int8_t>(std::min(std::max(scaled, -127.0f), 127.0f));
        };

        out[i * 3 + 0] = encode(n.x / length);
        out[i * 3 + 1] = encode(n.y / length);
        out[i * 3 + 2] = encode(n.z / length);
    }
}

bool NeighborAverage(const Neighborhood& hood, int32_t index, float& out) {
    Coords::VertexCoord coord = Coords::DecodeVertex(index);

    int32_t rows[4];
    int32_t cols[4];

    if (coord.m_inner) {
        rows[0] = coord.m_row;
        cols[0] = coord.m_col;
        rows[1] = coord.m_row;
        cols[1] = coord.m_col + 1;
        rows[2] = coord.m_row + 1;
        cols[2] = coord.m_col;
        rows[3] = coord.m_row + 1;
        cols[3] = coord.m_col + 1;
    } else {
        rows[0] = coord.m_row - 1;
        cols[0] = coord.m_col - 1;
        rows[1] = coord.m_row - 1;
        cols[1] = coord.m_col;
        rows[2] = coord.m_row;
        cols[2] = coord.m_col - 1;
        rows[3] = coord.m_row;
        cols[3] = coord.m_col;
    }

    float total = 0.0f;
    int32_t count = 0;

    for (int32_t i = 0; i < 4; ++i) {
        C3Vector at{};
        bool ok = coord.m_inner ? OuterAt(hood, rows[i], cols[i], at)
                              : InnerAt(hood, rows[i], cols[i], at);
        if (!ok) {
            continue;
        }

        total += at.z;
        ++count;
    }

    if (!count) {
        return false;
    }

    out = total / static_cast<float>(count);
    return true;
}

float Jitter(const ChunkRef& ref, int32_t index) {
    Coords::VertexCoord coord = Coords::DecodeVertex(index);

    int32_t row = ((ref.m_tileY * kSide + ref.m_chunkY) * 8 + coord.m_row) * 2 + (coord.m_inner ? 1 : 0);
    int32_t col = ((ref.m_tileX * kSide + ref.m_chunkX) * 8 + coord.m_col) * 2 + (coord.m_inner ? 1 : 0);

    uint32_t h = static_cast<uint32_t>(row) * 73856093u ^ static_cast<uint32_t>(col) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;

    return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF) * 2.0f - 1.0f;
}

struct Target {
    ChunkRef m_ref;
    CMapChunk* m_chunk = nullptr;
    float m_heights[kVerts] = {};
    bool m_moved = false;
};

void MirrorHeights(Session::OpenTile& tile, const ChunkRef& ref, const float* heights) {
    Adt::Mcnk* mcnk = tile.m_doc.ChunkAt(ref.m_chunkX, ref.m_chunkY);
    if (!mcnk) {
        return;
    }

    Adt::SubChunk* mcvt = mcnk->Find(Adt::kMCVT);
    if (!mcvt || mcvt->m_data.size() < kVerts * sizeof(float)) {
        return;
    }

    std::memcpy(mcvt->m_data.data(), heights, kVerts * sizeof(float));
    tile.m_dirty = true;
}

void MirrorNormals(Session::OpenTile& tile, const ChunkRef& ref, const int8_t* normals) {
    Adt::Mcnk* mcnk = tile.m_doc.ChunkAt(ref.m_chunkX, ref.m_chunkY);
    if (!mcnk) {
        return;
    }

    Adt::SubChunk* mcnr = mcnk->Find(Adt::kMCNR);
    if (!mcnr || mcnr->m_data.size() < kVerts * 3) {
        return;
    }

    std::memcpy(mcnr->m_data.data(), normals, kVerts * 3);
    tile.m_dirty = true;
}

void UpdateNormals(const ChunkRef& ref) {
    CMapChunk* chunk = Resolve(ref);
    if (!chunk || !chunk->m_height || !chunk->m_normals) {
        return;
    }

    int8_t normals[kVerts * 3];
    RecomputeNormals(Gather(ref), normals);

    if (std::memcmp(chunk->m_normals, normals, sizeof(normals)) == 0) {
        return;
    }

    std::memcpy(chunk->m_normals, normals, sizeof(normals));

    std::string error;
    if (Session::OpenTile* tile = Session::Open(ref.m_tileX, ref.m_tileY, error)) {
        MirrorNormals(*tile, ref, normals);
    }

    if (chunk->m_renderChunk) {
        chunk->m_renderChunk->FreeBuf();
    }
}
}

int32_t Apply(const Stroke& stroke) {
    if (stroke.m_radius <= 0.0f) {
        return 0;
    }

    bool converging = stroke.m_mode == Mode::Flatten || stroke.m_mode == Mode::Smooth;
    if (converging ? stroke.m_blend <= 0.0f : stroke.m_amount == 0.0f) {
        return 0;
    }

    C3Vector lo{ stroke.m_center.x - stroke.m_radius, stroke.m_center.y - stroke.m_radius, 0.0f };
    C3Vector hi{ stroke.m_center.x + stroke.m_radius, stroke.m_center.y + stroke.m_radius, 0.0f };

    int32_t tileX0 = std::min(Coords::TileX(lo), Coords::TileX(hi));
    int32_t tileX1 = std::max(Coords::TileX(lo), Coords::TileX(hi));
    int32_t tileY0 = std::min(Coords::TileY(lo), Coords::TileY(hi));
    int32_t tileY1 = std::max(Coords::TileY(lo), Coords::TileY(hi));

    std::vector<Target> targets;

    for (int32_t tileY = tileY0; tileY <= tileY1; ++tileY) {
        for (int32_t tileX = tileX0; tileX <= tileX1; ++tileX) {
            if (!Access::IsAreaReady(Access::GetArea(tileX, tileY))) {
                continue;
            }

            for (int32_t chunkY = 0; chunkY < kSide; ++chunkY) {
                for (int32_t chunkX = 0; chunkX < kSide; ++chunkX) {
                    ChunkRef ref{ tileX, tileY, chunkX, chunkY };
                    CMapChunk* chunk = Resolve(ref);
                    if (!chunk || !chunk->m_height || !chunk->m_normals) {
                        continue;
                    }

                    if (!Coords::CircleTouchesChunk(chunk, stroke.m_center, stroke.m_radius)) {
                        continue;
                    }

                    Target target;
                    target.m_ref = ref;
                    target.m_chunk = chunk;
                    std::memcpy(target.m_heights, chunk->m_height, sizeof(target.m_heights));
                    targets.push_back(target);
                }
            }
        }
    }

    int32_t moved = 0;

    for (Target& target : targets) {
        Neighborhood hood;
        if (stroke.m_mode == Mode::Smooth) {
            hood = Gather(target.m_ref);
        }

        for (int32_t i = 0; i < kVerts; ++i) {
            C3Vector world = Coords::VertexWorldPos(target.m_chunk, i);
            float weight = Weight(Coords::Distance2D(world, stroke.m_center), stroke.m_radius,
                stroke.m_falloff);

            if (weight <= 0.0f) {
                continue;
            }

            float height = target.m_chunk->m_height[i];

            switch (stroke.m_mode) {
                case Mode::Raise:
                    height += stroke.m_amount * weight;
                    break;

                case Mode::Noise:
                    height += stroke.m_amount * weight * Jitter(target.m_ref, i);
                    break;

                case Mode::Flatten: {
                    float goal = stroke.m_targetHeight - target.m_chunk->m_topLeftCoords.z;
                    height += (goal - height) * weight * stroke.m_blend;
                    break;
                }

                case Mode::Smooth: {
                    float average = 0.0f;
                    if (!NeighborAverage(hood, i, average)) {
                        continue;
                    }

                    float goal = average - target.m_chunk->m_topLeftCoords.z;
                    height += (goal - height) * weight * stroke.m_blend;
                    break;
                }
            }

            if (height == target.m_heights[i]) {
                continue;
            }

            target.m_heights[i] = height;
            target.m_moved = true;
            ++moved;
        }
    }

    if (!moved) {
        return 0;
    }

    std::vector<ChunkRef> needNormals;

    for (Target& target : targets) {
        if (!target.m_moved) {
            continue;
        }

        std::memcpy(target.m_chunk->m_height, target.m_heights, sizeof(target.m_heights));

        std::string error;
        if (Session::OpenTile* tile = Session::Open(target.m_ref.m_tileX, target.m_ref.m_tileY, error)) {
            MirrorHeights(*tile, target.m_ref, target.m_heights);
        }

        target.m_chunk->CreateBounds();

        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                ChunkRef ref = Step(target.m_ref, dx, dy);
                if (std::find(needNormals.begin(), needNormals.end(), ref) == needNormals.end()) {
                    needNormals.push_back(ref);
                }
            }
        }
    }

    for (const ChunkRef& ref : needNormals) {
        UpdateNormals(ref);
    }

    return moved;
}

}
