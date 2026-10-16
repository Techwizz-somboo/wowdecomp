#ifndef HOT_EDITOR_MAP_ADT_ADTDOCUMENT_H
#define HOT_EDITOR_MAP_ADT_ADTDOCUMENT_H

#include <cstdint>
#include <string>
#include <vector>
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Adt {

constexpr uint32_t ChunkId(const char (&text)[5]) {
    return static_cast<uint32_t>(static_cast<uint8_t>(text[0])) << 24 | static_cast<uint32_t>(static_cast<uint8_t>(text[1])) << 16 | static_cast<uint32_t>(static_cast<uint8_t>(text[2])) << 8 | static_cast<uint32_t>(static_cast<uint8_t>(text[3]));
}

constexpr uint32_t kMVER = ChunkId("MVER");
constexpr uint32_t kMHDR = ChunkId("MHDR");
constexpr uint32_t kMCIN = ChunkId("MCIN");
constexpr uint32_t kMTEX = ChunkId("MTEX");
constexpr uint32_t kMMDX = ChunkId("MMDX");
constexpr uint32_t kMMID = ChunkId("MMID");
constexpr uint32_t kMWMO = ChunkId("MWMO");
constexpr uint32_t kMWID = ChunkId("MWID");
constexpr uint32_t kMDDF = ChunkId("MDDF");
constexpr uint32_t kMODF = ChunkId("MODF");
constexpr uint32_t kMFBO = ChunkId("MFBO");
constexpr uint32_t kMH2O = ChunkId("MH2O");
constexpr uint32_t kMTXF = ChunkId("MTXF");
constexpr uint32_t kMCNK = ChunkId("MCNK");

constexpr uint32_t kMCVT = ChunkId("MCVT");
constexpr uint32_t kMCNR = ChunkId("MCNR");
constexpr uint32_t kMCLY = ChunkId("MCLY");
constexpr uint32_t kMCRF = ChunkId("MCRF");
constexpr uint32_t kMCAL = ChunkId("MCAL");
constexpr uint32_t kMCSH = ChunkId("MCSH");
constexpr uint32_t kMCSE = ChunkId("MCSE");
constexpr uint32_t kMCLQ = ChunkId("MCLQ");
constexpr uint32_t kMCCV = ChunkId("MCCV");

std::string ChunkName(uint32_t id);

// MCNR carries 13 bytes of padding after its 435 bytes of normals that its size does not count.
constexpr uint32_t kMcnrPadding = 13;

constexpr int32_t kMcvtHeights = 9 * 9 + 8 * 8;

struct SubChunk {
    uint32_t m_id = 0;
    std::vector<uint8_t> m_data;
    uint32_t m_declared = 0;
    uint32_t m_originalSize = 0;

    bool Resized() const {
        return this->m_data.size() != this->m_originalSize;
    }

    // An untouched chunk keeps its declared size, so files with odd sizes round trip byte for byte.
    uint32_t DeclaredSize() const {
        if (!this->Resized()) {
            return this->m_declared;
        }

        uint32_t size = static_cast<uint32_t>(this->m_data.size());

        if (this->m_id == kMCNR && size >= kMcnrPadding) {
            size -= kMcnrPadding;
        }

        return size;
    }
};

struct Mcnk {
    SMChunk m_header{};
    std::vector<SubChunk> m_subs;

    const SubChunk* Find(uint32_t id) const;
    SubChunk* Find(uint32_t id);
};

struct TopChunk {
    uint32_t m_id = 0;
    std::vector<uint8_t> m_data;
    int32_t m_mcnkIndex = -1;
};

struct AdtDocument {
    std::vector<TopChunk> m_order;
    std::vector<Mcnk> m_chunks;

    SMAreaHeader m_header{};
    bool m_hasHeader = false;

    SMChunkInfo m_info[256]{};
    bool m_hasInfo = false;

    // Writers disagree on whether MCIN sizes count the MCNK header, the reader votes on it.
    bool m_mcinSizeIncludesHeader = true;

    Mcnk* ChunkAt(int32_t chunkX, int32_t chunkY);
    const Mcnk* ChunkAt(int32_t chunkX, int32_t chunkY) const;
};

bool ReadFileBytes(const char* path, std::vector<uint8_t>& out, std::string& error);

bool Read(const uint8_t* bytes, uint32_t size, AdtDocument& out, std::string& error);
bool Write(const AdtDocument& doc, std::vector<uint8_t>& out, std::string& error);

std::string TilePath(int32_t tileX, int32_t tileY);
std::string TilePath(const char* mapName, int32_t tileX, int32_t tileY);

std::string DescribeOffset(const AdtDocument& doc, uint32_t offset);

int32_t ChunkIndexAtOffset(const AdtDocument& doc, uint32_t offset);

}

#endif
