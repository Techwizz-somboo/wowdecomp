#ifndef HOT_MSDF_TYPES_H
#define HOT_MSDF_TYPES_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <vector>

namespace HoT::MSDF {

static const uint32_t ATLAS_SIZE = 1024;
static const uint32_t ATLAS_GUTTER = 12;
static const uint32_t MAX_ATLAS_PAGES = 4;
static const uint32_t SDF_RENDER_SIZE = 64;
static const uint32_t SDF_SPREAD = 8;
static const size_t CJK_CACHE_THRESHOLD = 10000;

static const size_t BLOCK_SIZE = 512;
static const uint32_t BLOCK_MAGIC = 0x4D534442;
static const uint32_t MANIFEST_MAGIC = 0x4D534D46;
static const uint32_t CACHE_VERSION = 1;

struct BlockKey {
    uint32_t m_fontId = 0xFFFFFFFF;
    uint32_t m_blockId = 0xFFFFFFFF;

    BlockKey() = default;
    BlockKey(uint32_t fontId, uint32_t blockId) : m_fontId(fontId), m_blockId(blockId) {}

    bool operator==(const BlockKey& other) const {
        return this->m_fontId == other.m_fontId && this->m_blockId == other.m_blockId;
    }

    uint64_t Pack() const {
        return (static_cast<uint64_t>(this->m_fontId) << 32) | this->m_blockId;
    }
};

struct BlockKeyHash {
    size_t operator()(const BlockKey& key) const noexcept {
        return std::hash<uint64_t>()(key.Pack());
    }
};

struct BlockWrap {
    BlockKey m_key;
    std::filesystem::path m_path;
};

struct CacheKey {
    uint32_t m_sdfRenderSize = 0;
    uint32_t m_sdfSpread = 0;

    bool operator==(const CacheKey& other) const {
        return this->m_sdfRenderSize == other.m_sdfRenderSize && this->m_sdfSpread == other.m_sdfSpread;
    }
};

#pragma pack(push, 1)
struct ManifestHeader {
    uint32_t m_magic;
    uint32_t m_version;
    CacheKey m_key;
    uint32_t m_entryCount;
    uint32_t m_pad;
};

struct ManifestEntry {
    uint32_t m_codepoint;
    uint32_t m_blockId;
};

struct alignas(64) BlockFileHeader {
    uint32_t m_magic;
    uint32_t m_version;
    uint32_t m_blockId;
    uint32_t m_entryCount;
};

struct alignas(64) GlyphEntry {
    uint32_t m_codepoint;
    uint16_t m_width;
    uint16_t m_height;
    int32_t m_bitmapTop;
    int32_t m_bitmapLeft;
    uint32_t m_dataOffset;
    uint32_t m_dataSize;
};
#pragma pack(pop)

static_assert(sizeof(ManifestHeader) == 24);
static_assert(sizeof(ManifestEntry) == 8);
static_assert(sizeof(BlockFileHeader) == 64);
static_assert(sizeof(GlyphEntry) == 64);

struct GlyphMetrics {
    uint16_t m_width = 0;
    uint16_t m_height = 0;
    int32_t m_bitmapTop = 0;
    int32_t m_bitmapLeft = 0;
    float m_u0 = 0.0f;
    float m_v0 = 0.0f;
    float m_u1 = 0.0f;
    float m_v1 = 0.0f;
    uint16_t m_page = 0;
    const uint8_t* m_pixels = nullptr;
};

struct GlyphToStore {
    uint32_t m_codepoint = 0;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
    int32_t m_bitmapTop = 0;
    int32_t m_bitmapLeft = 0;
    std::vector<uint8_t> m_pixels;
    uint32_t m_dataSize = 0;
};

}

#endif
