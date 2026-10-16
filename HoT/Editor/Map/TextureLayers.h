#ifndef HOT_EDITOR_MAP_TEXTURELAYERS_H
#define HOT_EDITOR_MAP_TEXTURELAYERS_H

#include <cstdint>
#include <string>
#include <vector>
#include "Engine/Source/Tempest/C3Vector.h"

namespace HoT::MapEditor::TextureLayers {

constexpr int32_t kMaxLayers = 4;

int32_t Add(const C3Vector& center, float radius, const char* texture, int32_t& skipped,
    std::string& error);

int32_t Remove(const C3Vector& center, float radius, int32_t layer, int32_t& skipped,
    std::string& error);

bool ListTileTextures(int32_t tileX, int32_t tileY, std::vector<std::string>& out,
    std::string& error);

void Reset();

}

#endif
