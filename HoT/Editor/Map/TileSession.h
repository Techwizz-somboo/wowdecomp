#ifndef HOT_EDITOR_MAP_TILESESSION_H
#define HOT_EDITOR_MAP_TILESESSION_H

#include <cstdint>
#include <string>
#include <vector>
#include "HoT/Editor/Map/Adt/AdtDocument.h"

namespace HoT::MapEditor::Session {

// A tile file parsed into a document that live edits are mirrored into, so a save can write them out.
struct OpenTile {
    int32_t m_tileX = -1;
    int32_t m_tileY = -1;
    Adt::AdtDocument m_doc;
    bool m_dirty = false;
};

OpenTile* Open(int32_t tileX, int32_t tileY, std::string& error);

OpenTile* Find(int32_t tileX, int32_t tileY);

std::vector<OpenTile*> AllOpen();

int32_t DirtyCount();

bool SaveDirty(int32_t& saved, std::string& error);

bool SaveOne(int32_t tileX, int32_t tileY, std::string& error);

int32_t RevertAll();

void CloseAll();

const char* MapName();
void OnMapChanged();

}

#endif
