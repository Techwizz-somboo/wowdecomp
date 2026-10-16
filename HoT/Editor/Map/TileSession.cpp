#include "HoT/Editor/Map/TileSession.h"
#include <cstring>
#include <memory>
#include "HoT/Editor/Map/Adt/AdtStore.h"
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Session {

namespace {
std::vector<std::unique_ptr<OpenTile>>& Tiles() {
    static std::vector<std::unique_ptr<OpenTile>> tiles;
    return tiles;
}

std::string& CurrentMap() {
    static std::string name;
    return name;
}

void DropIfMapChanged() {
    const char* now = Access::MapName();
    if (CurrentMap() == now) {
        return;
    }

    Tiles().clear();
    CurrentMap() = now;
}
}

const char* MapName() {
    return CurrentMap().c_str();
}

void OnMapChanged() {
    Tiles().clear();
    CurrentMap().clear();
}

OpenTile* Find(int32_t tileX, int32_t tileY) {
    DropIfMapChanged();

    for (const std::unique_ptr<OpenTile>& tile : Tiles()) {
        if (tile->m_tileX == tileX && tile->m_tileY == tileY) {
            return tile.get();
        }
    }

    return nullptr;
}

OpenTile* Open(int32_t tileX, int32_t tileY, std::string& error) {
    if (OpenTile* existing = Find(tileX, tileY)) {
        return existing;
    }

    std::string path = Adt::TilePath(CurrentMap().c_str(), tileX, tileY);

    std::vector<uint8_t> bytes;
    if (!Adt::ReadFileBytes(path.c_str(), bytes, error)) {
        return nullptr;
    }

    auto tile = std::make_unique<OpenTile>();
    tile->m_tileX = tileX;
    tile->m_tileY = tileY;

    if (!Adt::Read(bytes.data(), static_cast<uint32_t>(bytes.size()), tile->m_doc, error)) {
        return nullptr;
    }

    Tiles().push_back(std::move(tile));
    return Tiles().back().get();
}

std::vector<OpenTile*> AllOpen() {
    DropIfMapChanged();

    std::vector<OpenTile*> out;
    out.reserve(Tiles().size());
    for (const std::unique_ptr<OpenTile>& tile : Tiles()) {
        out.push_back(tile.get());
    }

    return out;
}

int32_t DirtyCount() {
    int32_t count = 0;
    for (const std::unique_ptr<OpenTile>& tile : Tiles()) {
        count += tile->m_dirty ? 1 : 0;
    }

    return count;
}

bool SaveDirty(int32_t& saved, std::string& error) {
    saved = 0;

    for (std::unique_ptr<OpenTile>& tile : Tiles()) {
        if (!tile->m_dirty) {
            continue;
        }

        std::vector<uint8_t> bytes;
        if (!Adt::Write(tile->m_doc, bytes, error)) {
            return false;
        }

        if (!Adt::SaveTile(CurrentMap().c_str(), tile->m_tileX, tile->m_tileY, bytes, error)) {
            return false;
        }

        tile->m_dirty = false;
        ++saved;
    }

    return true;
}

bool SaveOne(int32_t tileX, int32_t tileY, std::string& error) {
    OpenTile* tile = Find(tileX, tileY);
    if (!tile) {
        error = "tile " + std::to_string(tileX) + "_" + std::to_string(tileY) + " is not open";
        return false;
    }

    std::vector<uint8_t> bytes;
    if (!Adt::Write(tile->m_doc, bytes, error)) {
        return false;
    }

    if (!Adt::SaveTile(CurrentMap().c_str(), tileX, tileY, bytes, error)) {
        return false;
    }

    tile->m_dirty = false;
    return true;
}

int32_t RevertAll() {
    int32_t reverted = 0;
    std::string error;

    for (const std::unique_ptr<OpenTile>& tile : Tiles()) {
        if (Adt::RestoreTile(CurrentMap().c_str(), tile->m_tileX, tile->m_tileY, error)) {
            ++reverted;
        }

        Access::ReloadTile(tile->m_tileX, tile->m_tileY);
    }

    Tiles().clear();
    return reverted;
}

void CloseAll() {
    Tiles().clear();
}

}
