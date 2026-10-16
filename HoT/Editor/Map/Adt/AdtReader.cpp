#include <algorithm>
#include <cstdio>
#include <cstring>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Editor/Map/Adt/AdtDocument.h"
#include "HoT/Editor/Map/MapClient.h"

namespace HoT::MapEditor::Adt {

namespace {
struct IffHeader {
    uint32_t m_id;
    uint32_t m_size;
};

constexpr uint32_t kIffSize = 8;
constexpr uint32_t kMcnkHeaderSize = 128;
}

std::string ChunkName(uint32_t id) {
    char text[5] = {};
    text[0] = static_cast<char>((id >> 24) & 0xFF);
    text[1] = static_cast<char>((id >> 16) & 0xFF);
    text[2] = static_cast<char>((id >> 8) & 0xFF);
    text[3] = static_cast<char>(id & 0xFF);

    for (int i = 0; i < 4; ++i) {
        if (text[i] < 32 || text[i] > 126) {
            text[i] = '?';
        }
    }

    return std::string(text);
}

const SubChunk* Mcnk::Find(uint32_t id) const {
    for (const SubChunk& sub : this->m_subs) {
        if (sub.m_id == id) {
            return &sub;
        }
    }
    return nullptr;
}

SubChunk* Mcnk::Find(uint32_t id) {
    for (SubChunk& sub : this->m_subs) {
        if (sub.m_id == id) {
            return &sub;
        }
    }
    return nullptr;
}

Mcnk* AdtDocument::ChunkAt(int32_t chunkX, int32_t chunkY) {
    int32_t index = chunkY * 16 + chunkX;
    if (chunkX < 0 || chunkX > 15 || chunkY < 0 || chunkY > 15 || index >= static_cast<int32_t>(this->m_chunks.size())) {
        return nullptr;
    }

    return &this->m_chunks[index];
}

const Mcnk* AdtDocument::ChunkAt(int32_t chunkX, int32_t chunkY) const {
    return const_cast<AdtDocument*>(this)->ChunkAt(chunkX, chunkY);
}

std::string TilePath(const char* mapName, int32_t tileX, int32_t tileY) {
    char path[260] = {};
    std::snprintf(path, sizeof(path), "World\\Maps\\%s\\%s_%d_%d.adt", mapName, mapName,
        tileX, tileY);
    return std::string(path);
}

std::string TilePath(int32_t tileX, int32_t tileY) {
    return TilePath(Access::MapName(), tileX, tileY);
}

bool ReadFileBytes(const char* path, std::vector<uint8_t>& out, std::string& error) {
    void* data = nullptr;
    uint32_t size = 0;

    if (!SFile::LoadFile(path, &data, &size, 0, nullptr) || !data) {
        error = std::string("SFile could not open ") + path;
        return false;
    }

    if (size == 0 || size == 0xFFFFFFFF) {
        SFile::Unload(data);
        error = std::string("bad file size for ") + path;
        return false;
    }

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    out.assign(bytes, bytes + size);
    SFile::Unload(data);

    return true;
}

bool Read(const uint8_t* bytes, uint32_t size, AdtDocument& out, std::string& error) {
    out = AdtDocument();
    out.m_chunks.resize(256);

    uint32_t pos = 0;
    int32_t mcnkSeen = 0;

    int32_t sizeWithHeader = 0;
    int32_t sizeWithoutHeader = 0;

    while (pos + kIffSize <= size) {
        IffHeader head{};
        std::memcpy(&head, bytes + pos, kIffSize);

        if (static_cast<uint64_t>(pos) + kIffSize + head.m_size > size) {
            char message[160];
            std::snprintf(message, sizeof(message), "%s at 0x%X claims %u bytes, past the end",
                ChunkName(head.m_id).c_str(), pos, head.m_size);
            error = message;
            return false;
        }

        const uint8_t* data = bytes + pos + kIffSize;

        if (head.m_id == kMCNK) {
            if (mcnkSeen >= 256) {
                error = "more than 256 MCNK chunks";
                return false;
            }

            if (head.m_size < kMcnkHeaderSize) {
                error = "MCNK smaller than its header";
                return false;
            }

            Mcnk& chunk = out.m_chunks[mcnkSeen];
            std::memcpy(&chunk.m_header, data, kMcnkHeaderSize);

            uint32_t bounds[9] = {};
            uint32_t boundCount = 0;

            const uint32_t fields[] = { chunk.m_header.m_ofsHeight, chunk.m_header.m_ofsMCCV,
                chunk.m_header.m_ofsNormal, chunk.m_header.m_ofsLayer, chunk.m_header.m_ofsRefs,
                chunk.m_header.m_ofsAlpha, chunk.m_header.m_ofsShadow, chunk.m_header.m_ofsLiquid,
                chunk.m_header.m_ofsSndEmitters };

            for (uint32_t field : fields) {
                if (field < kIffSize + kMcnkHeaderSize) {
                    continue;
                }

                uint32_t at = field - kIffSize;
                if (static_cast<uint64_t>(at) + kIffSize > head.m_size) {
                    continue;
                }

                if (std::find(bounds, bounds + boundCount, at) == bounds + boundCount) {
                    bounds[boundCount++] = at;
                }
            }

            std::sort(bounds, bounds + boundCount);

            uint32_t sub = kMcnkHeaderSize;
            while (sub + kIffSize <= head.m_size) {
                IffHeader subHead{};
                std::memcpy(&subHead, data + sub, kIffSize);

                uint32_t end = head.m_size;
                const uint32_t* next = std::upper_bound(bounds, bounds + boundCount, sub + kIffSize - 1);
                if (next != bounds + boundCount) {
                    end = *next;
                }

                SubChunk entry;
                entry.m_id = subHead.m_id;
                entry.m_declared = subHead.m_size;
                entry.m_data.assign(data + sub + kIffSize, data + end);
                entry.m_originalSize = static_cast<uint32_t>(entry.m_data.size());
                chunk.m_subs.push_back(std::move(entry));

                sub = end;
            }

            if (out.m_hasInfo) {
                uint32_t stated = out.m_info[mcnkSeen].m_size;
                if (stated == head.m_size + kIffSize) {
                    ++sizeWithHeader;
                } else if (stated == head.m_size) {
                    ++sizeWithoutHeader;
                }
            }

            TopChunk top;
            top.m_id = kMCNK;
            top.m_mcnkIndex = mcnkSeen;
            out.m_order.push_back(std::move(top));
            ++mcnkSeen;
        } else {
            if (head.m_id == kMHDR && head.m_size >= sizeof(SMAreaHeader)) {
                std::memcpy(&out.m_header, data, sizeof(SMAreaHeader));
                out.m_hasHeader = true;
            } else if (head.m_id == kMCIN && head.m_size >= sizeof(out.m_info)) {
                std::memcpy(out.m_info, data, sizeof(out.m_info));
                out.m_hasInfo = true;
            }

            TopChunk top;
            top.m_id = head.m_id;
            top.m_data.assign(data, data + head.m_size);
            out.m_order.push_back(std::move(top));
        }

        pos += kIffSize + head.m_size;
    }

    if (pos != size) {
        char message[128];
        std::snprintf(message, sizeof(message), "%u trailing bytes after the last chunk",
            size - pos);
        error = message;
        return false;
    }

    if (!out.m_hasHeader) {
        error = "no MHDR";
        return false;
    }

    out.m_mcinSizeIncludesHeader = sizeWithoutHeader <= sizeWithHeader;
    out.m_chunks.resize(mcnkSeen);
    return true;
}

int32_t ChunkIndexAtOffset(const AdtDocument& doc, uint32_t offset) {
    uint32_t pos = 0;
    for (const TopChunk& top : doc.m_order) {
        uint32_t body = static_cast<uint32_t>(top.m_data.size());
        if (top.m_mcnkIndex >= 0) {
            body = kMcnkHeaderSize;
            for (const SubChunk& sub : doc.m_chunks[top.m_mcnkIndex].m_subs) {
                body += kIffSize + static_cast<uint32_t>(sub.m_data.size());
            }
        }

        uint32_t total = kIffSize + body;
        if (offset < pos + total) {
            return top.m_mcnkIndex;
        }

        pos += total;
    }

    return -1;
}

std::string DescribeOffset(const AdtDocument& doc, uint32_t offset) {
    uint32_t pos = 0;
    for (const TopChunk& top : doc.m_order) {
        uint32_t body = top.m_mcnkIndex >= 0 ? 0 : static_cast<uint32_t>(top.m_data.size());
        if (top.m_mcnkIndex >= 0) {
            const Mcnk& chunk = doc.m_chunks[top.m_mcnkIndex];
            body = kMcnkHeaderSize;
            for (const SubChunk& sub : chunk.m_subs) {
                body += kIffSize + static_cast<uint32_t>(sub.m_data.size());
            }
        }

        uint32_t total = kIffSize + body;
        if (offset < pos + total) {
            uint32_t inner = offset - pos;
            char message[192];

            if (top.m_mcnkIndex < 0) {
                std::snprintf(message, sizeof(message), "%s +0x%X",
                    ChunkName(top.m_id).c_str(), inner);
                return message;
            }

            const Mcnk& chunk = doc.m_chunks[top.m_mcnkIndex];
            if (inner < kIffSize + kMcnkHeaderSize) {
                std::snprintf(message, sizeof(message), "MCNK %d (%d,%d) header +0x%X",
                    top.m_mcnkIndex, chunk.m_header.m_index.x, chunk.m_header.m_index.y,
                    inner - kIffSize);
                return message;
            }

            uint32_t sub = kIffSize + kMcnkHeaderSize;
            for (const SubChunk& entry : chunk.m_subs) {
                uint32_t span = kIffSize + static_cast<uint32_t>(entry.m_data.size());
                if (inner < sub + span) {
                    std::snprintf(message, sizeof(message), "MCNK %d (%d,%d) %s +0x%X",
                        top.m_mcnkIndex, chunk.m_header.m_index.x, chunk.m_header.m_index.y,
                        ChunkName(entry.m_id).c_str(), inner - sub - kIffSize);
                    return message;
                }
                sub += span;
            }

            std::snprintf(message, sizeof(message), "MCNK %d +0x%X", top.m_mcnkIndex, inner);
            return message;
        }

        pos += total;
    }

    return "past the end";
}

}
