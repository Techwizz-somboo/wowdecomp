#ifndef HOT_MSDF_CACHE_H
#define HOT_MSDF_CACHE_H

#include <cstdint>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "HoT/MSDF/Types.h"

namespace HoT::MSDF {

// On disk glyph cache of one font face, FontCache/<family>_<style>_s<size>_sp<spread>.
class Cache {
    public:
    static constexpr const char* CACHE_DIR = "FontCache";

    Cache(const uint8_t* fontData, size_t dataSize, const char* familyName, const char* styleName, uint32_t sdfRenderSize, uint32_t sdfSpread);
    ~Cache();

    Cache(const Cache&) = delete;
    Cache& operator=(const Cache&) = delete;

    bool TryLoadGlyph(uint32_t codepoint, GlyphMetrics& outMetrics);
    bool StoreGlyph(GlyphToStore&& glyph);
    size_t GetManifestSize();
    bool FlushPendingWrites();

    private:
    static const size_t WRITE_BATCH_SIZE = 64;
    static const size_t MAX_SAFE_ALLOCATION = 32 * 1024 * 1024;

    using ManifestMap = std::unordered_map<uint32_t, ManifestEntry>;

    static uint32_t GetBlockId(uint32_t codepoint);
    static std::string GetCacheBasePath(const char* familyName, const char* styleName, uint32_t sdfRenderSize, uint32_t sdfSpread);
    static std::string SanitizeName(std::string_view name);
    static bool LoadManifestJournal(const std::filesystem::path& journalPath, ManifestMap& outMap, size_t& outEntriesApplied);

    bool LoadManifest();
    bool SaveManifest(bool isLocked = false);
    bool LoadManifestFromFile(const std::filesystem::path& path, ManifestMap& outMap) const;
    bool AppendManifestJournal(const std::vector<ManifestEntry>& entries);
    void BuildBlockLockPath(uint32_t blockId, std::filesystem::path& outPath) const;
    void BuildBlockPath(uint32_t blockId, std::filesystem::path& outPath) const;
    bool WriteBlockFile(uint32_t blockId, std::vector<GlyphToStore*>& pending, std::vector<ManifestEntry>& outEntries);
    void CleanupOrphans() const;

    std::filesystem::path m_basePath;
    std::filesystem::path m_manifestPath;
    std::filesystem::path m_manifestLockPath;
    std::filesystem::path m_manifestJournalPath;
    CacheKey m_key;
    ManifestMap m_manifest;
    bool m_enabled = false;
    bool m_manifestLoaded = false;
    uint32_t m_fontId = 0xFFFFFFFF;
    std::deque<GlyphToStore> m_pendingWrites;
    std::unordered_map<uint32_t, BlockWrap> m_blockWrap;
};

}

#endif
