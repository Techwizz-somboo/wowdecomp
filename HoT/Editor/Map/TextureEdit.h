#ifndef HOT_EDITOR_MAP_TEXTUREEDIT_H
#define HOT_EDITOR_MAP_TEXTUREEDIT_H

#include <cstdint>
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Editor/Map/EditorUndo.h"

namespace HoT::Editor::Map::TextureEdit {

struct Brush {
    float m_radius;
    float m_hardness;
    float m_pressure;
    float m_opacity;
};

extern Brush s_brush;

// Lists every file in the client's open archives, the callback returns 1 to continue.
void ListArchives(int32_t (*callback)(const char* fileName, void* param), void* param);
void SetTexture(const char* name);
const char* GetTexture();
void Paint(const C3Vector& pos, float elapsedSec);
void EndStroke();
bool PickTexture(const C3Vector& pos);
uint32_t GetChunkTextures(const C3Vector& pos, const char** names, uint32_t maxNames);
uint32_t GetTilesetCount();
const char* GetTileset(uint32_t index);
bool Capture(int32_t row, int32_t col, EditorUndo::TextureSnapshot& snapshot);
void Restore(const EditorUndo::TextureSnapshot& snapshot);
void FlushRenderRebuilds();
void RequestRebuild(int32_t row, int32_t col);
void Reset();
void FreeRetired();
void Rebind();
void ResetArea(int32_t areaX, int32_t areaY);
bool GetSaveData(int32_t row, int32_t col, const SMLayer** layers, uint32_t* layerCount, const uint8_t** alpha, uint32_t* alphaSize);

}

#endif
