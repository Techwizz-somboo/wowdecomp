#ifndef HOT_EDITOR_MAP_ADT_ALPHAMAP_H
#define HOT_EDITOR_MAP_ADT_ALPHAMAP_H

#include <cstdint>
#include <string>
#include <vector>
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Adt {

constexpr int32_t kAlphaSide = 64;
constexpr int32_t kAlphaTexels = kAlphaSide * kAlphaSide;

constexpr uint32_t kLayerUseAlpha = 0x100;
constexpr uint32_t kLayerCompressed = 0x200;

// Without this MCNK flag the client copies texel row and column 62 over 63 when it loads a mask.
constexpr uint32_t kChunkDoNotFixAlpha = 0x8000;

struct AlphaFormat {
    bool m_bigAlpha = false;
    bool m_doNotFix = false;
};

struct AlphaMap {
    uint8_t m_texel[kAlphaTexels] = {};
};

// Masks decoded to 8 bits a texel whatever the file holds, one per layer with layer 0 left unused.
struct ChunkAlpha {
    std::vector<SMLayer> m_layers;

    std::vector<AlphaMap> m_maps;
};

bool ReadChunkAlpha(const Mcnk& mcnk, const AlphaFormat& format, ChunkAlpha& out,
    std::string& error);

bool WriteChunkAlpha(const ChunkAlpha& in, const AlphaFormat& format, Mcnk& mcnk,
    std::string& error);

constexpr uint32_t AlphaStride(bool bigAlpha) {
    return bigAlpha ? 4096u : 2048u;
}

bool ReadMapFlags(const char* mapName, uint32_t& flags, std::string& error);

AlphaFormat FormatFor(const Mcnk& mcnk, uint32_t mapFlags);

}

#endif
