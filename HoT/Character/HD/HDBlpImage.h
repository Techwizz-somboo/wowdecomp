#ifndef HOT_CHARACTER_HD_HDBLPIMAGE_H
#define HOT_CHARACTER_HD_HDBLPIMAGE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

// A BLP2 decoded to ARGB8888 for the HD sheet compositor, loads are safe from any thread.
namespace HoT::HD {

struct BlpImage {
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_hasAlpha = false;
    std::vector<uint32_t> m_pixels;

    uint32_t At(uint32_t x, uint32_t y) const {
        return this->m_pixels[y * this->m_width + x];
    }
};

bool DecodeBlp(const uint8_t* data, size_t size, BlpImage& out);
std::shared_ptr<const BlpImage> LoadBlp(const char* path);
void ClearBlpCache();

}

#endif
