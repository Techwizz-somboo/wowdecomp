#ifndef HOT_EDITOR_MAP_ADT_ADTSTORE_H
#define HOT_EDITOR_MAP_ADT_ADTSTORE_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace HoT::MapEditor::Adt {

std::filesystem::path DiskTilePath(const char* mapName, int32_t tileX, int32_t tileY);

std::filesystem::path BackupTilePath(const char* mapName, int32_t tileX, int32_t tileY);

bool SaveTile(const char* mapName, int32_t tileX, int32_t tileY,
    const std::vector<uint8_t>& bytes, std::string& error);

bool RestoreTile(const char* mapName, int32_t tileX, int32_t tileY, std::string& error);

}

#endif
