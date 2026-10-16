#include <cctype>
#include <cstring>
#include <list>
#include <mutex>
#include <string>
#include <unordered_map>
#include "HoT/Character/HD/HDBlpImage.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Core/Log.h"

namespace HoT::HD {

static const size_t HEADER_SIZE = 4 + 4 + 4 + 4 + 4 + 64 + 64 + 1024;
static const size_t CACHE_BUDGET_BYTES = 96u * 1024u * 1024u;

enum Encoding : uint8_t {
    ENCODING_PALETTE = 1,
    ENCODING_DXT = 2,
    ENCODING_RAW = 3,
};

struct Header {
    uint8_t m_encoding;
    uint8_t m_alphaDepth;
    uint8_t m_alphaEncoding;
    uint8_t m_hasMips;
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_mipOffset;
    uint32_t m_mipSize;
    const uint8_t* m_palette;
};

struct CacheEntry {
    std::shared_ptr<const BlpImage> m_image;
    std::list<std::string>::iterator m_order;
};

static std::mutex s_cacheMutex;
static std::unordered_map<std::string, CacheEntry> s_cache;
static std::list<std::string> s_order;
static size_t s_cacheBytes = 0;

static uint32_t Pack(uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

static void Expand565(uint16_t c, uint32_t& r, uint32_t& g, uint32_t& b) {
    r = (c >> 11) & 0x1F;
    g = (c >> 5) & 0x3F;
    b = c & 0x1F;
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
}

static uint32_t PaletteAt(const Header& h, uint8_t index) {
    uint32_t value;
    memcpy(&value, h.m_palette + index * 4, sizeof(value));
    return value;
}

static void DecodeColorBlock(const uint8_t* block, bool dxt1, uint32_t colors[4], uint32_t alphas[4]) {
    uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
    uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));
    uint32_t r0, g0, b0, r1, g1, b1;
    Expand565(c0, r0, g0, b0);
    Expand565(c1, r1, g1, b1);
    colors[0] = Pack(r0, g0, b0, 255);
    colors[1] = Pack(r1, g1, b1, 255);
    alphas[0] = alphas[1] = 255;

    if (!dxt1 || c0 > c1) {
        colors[2] = Pack((2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255);
        colors[3] = Pack((r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255);
        alphas[2] = alphas[3] = 255;
    } else {
        colors[2] = Pack((r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255);
        colors[3] = 0;
        alphas[2] = 255;
        alphas[3] = 0;
    }
}

static bool DecodeDxt(const Header& h, const uint8_t* data, BlpImage& out) {
    bool dxt1 = h.m_alphaEncoding == 0;
    bool dxt3 = h.m_alphaEncoding == 1;
    size_t blockSize = dxt1 ? 8 : 16;
    uint32_t bw = (h.m_width + 3) / 4;
    uint32_t bh = (h.m_height + 3) / 4;

    if (static_cast<size_t>(bw) * bh * blockSize > h.m_mipSize) {
        return false;
    }

    const uint8_t* block = data;

    for (uint32_t by = 0; by < bh; by++) {
        for (uint32_t bx = 0; bx < bw; bx++, block += blockSize) {
            uint32_t colors[4];
            uint32_t alphas[4];
            const uint8_t* colorBlock = dxt1 ? block : block + 8;
            DecodeColorBlock(colorBlock, dxt1, colors, alphas);
            uint32_t alphaTable[8] = {};

            if (!dxt1 && !dxt3) {
                uint32_t a0 = block[0];
                uint32_t a1 = block[1];
                alphaTable[0] = a0;
                alphaTable[1] = a1;

                if (a0 > a1) {
                    for (uint32_t i = 1; i < 7; i++) {
                        alphaTable[i + 1] = ((7 - i) * a0 + i * a1) / 7;
                    }
                } else {
                    for (uint32_t i = 1; i < 5; i++) {
                        alphaTable[i + 1] = ((5 - i) * a0 + i * a1) / 5;
                    }

                    alphaTable[6] = 0;
                    alphaTable[7] = 255;
                }
            }

            uint32_t indices = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | (static_cast<uint32_t>(colorBlock[7]) << 24);
            uint64_t alphaBits = 0;

            if (!dxt1 && !dxt3) {
                for (int32_t i = 0; i < 6; i++) {
                    alphaBits |= static_cast<uint64_t>(block[2 + i]) << (8 * i);
                }
            }

            for (uint32_t py = 0; py < 4; py++) {
                uint32_t y = by * 4 + py;

                if (y >= h.m_height) {
                    break;
                }

                for (uint32_t px = 0; px < 4; px++) {
                    uint32_t x = bx * 4 + px;

                    if (x >= h.m_width) {
                        break;
                    }

                    uint32_t texel = py * 4 + px;
                    uint32_t ci = (indices >> (texel * 2)) & 3;
                    uint32_t rgb = colors[ci] & 0x00FFFFFFu;
                    uint32_t a;

                    if (dxt1) {
                        a = h.m_alphaDepth ? alphas[ci] : 255;
                    } else if (dxt3) {
                        uint32_t nibble = (block[texel / 2] >> ((texel & 1) * 4)) & 0xF;
                        a = nibble * 17;
                    } else {
                        a = alphaTable[(alphaBits >> (texel * 3)) & 7];
                    }

                    out.m_pixels[y * h.m_width + x] = rgb | (a << 24);
                }
            }
        }
    }

    return true;
}

static bool DecodePalette(const Header& h, const uint8_t* data, BlpImage& out) {
    size_t count = static_cast<size_t>(h.m_width) * h.m_height;
    size_t alphaBytes = 0;

    if (h.m_alphaDepth == 1) {
        alphaBytes = (count + 7) / 8;
    } else if (h.m_alphaDepth == 4) {
        alphaBytes = (count + 1) / 2;
    } else if (h.m_alphaDepth == 8) {
        alphaBytes = count;
    }

    if (count + alphaBytes > h.m_mipSize) {
        return false;
    }

    const uint8_t* alpha = data + count;

    for (size_t i = 0; i < count; i++) {
        uint32_t rgb = PaletteAt(h, data[i]) & 0x00FFFFFFu;
        uint32_t a = 255;

        if (h.m_alphaDepth == 1) {
            a = (alpha[i / 8] >> (i & 7)) & 1 ? 255 : 0;
        } else if (h.m_alphaDepth == 4) {
            a = ((alpha[i / 2] >> ((i & 1) * 4)) & 0xF) * 17;
        } else if (h.m_alphaDepth == 8) {
            a = alpha[i];
        }

        out.m_pixels[i] = rgb | (a << 24);
    }

    return true;
}

static bool DecodeRaw(const Header& h, const uint8_t* data, BlpImage& out) {
    size_t count = static_cast<size_t>(h.m_width) * h.m_height;

    if (count * 4 > h.m_mipSize) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t v;
        memcpy(&v, data + i * 4, 4);

        if (h.m_alphaDepth == 0) {
            v |= 0xFF000000u;
        }

        out.m_pixels[i] = v;
    }

    return true;
}

static std::string CacheKey(const char* path) {
    std::string key(path);

    for (char& c : key) {
        if (c == '/') {
            c = '\\';
        } else {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
    }

    return key;
}

static void Evict() {
    while (s_cacheBytes > CACHE_BUDGET_BYTES && !s_order.empty()) {
        auto it = s_cache.find(s_order.back());

        if (it != s_cache.end()) {
            s_cacheBytes -= it->second.m_image->m_pixels.size() * sizeof(uint32_t);
            s_cache.erase(it);
        }

        s_order.pop_back();
    }
}

static bool ReadWholeFile(const char* path, std::vector<uint8_t>& out) {
    SFileImpl* file = nullptr;

    if (!SFile::OpenEx(nullptr, path, 0, &file) || !file) {
        return false;
    }

    uint32_t size = SFileGetFileSize(file, nullptr);
    bool ok = false;

    if (size != 0 && size != 0xFFFFFFFFu) {
        out.resize(size);
        uint32_t read = 0;
        ok = SFileReadFile(file, out.data(), size, &read, nullptr, 0) != 0 && read == size;
    }

    SFileCloseFile(file);
    return ok;
}

bool DecodeBlp(const uint8_t* data, size_t size, BlpImage& out) {
    if (!data || size < HEADER_SIZE || memcmp(data, "BLP2", 4) != 0) {
        return false;
    }

    Header h {};
    h.m_encoding = data[8];
    h.m_alphaDepth = data[9];
    h.m_alphaEncoding = data[10];
    h.m_hasMips = data[11];
    memcpy(&h.m_width, data + 12, 4);
    memcpy(&h.m_height, data + 16, 4);
    memcpy(&h.m_mipOffset, data + 20, 4);
    memcpy(&h.m_mipSize, data + 20 + 64, 4);
    h.m_palette = data + 20 + 128;

    if (h.m_width == 0 || h.m_height == 0 || h.m_width > 8192 || h.m_height > 8192) {
        return false;
    }

    if (static_cast<size_t>(h.m_mipOffset) + h.m_mipSize > size || h.m_mipSize == 0) {
        return false;
    }

    out.m_width = h.m_width;
    out.m_height = h.m_height;
    out.m_hasAlpha = h.m_alphaDepth != 0;
    out.m_pixels.assign(static_cast<size_t>(h.m_width) * h.m_height, 0);
    const uint8_t* mip = data + h.m_mipOffset;

    switch (h.m_encoding) {
        case ENCODING_PALETTE: {
            return DecodePalette(h, mip, out);
        }

        case ENCODING_DXT: {
            return DecodeDxt(h, mip, out);
        }

        case ENCODING_RAW: {
            return DecodeRaw(h, mip, out);
        }

        default: {
            return false;
        }
    }
}

std::shared_ptr<const BlpImage> LoadBlp(const char* path) {
    if (!path || !*path) {
        return nullptr;
    }

    std::string key = CacheKey(path);

    {
        std::lock_guard<std::mutex> lock(s_cacheMutex);
        auto it = s_cache.find(key);

        if (it != s_cache.end()) {
            s_order.splice(s_order.begin(), s_order, it->second.m_order);
            return it->second.m_image;
        }
    }

    std::vector<uint8_t> bytes;

    if (!ReadWholeFile(path, bytes)) {
        LOG_WARN << "hd characters: cannot read texture " << path;
        return nullptr;
    }

    auto image = std::make_shared<BlpImage>();

    if (!DecodeBlp(bytes.data(), bytes.size(), *image)) {
        LOG_WARN << "hd characters: cannot decode texture " << path;
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(s_cacheMutex);
    auto it = s_cache.find(key);

    if (it != s_cache.end()) {
        s_order.splice(s_order.begin(), s_order, it->second.m_order);
        return it->second.m_image;
    }

    s_order.push_front(key);
    s_cache[key] = CacheEntry { image, s_order.begin() };
    s_cacheBytes += image->m_pixels.size() * sizeof(uint32_t);
    Evict();

    return image;
}

void ClearBlpCache() {
    std::lock_guard<std::mutex> lock(s_cacheMutex);
    s_cache.clear();
    s_order.clear();
    s_cacheBytes = 0;
}

}
