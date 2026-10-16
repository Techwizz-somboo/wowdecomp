#include "HoT/Editor/Map/Placements.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_set>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/MapClient.h"
#include "HoT/Editor/Map/MapCoords.h"
#include "HoT/Editor/Map/TileSession.h"
#include "HoT/Editor/VectorMath.h"

namespace HoT::MapEditor::Placements {

namespace {
namespace VectorMath = HoT::Editor::VectorMath;

constexpr float kPi = 3.14159265f;
constexpr float kDegToRad = kPi / 180.0f;

constexpr float kScaleUnit = 1.0f / 1024.0f;

constexpr float kBuriedZ = -30000.0f;

CAaBox Normalized(const CAaBox& box) {
    CAaBox out;
    out.b.x = box.b.x < box.t.x ? box.b.x : box.t.x;
    out.b.y = box.b.y < box.t.y ? box.b.y : box.t.y;
    out.b.z = box.b.z < box.t.z ? box.b.z : box.t.z;
    out.t.x = box.b.x < box.t.x ? box.t.x : box.b.x;
    out.t.y = box.b.y < box.t.y ? box.t.y : box.b.y;
    out.t.z = box.b.z < box.t.z ? box.t.z : box.b.z;
    return out;
}

bool BoxIsEmpty(const CAaBox& box) {
    return box.t.x - box.b.x <= 0.0f && box.t.y - box.b.y <= 0.0f && box.t.z - box.b.z <= 0.0f;
}

CAaBox BoxAround(const C3Vector& point, float pad) {
    return { { point.x - pad, point.y - pad, point.z - pad },
        { point.x + pad, point.y + pad, point.z + pad } };
}

bool BoxIsFinite(const CAaBox& box) {
    return std::isfinite(box.b.x) && std::isfinite(box.b.y) && std::isfinite(box.b.z) && std::isfinite(box.t.x) && std::isfinite(box.t.y) && std::isfinite(box.t.z);
}

float BoxDiagonal(const CAaBox& box) {
    float dx = box.t.x - box.b.x;
    float dy = box.t.y - box.b.y;
    float dz = box.t.z - box.b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

float ClampToRange(float value, float lo, float hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

float BoxDistanceSqXY(const CAaBox& box, const C3Vector& point) {
    float dx = point.x - ClampToRange(point.x, box.b.x, box.t.x);
    float dy = point.y - ClampToRange(point.y, box.b.y, box.t.y);
    return dx * dx + dy * dy;
}

float DistanceSqXY(const C3Vector& a, const C3Vector& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

constexpr float kMaxSaneBoxSpan = 2000.0f;
constexpr float kMaxBoxDriftSq = 500.0f * 500.0f;

bool BoxUsable(const CAaBox& box, const C3Vector& origin) {
    if (!BoxIsFinite(box) || BoxIsEmpty(box)) {
        return false;
    }

    if (box.t.x - box.b.x > kMaxSaneBoxSpan || box.t.y - box.b.y > kMaxSaneBoxSpan || box.t.z - box.b.z > kMaxSaneBoxSpan) {
        return false;
    }

    return BoxDistanceSqXY(box, origin) <= kMaxBoxDriftSq;
}

CAaBox FallbackBounds(const C3Vector& position, float sphereRadius) {
    float pad = sphereRadius;
    if (!std::isfinite(pad) || pad <= 0.0f || pad > kMaxSaneBoxSpan) {
        pad = 1.0f;
    }

    return BoxAround(position, pad);
}

CAaBox Merge(const CAaBox& a, const CAaBox& b) {
    CAaBox out;
    out.b.x = a.b.x < b.b.x ? a.b.x : b.b.x;
    out.b.y = a.b.y < b.b.y ? a.b.y : b.b.y;
    out.b.z = a.b.z < b.b.z ? a.b.z : b.b.z;
    out.t.x = a.t.x > b.t.x ? a.t.x : b.t.x;
    out.t.y = a.t.y > b.t.y ? a.t.y : b.t.y;
    out.t.z = a.t.z > b.t.z ? a.t.z : b.t.z;
    return out;
}

bool RayHitsBox(const C3Vector& start, const C3Vector& dir, const CAaBox& box, float& tHit) {
    const float s[3] = { start.x, start.y, start.z };
    const float d[3] = { dir.x, dir.y, dir.z };
    const float lo[3] = { box.b.x, box.b.y, box.b.z };
    const float hi[3] = { box.t.x, box.t.y, box.t.z };

    float entry = 0.0f;
    float exit = 1.0f;

    for (int32_t axis = 0; axis < 3; ++axis) {
        if (std::fabs(d[axis]) < 1e-6f) {
            if (s[axis] < lo[axis] || s[axis] > hi[axis]) {
                return false;
            }

            continue;
        }

        float t0 = (lo[axis] - s[axis]) / d[axis];
        float t1 = (hi[axis] - s[axis]) / d[axis];
        if (t0 > t1) {
            float swap = t0;
            t0 = t1;
            t1 = swap;
        }

        if (t0 > entry) {
            entry = t0;
        }
        if (t1 < exit) {
            exit = t1;
        }
        if (entry > exit) {
            return false;
        }
    }

    tHit = entry;
    return true;
}

C44Matrix BuildDoodadMatrix(const C3Vector& world, const C3Vector& rotation, float scale) {
    C44Matrix mat;
    mat.d0 = world.x;
    mat.d1 = world.y;
    mat.d2 = world.z;

    mat.RotateAroundZ(rotation.y * kDegToRad + kPi);
    mat.RotateAroundY(rotation.x * kDegToRad);
    mat.RotateAroundX(rotation.z * kDegToRad);
    mat.Scale(scale);
    return mat;
}

uint16_t PackScale(float scale) {
    float raw = scale * 1024.0f;
    if (raw < 1.0f) {
        raw = 1.0f;
    }
    if (raw > 65535.0f) {
        raw = 65535.0f;
    }

    return static_cast<uint16_t>(raw + 0.5f);
}

Adt::TopChunk* FindTop(Adt::AdtDocument& doc, uint32_t id) {
    for (Adt::TopChunk& top : doc.m_order) {
        if (top.m_id == id) {
            return &top;
        }
    }

    return nullptr;
}

SMDoodadDef* DocDoodads(Session::OpenTile& tile, size_t& count) {
    count = 0;

    Adt::TopChunk* top = FindTop(tile.m_doc, Adt::kMDDF);
    if (!top) {
        return nullptr;
    }

    count = top->m_data.size() / sizeof(SMDoodadDef);
    return count ? reinterpret_cast<SMDoodadDef*>(top->m_data.data()) : nullptr;
}

SMMapObjDef* DocMapObjs(Session::OpenTile& tile, size_t& count) {
    count = 0;

    Adt::TopChunk* top = FindTop(tile.m_doc, Adt::kMODF);
    if (!top) {
        return nullptr;
    }

    count = top->m_data.size() / sizeof(SMMapObjDef);
    return count ? reinterpret_cast<SMMapObjDef*>(top->m_data.data()) : nullptr;
}

constexpr uint32_t kUniqueIdBase = 0x40000000;
constexpr uint32_t kUniqueIdCeiling = 0x80000000;

constexpr float kUnknownMapObjRadius = 200.0f;

std::string GamePath(const char* text) {
    std::string out = text ? text : "";
    for (char& c : out) {
        if (c == '/') {
            c = '\\';
        }
    }

    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
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

bool ModelExists(const std::string& path) {
    if (SFile::FileExistsEx(path.c_str(), 0)) {
        return true;
    }

    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }

    std::string swapped = path.substr(0, dot) + ".m2";
    return SFile::FileExistsEx(swapped.c_str(), 0) != 0;
}

float MapObjRadius(const std::string& path) {
    std::vector<uint8_t> bytes;
    std::string ignored;
    if (!Adt::ReadFileBytes(path.c_str(), bytes, ignored)) {
        return kUnknownMapObjRadius;
    }

    size_t at = 0;
    while (at + 8 <= bytes.size()) {
        uint32_t id = 0;
        uint32_t size = 0;
        std::memcpy(&id, bytes.data() + at, sizeof(id));
        std::memcpy(&size, bytes.data() + at + 4, sizeof(size));
        at += 8;

        if (size > bytes.size() - at) {
            break;
        }

        if (id == Adt::ChunkId("MOHD") && size >= 0x3C) {
            float corners[6] = {};
            std::memcpy(corners, bytes.data() + at + 0x24, sizeof(corners));

            float radius = 0.0f;
            for (float value : corners) {
                if (std::isfinite(value) && std::fabs(value) > radius) {
                    radius = std::fabs(value);
                }
            }

            radius *= 1.7320508f;
            return radius > 1.0f && radius < 4000.0f ? radius : kUnknownMapObjRadius;
        }

        at += size;
    }

    return kUnknownMapObjRadius;
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

const char* DocName(Session::OpenTile& tile, bool doodad, uint32_t nameId) {
    Adt::TopChunk* ids = FindTop(tile.m_doc, doodad ? Adt::kMMID : Adt::kMWID);
    Adt::TopChunk* blob = FindTop(tile.m_doc, doodad ? Adt::kMMDX : Adt::kMWMO);
    if (!ids || !blob) {
        return nullptr;
    }

    if ((static_cast<size_t>(nameId) + 1) * sizeof(uint32_t) > ids->m_data.size()) {
        return nullptr;
    }

    uint32_t offset = 0;
    std::memcpy(&offset, ids->m_data.data() + nameId * sizeof(uint32_t), sizeof(offset));
    if (offset >= blob->m_data.size()) {
        return nullptr;
    }

    const char* text = reinterpret_cast<const char*>(blob->m_data.data()) + offset;
    if (!std::memchr(text, 0, blob->m_data.size() - offset)) {
        return nullptr;
    }

    return text;
}

bool SameName(const char* a, const char* b) {
    for (; *a && *b; ++a, ++b) {
        char ca = *a == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(*a)));
        char cb = *b == '/' ? '\\' : static_cast<char>(std::tolower(static_cast<unsigned char>(*b)));
        if (ca != cb) {
            return false;
        }
    }

    return *a == *b;
}

bool ResolveName(Session::OpenTile& tile, bool doodad, const char* name, uint32_t& nameId) {
    uint32_t blobId = doodad ? Adt::kMMDX : Adt::kMWMO;
    uint32_t idsId = doodad ? Adt::kMMID : Adt::kMWID;

    if (Adt::TopChunk* ids = FindTop(tile.m_doc, idsId)) {
        size_t count = ids->m_data.size() / sizeof(uint32_t);
        for (size_t i = 0; i < count; ++i) {
            const char* text = DocName(tile, doodad, static_cast<uint32_t>(i));
            if (text && SameName(text, name)) {
                nameId = static_cast<uint32_t>(i);
                return true;
            }
        }
    }

    EnsureTop(tile.m_doc, blobId, doodad ? Adt::kMTEX : Adt::kMMID);
    EnsureTop(tile.m_doc, idsId, blobId);

    Adt::TopChunk* blob = FindTop(tile.m_doc, blobId);
    Adt::TopChunk* ids = FindTop(tile.m_doc, idsId);
    if (!blob || !ids) {
        return false;
    }

    uint32_t offset = static_cast<uint32_t>(blob->m_data.size());
    size_t length = std::strlen(name);
    blob->m_data.insert(blob->m_data.end(), name, name + length + 1);

    nameId = static_cast<uint32_t>(ids->m_data.size() / sizeof(uint32_t));

    const uint8_t* raw = reinterpret_cast<const uint8_t*>(&offset);
    ids->m_data.insert(ids->m_data.end(), raw, raw + sizeof(offset));
    return true;
}

bool ChunkOverlaps(int32_t tileX, int32_t tileY, int32_t chunkX, int32_t chunkY, const CAaBox& bounds) {
    float maxY = Coords::kMapHalf - static_cast<float>(tileX * 16 + chunkX) * Coords::kChunkSize;
    float maxX = Coords::kMapHalf - static_cast<float>(tileY * 16 + chunkY) * Coords::kChunkSize;

    return bounds.t.x >= maxX - Coords::kChunkSize && bounds.b.x <= maxX && bounds.t.y >= maxY - Coords::kChunkSize && bounds.b.y <= maxY;
}

void SetRef(Adt::Mcnk& mcnk, bool doodad, uint32_t index, bool want, bool shift) {
    Adt::SubChunk* mcrf = mcnk.Find(Adt::kMCRF);
    if (!mcrf) {
        return;
    }

    size_t doodadRefs = mcnk.m_header.m_nDoodadRefs;
    size_t mapObjRefs = mcnk.m_header.m_nMapObjRefs;
    size_t total = doodadRefs + mapObjRefs;
    if (mcrf->m_data.size() < total * sizeof(uint32_t)) {
        return;
    }

    std::vector<uint32_t> refs(total);
    if (total) {
        std::memcpy(refs.data(), mcrf->m_data.data(), total * sizeof(uint32_t));
    }

    std::vector<uint32_t> doodadOut;
    std::vector<uint32_t> mapObjOut;
    doodadOut.reserve(doodadRefs + 1);
    mapObjOut.reserve(mapObjRefs + 1);
    bool present = false;

    for (size_t i = 0; i < total; ++i) {
        bool isDoodad = i < doodadRefs;
        std::vector<uint32_t>& target = isDoodad ? doodadOut : mapObjOut;
        uint32_t value = refs[i];

        if (isDoodad != doodad) {
            target.push_back(value);
            continue;
        }

        if (value == index) {
            if (!want) {
                continue;
            }

            present = true;
            target.push_back(value);
            continue;
        }

        target.push_back(shift && value > index ? value - 1 : value);
    }

    if (want && !present) {
        (doodad ? doodadOut : mapObjOut).push_back(index);
    }

    mcnk.m_header.m_nDoodadRefs = static_cast<uint32_t>(doodadOut.size());
    mcnk.m_header.m_nMapObjRefs = static_cast<uint32_t>(mapObjOut.size());

    mcrf->m_data.resize((doodadOut.size() + mapObjOut.size()) * sizeof(uint32_t));
    if (!doodadOut.empty()) {
        std::memcpy(mcrf->m_data.data(), doodadOut.data(), doodadOut.size() * sizeof(uint32_t));
    }
    if (!mapObjOut.empty()) {
        std::memcpy(mcrf->m_data.data() + doodadOut.size() * sizeof(uint32_t), mapObjOut.data(),
            mapObjOut.size() * sizeof(uint32_t));
    }
}

void RetargetRefs(Session::OpenTile& tile, bool doodad, uint32_t index, const CAaBox& bounds) {
    for (int32_t chunkY = 0; chunkY < 16; ++chunkY) {
        for (int32_t chunkX = 0; chunkX < 16; ++chunkX) {
            Adt::Mcnk* mcnk = tile.m_doc.ChunkAt(chunkX, chunkY);
            if (!mcnk) {
                continue;
            }

            bool want = ChunkOverlaps(tile.m_tileX, tile.m_tileY, chunkX, chunkY, bounds);
            SetRef(*mcnk, doodad, index, want, false);
        }
    }
}

void DropRefs(Session::OpenTile& tile, bool doodad, uint32_t index) {
    for (int32_t chunkY = 0; chunkY < 16; ++chunkY) {
        for (int32_t chunkX = 0; chunkX < 16; ++chunkX) {
            if (Adt::Mcnk* mcnk = tile.m_doc.ChunkAt(chunkX, chunkY)) {
                SetRef(*mcnk, doodad, index, false, true);
            }
        }
    }
}

template <typename Fn>
void ForEachLiveDef(bool doodads, Fn&& fn) {
    std::unordered_set<CMapBaseObj*> seen;

    for (int32_t tileY = 0; tileY < Access::kTilesPerSide; ++tileY) {
        for (int32_t tileX = 0; tileX < Access::kTilesPerSide; ++tileX) {
            CMapArea* area = Access::GetArea(tileX, tileY);
            if (!Access::IsAreaReady(area)) {
                continue;
            }

            for (int32_t i = 0; i < Access::kChunksPerTile; ++i) {
                CMapChunk* chunk = area->m_mapChunks[i];
                if (!chunk) {
                    continue;
                }

                auto& list = doodads ? chunk->m_doodadDefLinkList : chunk->m_mapObjDefLinkList;
                Access::ForEachLinkOwner(list,
                    [&](CMapBaseObj* owner) {
                        if (owner && seen.insert(owner).second) {
                            fn(owner);
                        }
                    });
            }
        }
    }
}

CAaBox DoodadBounds(const CMapDoodadDef* def) {
    CAaBox box = Normalized(def->m_bbox);
    if (BoxUsable(box, def->m_position)) {
        return box;
    }

    return FallbackBounds(def->m_position, def->m_sphere.r);
}

CAaBox MapObjBounds(const CMapObjDef* def) {
    CAaBox box = Normalized(def->m_bbox);
    if (BoxUsable(box, def->m_position)) {
        return box;
    }

    return FallbackBounds(def->m_position, def->m_sphere.r);
}

Session::OpenTile* OwningTile(const C3Vector& world, int32_t& tileX, int32_t& tileY) {
    tileX = Coords::TileX(world);
    tileY = Coords::TileY(world);
    if (!Access::IsValidTile(tileX, tileY)) {
        return nullptr;
    }

    std::string ignored;
    return Session::Open(tileX, tileY, ignored);
}

bool RefForDoodad(const CMapDoodadDef* def, Ref& out) {
    int32_t tileX = 0;
    int32_t tileY = 0;
    Session::OpenTile* tile = OwningTile(def->m_position, tileX, tileY);
    if (!tile) {
        return false;
    }

    size_t count = 0;
    const SMDoodadDef* entries = DocDoodads(*tile, count);
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].m_uniqueId != def->m_hashval) {
            continue;
        }

        out.m_kind = Kind::Doodad;
        out.m_tileX = tileX;
        out.m_tileY = tileY;
        out.m_index = static_cast<int32_t>(i);
        out.m_uniqueId = def->m_hashval;
        return true;
    }

    return false;
}

bool RefForMapObj(const CMapObjDef* def, Ref& out) {
    int32_t tileX = 0;
    int32_t tileY = 0;
    Session::OpenTile* tile = OwningTile(def->m_position, tileX, tileY);
    if (!tile) {
        return false;
    }

    size_t count = 0;
    const SMMapObjDef* entries = DocMapObjs(*tile, count);
    for (size_t i = 0; i < count; ++i) {
        if (entries[i].m_uniqueId != def->m_hashval) {
            continue;
        }

        out.m_kind = Kind::MapObj;
        out.m_tileX = tileX;
        out.m_tileY = tileY;
        out.m_index = static_cast<int32_t>(i);
        out.m_uniqueId = entries[i].m_uniqueId;
        return true;
    }

    return false;
}

CMapObjDef* LiveMapObj(const SMMapObjDef& entry) {
    return Access::FindMapObjDef(entry.m_uniqueId);
}

std::string TileName(int32_t tileX, int32_t tileY) {
    return std::to_string(tileX) + "_" + std::to_string(tileY);
}

uint32_t AppendEntry(Adt::TopChunk& top, const void* entry, size_t stride) {
    uint32_t index = static_cast<uint32_t>(top.m_data.size() / stride);
    const uint8_t* raw = static_cast<const uint8_t*>(entry);
    top.m_data.insert(top.m_data.end(), raw, raw + stride);
    return index;
}

void RaiseHighest(uint32_t& highest, uint32_t candidate) {
    if (candidate >= kUniqueIdBase && candidate < kUniqueIdCeiling && candidate >= highest) {
        highest = candidate + 1;
    }
}

uint32_t AllocateUniqueId() {
    static uint32_t next = kUniqueIdBase;
    uint32_t highest = next;

    for (Session::OpenTile* tile : Session::AllOpen()) {
        size_t count = 0;
        if (const SMDoodadDef* entries = DocDoodads(*tile, count)) {
            for (size_t i = 0; i < count; ++i) {
                RaiseHighest(highest, entries[i].m_uniqueId);
            }
        }

        count = 0;
        if (const SMMapObjDef* entries = DocMapObjs(*tile, count)) {
            for (size_t i = 0; i < count; ++i) {
                RaiseHighest(highest, entries[i].m_uniqueId);
            }
        }
    }

    for (int32_t tileY = 0; tileY < Access::kTilesPerSide; ++tileY) {
        for (int32_t tileX = 0; tileX < Access::kTilesPerSide; ++tileX) {
            CMapArea* area = Access::GetArea(tileX, tileY);
            if (!Access::IsAreaReady(area)) {
                continue;
            }

            for (int32_t i = 0; i < area->m_doodadDefCount; ++i) {
                RaiseHighest(highest, area->m_doodadDef[i].m_uniqueId);
            }

            for (int32_t i = 0; i < area->m_mapObjDefCount; ++i) {
                RaiseHighest(highest, area->m_mapObjDef[i].m_uniqueId);
            }
        }
    }

    next = highest + 1;
    return highest;
}

bool InsertDoodad(Session::OpenTile& tile, const std::string& model, const Transform& transform,
    uint16_t flags, Ref& out, std::string& error) {
    uint32_t nameId = 0;
    if (!ResolveName(tile, true, model.c_str(), nameId)) {
        error = "could not add " + model + " to tile " + TileName(tile.m_tileX, tile.m_tileY);
        return false;
    }

    SMDoodadDef entry{};
    entry.m_nameId = nameId;
    entry.m_uniqueId = AllocateUniqueId();
    entry.m_position = Coords::WorldToAdt(transform.m_position);
    entry.m_rotation = transform.m_rotation;
    entry.m_scale = PackScale(transform.m_scale);
    entry.m_flags = flags;

    Adt::TopChunk* top = EnsureTop(tile.m_doc, Adt::kMDDF, Adt::kMWID);
    uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

    RetargetRefs(tile, true, index, BoxAround(transform.m_position, 8.0f));
    tile.m_dirty = true;

    out.m_kind = Kind::Doodad;
    out.m_tileX = tile.m_tileX;
    out.m_tileY = tile.m_tileY;
    out.m_index = static_cast<int32_t>(index);
    out.m_uniqueId = entry.m_uniqueId;
    return true;
}

bool InsertMapObj(Session::OpenTile& tile, const std::string& model, const Transform& transform,
    const CAaBox& extents, uint16_t flags, uint16_t doodadSet, uint16_t nameSet, Ref& out,
    std::string& error) {
    uint32_t nameId = 0;
    if (!ResolveName(tile, false, model.c_str(), nameId)) {
        error = "could not add " + model + " to tile " + TileName(tile.m_tileX, tile.m_tileY);
        return false;
    }

    SMMapObjDef entry{};
    entry.m_nameId = nameId;
    entry.m_uniqueId = AllocateUniqueId();
    entry.m_position = Coords::WorldToAdt(transform.m_position);
    entry.m_rotation = transform.m_rotation;
    entry.m_extents = extents;
    entry.m_flags = flags;
    entry.m_doodadSet = doodadSet;
    entry.m_nameSet = nameSet;

    entry.m_scale = 1024;

    Adt::TopChunk* top = EnsureTop(tile.m_doc, Adt::kMODF, Adt::kMDDF);
    uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

    CAaBox world = Merge(BoxAround(transform.m_position, 1.0f),
        { Coords::AdtToWorld(extents.b), Coords::AdtToWorld(extents.t) });

    RetargetRefs(tile, false, index, Normalized(world));
    tile.m_dirty = true;

    out.m_kind = Kind::MapObj;
    out.m_tileX = tile.m_tileX;
    out.m_tileY = tile.m_tileY;
    out.m_index = static_cast<int32_t>(index);
    out.m_uniqueId = entry.m_uniqueId;
    return true;
}

bool MigrateDoodad(Ref& ref, Session::OpenTile& from, Session::OpenTile& to,
    const Transform& transform, std::string& error) {
    size_t count = 0;
    SMDoodadDef* entries = DocDoodads(from, count);
    if (!entries || static_cast<size_t>(ref.m_index) >= count || entries[ref.m_index].m_uniqueId != ref.m_uniqueId) {
        error = "that placement is no longer at index " + std::to_string(ref.m_index);
        return false;
    }

    SMDoodadDef entry = entries[ref.m_index];

    const char* found = DocName(from, true, entry.m_nameId);
    if (!found) {
        error = "tile " + TileName(from.m_tileX, from.m_tileY) + " has no model path for that entry";
        return false;
    }

    std::string model = found;

    uint32_t nameId = 0;
    if (!ResolveName(to, true, model.c_str(), nameId)) {
        error = "could not add " + model + " to tile " + TileName(to.m_tileX, to.m_tileY);
        return false;
    }

    entry.m_nameId = nameId;
    entry.m_position = Coords::WorldToAdt(transform.m_position);
    entry.m_rotation = transform.m_rotation;
    entry.m_scale = PackScale(transform.m_scale);

    if (!Remove(ref, error)) {
        return false;
    }

    Adt::TopChunk* top = EnsureTop(to.m_doc, Adt::kMDDF, Adt::kMWID);
    uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

    ref.m_tileX = to.m_tileX;
    ref.m_tileY = to.m_tileY;
    ref.m_index = static_cast<int32_t>(index);

    CAaBox bounds = BoxAround(transform.m_position, 1.0f);
    if (CMapDoodadDef* def = Access::FindDoodadDef(ref.m_uniqueId)) {
        C44Matrix mat = BuildDoodadMatrix(transform.m_position, transform.m_rotation, entry.m_scale * kScaleUnit);
        CMap::UpdateDoodadDefTransform(def, mat);
        bounds = Merge(bounds, DoodadBounds(def));
    }

    RetargetRefs(to, true, index, bounds);
    to.m_dirty = true;
    return true;
}

bool MigrateMapObj(Ref& ref, Session::OpenTile& from, Session::OpenTile& to,
    const Transform& transform, std::string& error) {
    size_t count = 0;
    SMMapObjDef* entries = DocMapObjs(from, count);
    if (!entries || static_cast<size_t>(ref.m_index) >= count || entries[ref.m_index].m_uniqueId != ref.m_uniqueId) {
        error = "that placement is no longer at index " + std::to_string(ref.m_index);
        return false;
    }

    SMMapObjDef entry = entries[ref.m_index];

    const char* found = DocName(from, false, entry.m_nameId);
    if (!found) {
        error = "tile " + TileName(from.m_tileX, from.m_tileY) + " has no model path for that entry";
        return false;
    }

    std::string model = found;

    uint32_t nameId = 0;
    if (!ResolveName(to, false, model.c_str(), nameId)) {
        error = "could not add " + model + " to tile " + TileName(to.m_tileX, to.m_tileY);
        return false;
    }

    C3Vector adt = Coords::WorldToAdt(transform.m_position);
    C3Vector shift = VectorMath::Subtract(adt, entry.m_position);

    entry.m_nameId = nameId;
    entry.m_position = adt;
    entry.m_rotation = transform.m_rotation;

    entry.m_extents.b = VectorMath::Add(entry.m_extents.b, shift);
    entry.m_extents.t = VectorMath::Add(entry.m_extents.t, shift);

    if (!Remove(ref, error)) {
        return false;
    }

    Adt::TopChunk* top = EnsureTop(to.m_doc, Adt::kMODF, Adt::kMDDF);
    uint32_t index = AppendEntry(*top, &entry, sizeof(entry));

    ref.m_tileX = to.m_tileX;
    ref.m_tileY = to.m_tileY;
    ref.m_index = static_cast<int32_t>(index);

    CAaBox bounds = BoxAround(transform.m_position, 1.0f);
    if (CMapObjDef* def = Access::FindMapObjDef(entry.m_uniqueId)) {
        C3Vector world = transform.m_position;
        CMapObjDef::UpdatePos(def, &world, transform.m_rotation.y * kDegToRad + kPi,
            transform.m_rotation.x * kDegToRad, transform.m_rotation.z * kDegToRad);
        bounds = Merge(bounds, MapObjBounds(def));
    }

    RetargetRefs(to, false, index, bounds);
    to.m_dirty = true;
    return true;
}

bool Migrate(Ref& ref, int32_t destX, int32_t destY, const Transform& transform, std::string& error) {
    if (!Access::IsValidTile(destX, destY)) {
        error = "that lands off the edge of the map";
        return false;
    }

    Session::OpenTile* from = Session::Open(ref.m_tileX, ref.m_tileY, error);
    if (!from) {
        return false;
    }

    Session::OpenTile* to = Session::Open(destX, destY, error);
    if (!to) {
        error = "tile " + TileName(destX, destY) + " could not be opened: " + error;
        return false;
    }

    return ref.m_kind == Kind::Doodad ? MigrateDoodad(ref, *from, *to, transform, error)
                                    : MigrateMapObj(ref, *from, *to, transform, error);
}
}

bool Pick(const C3Vector& start, const C3Vector& end, Ref& out) {
    if (!Access::IsActive()) {
        return false;
    }

    C3Vector dir = VectorMath::Subtract(end, start);
    float rayLength = VectorMath::Length(dir);

    Ref best;
    float bestDistance = 0.0f;
    float bestSize = 0.0f;

    auto beats = [&](float distance, float size) {
        if (!best.Valid()) {
            return true;
        }

        if (distance > bestDistance + 0.5f) {
            return false;
        }

        if (distance > bestDistance - 0.5f) {
            return size < bestSize;
        }

        return true;
    };

    ForEachLiveDef(true,
        [&](CMapBaseObj* owner) {
            auto* def = static_cast<CMapDoodadDef*>(owner);
            CAaBox box = DoodadBounds(def);

            float t = 0.0f;
            if (!RayHitsBox(start, dir, box, t)) {
                return;
            }

            float distance = t * rayLength;
            float size = BoxDiagonal(box);
            if (!beats(distance, size)) {
                return;
            }

            Ref ref;
            if (!RefForDoodad(def, ref)) {
                return;
            }

            bestDistance = distance;
            bestSize = size;
            best = ref;
        });

    ForEachLiveDef(false,
        [&](CMapBaseObj* owner) {
            auto* def = static_cast<CMapObjDef*>(owner);
            CAaBox box = MapObjBounds(def);

            float t = 0.0f;
            if (!RayHitsBox(start, dir, box, t)) {
                return;
            }

            float distance = t * rayLength;
            float size = BoxDiagonal(box);
            if (!beats(distance, size)) {
                return;
            }

            Ref ref;
            if (!RefForMapObj(def, ref)) {
                return;
            }

            bestDistance = distance;
            bestSize = size;
            best = ref;
        });

    if (!best.Valid()) {
        return false;
    }

    out = best;
    return true;
}

bool Resolve(Ref& ref) {
    if (!ref.Valid()) {
        return false;
    }

    Session::OpenTile* tile = Session::Find(ref.m_tileX, ref.m_tileY);
    if (!tile) {
        std::string ignored;
        tile = Session::Open(ref.m_tileX, ref.m_tileY, ignored);
    }

    if (!tile) {
        return false;
    }

    size_t count = 0;
    if (ref.m_kind == Kind::Doodad) {
        const SMDoodadDef* entries = DocDoodads(*tile, count);
        if (static_cast<size_t>(ref.m_index) < count && entries[ref.m_index].m_uniqueId == ref.m_uniqueId) {
            return true;
        }

        for (size_t i = 0; i < count; ++i) {
            if (entries[i].m_uniqueId != ref.m_uniqueId) {
                continue;
            }

            ref.m_index = static_cast<int32_t>(i);
            return true;
        }

        return false;
    }

    const SMMapObjDef* entries = DocMapObjs(*tile, count);
    if (static_cast<size_t>(ref.m_index) < count && entries[ref.m_index].m_uniqueId == ref.m_uniqueId) {
        return true;
    }

    for (size_t i = 0; i < count; ++i) {
        if (entries[i].m_uniqueId != ref.m_uniqueId) {
            continue;
        }

        ref.m_index = static_cast<int32_t>(i);
        return true;
    }

    return false;
}

bool Describe(const Ref& ref, Info& out) {
    if (!ref.Valid()) {
        return false;
    }

    Session::OpenTile* tile = Session::Find(ref.m_tileX, ref.m_tileY);
    if (!tile) {
        std::string ignored;
        tile = Session::Open(ref.m_tileX, ref.m_tileY, ignored);
    }

    if (!tile) {
        return false;
    }

    CMapArea* area = Access::GetArea(ref.m_tileX, ref.m_tileY);

    out = Info{};
    out.m_ref = ref;

    if (ref.m_kind == Kind::Doodad) {
        size_t count = 0;
        const SMDoodadDef* entries = DocDoodads(*tile, count);
        if (static_cast<size_t>(ref.m_index) >= count) {
            return false;
        }

        const SMDoodadDef& entry = entries[ref.m_index];
        out.m_transform.m_position = Coords::AdtToWorld(entry.m_position);
        out.m_transform.m_rotation = entry.m_rotation;
        out.m_transform.m_scale = entry.m_scale * kScaleUnit;

        if (const char* name = DocName(*tile, true, entry.m_nameId)) {
            out.m_name = name;
        } else if (const char* name = Access::IsAreaReady(area) ? Access::DoodadFileName(area, entry.m_nameId) : nullptr) {
            out.m_name = name;
        }

        if (CMapDoodadDef* def = Access::FindDoodadDef(entry.m_uniqueId)) {
            out.m_bounds = DoodadBounds(def);
            out.m_live = true;
        }

        return true;
    }

    size_t count = 0;
    const SMMapObjDef* entries = DocMapObjs(*tile, count);
    if (static_cast<size_t>(ref.m_index) >= count) {
        return false;
    }

    const SMMapObjDef& entry = entries[ref.m_index];
    out.m_transform.m_position = Coords::AdtToWorld(entry.m_position);
    out.m_transform.m_rotation = entry.m_rotation;
    out.m_transform.m_scale = 1.0f;

    if (const char* name = DocName(*tile, false, entry.m_nameId)) {
        out.m_name = name;
    } else if (const char* name = Access::IsAreaReady(area) ? Access::MapObjFileName(area, entry.m_nameId) : nullptr) {
        out.m_name = name;
    }

    if (CMapObjDef* def = LiveMapObj(entry)) {
        out.m_bounds = MapObjBounds(def);
        out.m_live = true;
    }

    return true;
}

bool Preview(const Ref& ref, const Transform& transform) {
    if (!ref.Valid()) {
        return false;
    }

    if (ref.m_kind == Kind::Doodad) {
        CMapDoodadDef* def = Access::FindDoodadDef(ref.m_uniqueId);
        if (!def) {
            return false;
        }

        C44Matrix mat = BuildDoodadMatrix(transform.m_position, transform.m_rotation, transform.m_scale);
        CMap::UpdateDoodadDefTransform(def, mat);
        return true;
    }

    Session::OpenTile* tile = Session::Find(ref.m_tileX, ref.m_tileY);
    if (!tile) {
        return false;
    }

    size_t count = 0;
    const SMMapObjDef* entries = DocMapObjs(*tile, count);
    if (static_cast<size_t>(ref.m_index) >= count) {
        return false;
    }

    CMapObjDef* def = LiveMapObj(entries[ref.m_index]);
    if (!def) {
        return false;
    }

    C3Vector world = transform.m_position;
    CMapObjDef::UpdatePos(def, &world, transform.m_rotation.y * kDegToRad + kPi,
        transform.m_rotation.x * kDegToRad, transform.m_rotation.z * kDegToRad);
    return true;
}

bool SetTransform(Ref& ref, const Transform& transform, std::string& error) {
    if (!ref.Valid()) {
        error = "nothing selected";
        return false;
    }

    Session::OpenTile* tile = Session::Open(ref.m_tileX, ref.m_tileY, error);
    if (!tile) {
        return false;
    }

    int32_t destX = Coords::TileX(transform.m_position);
    int32_t destY = Coords::TileY(transform.m_position);
    if (destX != ref.m_tileX || destY != ref.m_tileY) {
        return Migrate(ref, destX, destY, transform, error);
    }

    CMapArea* area = Access::GetArea(ref.m_tileX, ref.m_tileY);
    C3Vector adt = Coords::WorldToAdt(transform.m_position);

    if (ref.m_kind == Kind::Doodad) {
        size_t count = 0;
        SMDoodadDef* entries = DocDoodads(*tile, count);
        if (static_cast<size_t>(ref.m_index) >= count || entries[ref.m_index].m_uniqueId != ref.m_uniqueId) {
            error = "that placement is no longer at index " + std::to_string(ref.m_index);
            return false;
        }

        uint16_t scale = PackScale(transform.m_scale);

        entries[ref.m_index].m_position = adt;
        entries[ref.m_index].m_rotation = transform.m_rotation;
        entries[ref.m_index].m_scale = scale;

        if (Access::IsAreaReady(area) && ref.m_index < area->m_doodadDefCount) {
            area->m_doodadDef[ref.m_index].m_position = adt;
            area->m_doodadDef[ref.m_index].m_rotation = transform.m_rotation;
            area->m_doodadDef[ref.m_index].m_scale = scale;
        }

        CAaBox bounds = BoxAround(transform.m_position, 1.0f);
        if (CMapDoodadDef* def = Access::FindDoodadDef(ref.m_uniqueId)) {
            C44Matrix mat = BuildDoodadMatrix(transform.m_position, transform.m_rotation, scale * kScaleUnit);
            CMap::UpdateDoodadDefTransform(def, mat);
            bounds = Merge(bounds, DoodadBounds(def));
        }

        RetargetRefs(*tile, true, static_cast<uint32_t>(ref.m_index), bounds);
        tile->m_dirty = true;
        return true;
    }

    size_t count = 0;
    SMMapObjDef* entries = DocMapObjs(*tile, count);
    if (static_cast<size_t>(ref.m_index) >= count || entries[ref.m_index].m_uniqueId != ref.m_uniqueId) {
        error = "that placement is no longer at index " + std::to_string(ref.m_index);
        return false;
    }

    CMapObjDef* def = LiveMapObj(entries[ref.m_index]);

    C3Vector shift = VectorMath::Subtract(adt, entries[ref.m_index].m_position);
    entries[ref.m_index].m_position = adt;
    entries[ref.m_index].m_rotation = transform.m_rotation;

    entries[ref.m_index].m_extents.b = VectorMath::Add(entries[ref.m_index].m_extents.b, shift);
    entries[ref.m_index].m_extents.t = VectorMath::Add(entries[ref.m_index].m_extents.t, shift);

    if (Access::IsAreaReady(area) && ref.m_index < area->m_mapObjDefCount) {
        area->m_mapObjDef[ref.m_index] = entries[ref.m_index];
    }

    CAaBox bounds = BoxAround(transform.m_position, 1.0f);
    if (def) {
        C3Vector world = transform.m_position;
        CMapObjDef::UpdatePos(def, &world, transform.m_rotation.y * kDegToRad + kPi,
            transform.m_rotation.x * kDegToRad, transform.m_rotation.z * kDegToRad);
        bounds = Merge(bounds, Normalized(def->m_bbox));
    }

    RetargetRefs(*tile, false, static_cast<uint32_t>(ref.m_index), bounds);
    tile->m_dirty = true;
    return true;
}

bool Remove(const Ref& ref, std::string& error) {
    if (!ref.Valid()) {
        error = "nothing selected";
        return false;
    }

    Session::OpenTile* tile = Session::Open(ref.m_tileX, ref.m_tileY, error);
    if (!tile) {
        return false;
    }

    bool doodad = ref.m_kind == Kind::Doodad;
    Adt::TopChunk* top = FindTop(tile->m_doc, doodad ? Adt::kMDDF : Adt::kMODF);
    if (!top) {
        error = "tile has no placement list";
        return false;
    }

    size_t stride = doodad ? sizeof(SMDoodadDef) : sizeof(SMMapObjDef);
    size_t count = top->m_data.size() / stride;
    if (static_cast<size_t>(ref.m_index) >= count) {
        error = "index " + std::to_string(ref.m_index) + " is past the end of the list";
        return false;
    }

    if (doodad) {
        const auto& entry = reinterpret_cast<const SMDoodadDef*>(top->m_data.data())[ref.m_index];
        if (entry.m_uniqueId != ref.m_uniqueId) {
            error = "that placement is no longer at index " + std::to_string(ref.m_index);
            return false;
        }

        if (CMapDoodadDef* def = Access::FindDoodadDef(entry.m_uniqueId)) {
            C3Vector buried = Coords::AdtToWorld(entry.m_position);
            buried.z = kBuriedZ;

            C44Matrix mat = BuildDoodadMatrix(buried, entry.m_rotation, entry.m_scale * kScaleUnit);
            CMap::UpdateDoodadDefTransform(def, mat);
        }
    } else {
        const auto& entry = reinterpret_cast<const SMMapObjDef*>(top->m_data.data())[ref.m_index];
        if (entry.m_uniqueId != ref.m_uniqueId) {
            error = "that placement is no longer at index " + std::to_string(ref.m_index);
            return false;
        }

        if (CMapObjDef* def = LiveMapObj(entry)) {
            C3Vector buried = Coords::AdtToWorld(entry.m_position);
            buried.z = kBuriedZ;

            CMapObjDef::UpdatePos(def, &buried, entry.m_rotation.y * kDegToRad + kPi,
                entry.m_rotation.x * kDegToRad, entry.m_rotation.z * kDegToRad);
        }
    }

    DropRefs(*tile, doodad, static_cast<uint32_t>(ref.m_index));

    auto begin = top->m_data.begin() + static_cast<ptrdiff_t>(ref.m_index * stride);
    top->m_data.erase(begin, begin + static_cast<ptrdiff_t>(stride));

    tile->m_dirty = true;
    return true;
}

int32_t ListNear(const C3Vector& center, float radius, std::vector<Info>& out) {
    out.clear();
    if (!Access::IsActive()) {
        return 0;
    }

    float limit = radius * radius;

    auto collect = [&](const Ref& ref, float distanceSq) {
        Info info;
        if (!Describe(ref, info)) {
            return;
        }

        info.m_distance = std::sqrt(distanceSq);
        info.m_originDistance = std::sqrt(DistanceSqXY(info.m_transform.m_position, center));
        out.push_back(info);
    };

    ForEachLiveDef(true,
        [&](CMapBaseObj* owner) {
            auto* def = static_cast<CMapDoodadDef*>(owner);
            float distanceSq = BoxDistanceSqXY(DoodadBounds(def), center);
            if (distanceSq > limit) {
                return;
            }

            Ref ref;
            if (RefForDoodad(def, ref)) {
                collect(ref, distanceSq);
            }
        });

    ForEachLiveDef(false,
        [&](CMapBaseObj* owner) {
            auto* def = static_cast<CMapObjDef*>(owner);
            float distanceSq = BoxDistanceSqXY(MapObjBounds(def), center);
            if (distanceSq > limit) {
                return;
            }

            Ref ref;
            if (RefForMapObj(def, ref)) {
                collect(ref, distanceSq);
            }
        });

    std::stable_sort(out.begin(), out.end(),
        [](const Info& a, const Info& b) {
            if (a.m_distance != b.m_distance) {
                return a.m_distance < b.m_distance;
            }
            return a.m_originDistance < b.m_originDistance;
        });

    return static_cast<int32_t>(out.size());
}

bool KindForPath(const char* path, Kind& out) {
    std::string text = GamePath(path);
    size_t dot = text.find_last_of('.');
    if (dot == std::string::npos) {
        return false;
    }

    std::string ext = text.substr(dot);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (ext == ".wmo") {
        out = Kind::MapObj;
        return true;
    }

    if (ext == ".m2" || ext == ".mdx" || ext == ".mdl") {
        out = Kind::Doodad;
        return true;
    }

    return false;
}

bool Add(Kind kind, const char* path, const Transform& transform, Ref& out, std::string& error) {
    std::string model = GamePath(path);
    if (model.empty()) {
        error = "no model path given";
        return false;
    }

    if (LooksMangled(model)) {
        error = "that path has a control character in it, so a Lua escape ate a backslash. "
                "Use forward slashes.";
        return false;
    }

    if (!ModelExists(model)) {
        error = "the game files have no " + model;
        return false;
    }

    int32_t tileX = Coords::TileX(transform.m_position);
    int32_t tileY = Coords::TileY(transform.m_position);
    if (!Access::IsValidTile(tileX, tileY)) {
        error = "that spot is off the edge of the map";
        return false;
    }

    Session::OpenTile* tile = Session::Open(tileX, tileY, error);
    if (!tile) {
        return false;
    }

    if (kind == Kind::Doodad) {
        return InsertDoodad(*tile, model, transform, 0, out, error);
    }

    C3Vector adt = Coords::WorldToAdt(transform.m_position);
    float radius = MapObjRadius(model);
    CAaBox extents = { { adt.x - radius, adt.y - radius, adt.z - radius },
        { adt.x + radius, adt.y + radius, adt.z + radius } };

    return InsertMapObj(*tile, model, transform, extents, 0, 0, 0, out, error);
}

bool Clone(const Ref& ref, const Transform& transform, Ref& out, std::string& error) {
    if (!ref.Valid()) {
        error = "nothing selected";
        return false;
    }

    Session::OpenTile* source = Session::Open(ref.m_tileX, ref.m_tileY, error);
    if (!source) {
        return false;
    }

    int32_t tileX = Coords::TileX(transform.m_position);
    int32_t tileY = Coords::TileY(transform.m_position);
    if (!Access::IsValidTile(tileX, tileY)) {
        error = "that spot is off the edge of the map";
        return false;
    }

    bool doodad = ref.m_kind == Kind::Doodad;

    SMDoodadDef doodadEntry{};
    SMMapObjDef mapObjEntry{};
    uint32_t nameId = 0;

    if (doodad) {
        size_t count = 0;
        SMDoodadDef* entries = DocDoodads(*source, count);
        if (!entries || static_cast<size_t>(ref.m_index) >= count || entries[ref.m_index].m_uniqueId != ref.m_uniqueId) {
            error = "that placement is no longer at index " + std::to_string(ref.m_index);
            return false;
        }

        doodadEntry = entries[ref.m_index];
        nameId = doodadEntry.m_nameId;
    } else {
        size_t count = 0;
        SMMapObjDef* entries = DocMapObjs(*source, count);
        if (!entries || static_cast<size_t>(ref.m_index) >= count || entries[ref.m_index].m_uniqueId != ref.m_uniqueId) {
            error = "that placement is no longer at index " + std::to_string(ref.m_index);
            return false;
        }

        mapObjEntry = entries[ref.m_index];
        nameId = mapObjEntry.m_nameId;
    }

    const char* found = DocName(*source, doodad, nameId);
    if (!found) {
        error = "tile " + TileName(ref.m_tileX, ref.m_tileY) + " has no model path for that entry";
        return false;
    }

    std::string model = found;

    Session::OpenTile* tile = Session::Open(tileX, tileY, error);
    if (!tile) {
        return false;
    }

    if (doodad) {
        return InsertDoodad(*tile, model, transform, doodadEntry.m_flags, out, error);
    }

    C3Vector adt = Coords::WorldToAdt(transform.m_position);
    C3Vector shift = VectorMath::Subtract(adt, mapObjEntry.m_position);
    CAaBox extents = { VectorMath::Add(mapObjEntry.m_extents.b, shift),
        VectorMath::Add(mapObjEntry.m_extents.t, shift) };

    return InsertMapObj(*tile, model, transform, extents, mapObjEntry.m_flags, mapObjEntry.m_doodadSet,
        mapObjEntry.m_nameSet, out, error);
}

int32_t ListModels(const C3Vector& center, float radius, std::vector<std::string>& out) {
    out.clear();

    std::vector<Info> nearby;
    ListNear(center, radius, nearby);

    std::sort(nearby.begin(), nearby.end(),
        [&](const Info& a, const Info& b) {
            return BoxDistanceSqXY(a.m_bounds, center) < BoxDistanceSqXY(b.m_bounds, center);
        });

    for (const Info& info : nearby) {
        if (info.m_name.empty()) {
            continue;
        }

        bool seen = false;
        for (const std::string& have : out) {
            if (SameName(have.c_str(), info.m_name.c_str())) {
                seen = true;
                break;
            }
        }

        if (!seen) {
            out.push_back(info.m_name);
        }
    }

    return static_cast<int32_t>(out.size());
}

void DebugRay(const C3Vector& start, const C3Vector& end, std::vector<std::string>& lines) {
    lines.clear();

    char line[256];
    if (!Access::IsActive()) {
        lines.push_back("placements: CMap is not active");
        return;
    }

    C3Vector dir = VectorMath::Subtract(end, start);
    float rayLength = VectorMath::Length(dir);

    struct Candidate {
        bool m_doodad = false;
        float m_distance = 0.0f;
        float m_size = 0.0f;
        bool m_resolved = false;
        Ref m_ref;
    };

    std::vector<Candidate> hits;

    auto sweep = [&](bool doodads, int32_t& live, int32_t& resolved) {
        live = 0;
        resolved = 0;

        ForEachLiveDef(doodads,
            [&](CMapBaseObj* owner) {
                ++live;

                CAaBox box = doodads ? DoodadBounds(static_cast<CMapDoodadDef*>(owner))
                                     : MapObjBounds(static_cast<CMapObjDef*>(owner));

                Ref ref;
                bool ok = doodads ? RefForDoodad(static_cast<CMapDoodadDef*>(owner), ref)
                                  : RefForMapObj(static_cast<CMapObjDef*>(owner), ref);
                if (ok) {
                    ++resolved;
                }

                float t = 0.0f;
                if (!RayHitsBox(start, dir, box, t)) {
                    return;
                }

                Candidate candidate;
                candidate.m_doodad = doodads;
                candidate.m_distance = t * rayLength;
                candidate.m_size = BoxDiagonal(box);
                candidate.m_resolved = ok;
                candidate.m_ref = ref;
                hits.push_back(candidate);
            });
    };

    int32_t liveDoodads = 0;
    int32_t okDoodads = 0;
    int32_t liveMapObjs = 0;
    int32_t okMapObjs = 0;
    sweep(true, liveDoodads, okDoodads);
    sweep(false, liveMapObjs, okMapObjs);

    std::snprintf(line, sizeof(line), "placements: %d live doodads (%d matched), %d live WMOs (%d matched)",
        liveDoodads, okDoodads, liveMapObjs, okMapObjs);
    lines.push_back(line);

    std::snprintf(line, sizeof(line), "  ray crosses %d of them", static_cast<int32_t>(hits.size()));
    lines.push_back(line);

    std::sort(hits.begin(), hits.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.m_distance < b.m_distance;
        });

    size_t show = hits.size() < 8 ? hits.size() : 8;
    for (size_t i = 0; i < show; ++i) {
        const Candidate& hit = hits[i];

        Info info;
        std::string name = hit.m_resolved && Describe(hit.m_ref, info) ? info.m_name : std::string("?");

        std::snprintf(line, sizeof(line), "  %s dist %.1f  box %.1f  %s  %s",
            hit.m_doodad ? "m2 " : "wmo", hit.m_distance, hit.m_size,
            hit.m_resolved ? "matched" : "NO FILE ENTRY", name.c_str());
        lines.push_back(line);
    }
}

}
