#ifndef HOT_EDITOR_MAP_PLACEMENTS_H
#define HOT_EDITOR_MAP_PLACEMENTS_H

#include <cstdint>
#include <string>
#include <vector>
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Placements {

enum class Kind {
    Doodad,
    MapObj,
};

struct Ref {
    Kind m_kind = Kind::Doodad;
    int32_t m_tileX = -1;
    int32_t m_tileY = -1;
    int32_t m_index = -1;
    uint32_t m_uniqueId = 0;

    bool Valid() const {
        return this->m_index >= 0 && this->m_tileX >= 0 && this->m_tileY >= 0;
    }
};

struct Transform {
    C3Vector m_position{};
    C3Vector m_rotation{};
    float m_scale = 1.0f;
};

struct Info {
    Ref m_ref;
    std::string m_name;
    Transform m_transform;
    CAaBox m_bounds{};
    float m_distance = 0.f;
    float m_originDistance = 0.f;
    bool m_live = false;
};

bool Pick(const C3Vector& start, const C3Vector& end, Ref& out);

bool Describe(const Ref& ref, Info& out);

bool Resolve(Ref& ref);

bool SetTransform(Ref& ref, const Transform& transform, std::string& error);

bool Preview(const Ref& ref, const Transform& transform);

bool Remove(const Ref& ref, std::string& error);

int32_t ListNear(const C3Vector& center, float radius, std::vector<Info>& out);

bool KindForPath(const char* path, Kind& out);

bool Add(Kind kind, const char* path, const Transform& transform, Ref& out, std::string& error);

bool Clone(const Ref& ref, const Transform& transform, Ref& out, std::string& error);

int32_t ListModels(const C3Vector& center, float radius, std::vector<std::string>& out);

void DebugRay(const C3Vector& start, const C3Vector& end, std::vector<std::string>& lines);

}

#endif
