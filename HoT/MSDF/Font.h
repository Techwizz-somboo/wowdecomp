#ifndef HOT_MSDF_FONT_H
#define HOT_MSDF_FONT_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <freetype/freetype.h>
#include "Engine/Source/Gx/Types.h"
#include "HoT/MSDF/Types.h"

class CGxTex;

namespace msdfgen {
class FontHandle;
class FreetypeHandle;
}

namespace HoT::MSDF {

class Cache;

// msdfgen's own FreeType library, null when MSDF is off.
msdfgen::FreetypeHandle* FreetypeLibrary();
bool AllowUnsafeFonts();

// Distance field glyphs of one FreeType face, packed into up to four 1024x1024 atlas pages.
class Font {
    public:
    Font(FT_Face face, const uint8_t* fontData, size_t dataSize);
    ~Font();

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    bool IsValid() const { return this->m_valid; }
    uint32_t EvictionCount() const { return this->m_evictionCount; }
    CGxTex* PageTexture(uint32_t page) const;

    const GlyphMetrics* GetGlyph(uint32_t codepoint);
    void FlushDirtyPages();

    // Frees the atlas, the cache and the msdfgen face, the object stays as an empty shell.
    void Release();

    static bool GenerateMSDF(std::vector<uint8_t>& outData, msdfgen::FontHandle* font, uint32_t codepoint, int32_t sdfW, int32_t sdfH);

    private:
    struct AtlasPage {
        CGxTex* m_texture = nullptr;
        std::vector<uint8_t> m_pixels;
        int32_t m_nextX = ATLAS_GUTTER;
        int32_t m_nextY = ATLAS_GUTTER;
        int32_t m_rowHeight = 0;
        std::vector<uint32_t> m_codepoints;
        bool m_dirty = false;
        int32_t m_dirtyMinX = 0;
        int32_t m_dirtyMinY = 0;
        int32_t m_dirtyMaxX = 0;
        int32_t m_dirtyMaxY = 0;

        void Clear();
        void MarkDirty(int32_t minX, int32_t minY, int32_t maxX, int32_t maxY);
    };

    struct HotGlyph {
        uint32_t m_codepoint = 0xFFFFFFFF;
        const GlyphMetrics* m_metrics = nullptr;
    };

    static void TextureCallback(EGxTexCommand command, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevel, void* userArg, uint32_t& texelStrideInBytes, const void*& texels);

    const GlyphMetrics* FindInCache(uint32_t codepoint);
    const GlyphMetrics* LoadAndCache(uint32_t codepoint);
    void UpdateHotCache(uint32_t codepoint, const GlyphMetrics* metrics);
    void ClearHotCache();
    bool CreateAtlasPage();
    bool UploadGlyphToAtlas(GlyphMetrics& metrics, uint32_t codepoint);

    FT_Face m_ftFace;
    msdfgen::FontHandle* m_msdfFont = nullptr;
    bool m_valid = false;
    uint16_t m_oldestPage = 0;
    uint32_t m_evictionCount = 0;
    std::unique_ptr<Cache> m_cache;
    std::vector<std::unique_ptr<AtlasPage>> m_atlasPages;
    HotGlyph m_hotCache[64];
    std::unordered_map<uint32_t, GlyphMetrics> m_glyphPool;
};

}

#endif
