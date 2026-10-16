#include "HoT/Editor/Map/Adt/AlphaMap.h"
#include <cstdio>
#include <cstring>

namespace HoT::MapEditor::Adt {

namespace {
void FixLastRowAndColumn(AlphaMap& map) {
    for (int32_t i = 0; i < kAlphaSide; ++i) {
        map.m_texel[i * kAlphaSide + 63] = map.m_texel[i * kAlphaSide + 62];
        map.m_texel[63 * kAlphaSide + i] = map.m_texel[62 * kAlphaSide + i];
    }

    map.m_texel[63 * kAlphaSide + 63] = map.m_texel[62 * kAlphaSide + 62];
}

bool DecodeCompressed(const uint8_t* data, size_t available, AlphaMap& out,
    std::string& error) {
    size_t read = 0;
    int32_t written = 0;

    while (written < kAlphaTexels) {
        if (read >= available) {
            error = "compressed alpha ran out of input";
            return false;
        }

        uint8_t command = data[read++];
        int32_t count = command & 0x7F;
        bool fill = (command & 0x80) != 0;

        if (written + count > kAlphaTexels) {
            count = kAlphaTexels - written;
        }

        if (!count) {
            continue;
        }

        if (fill) {
            if (read >= available) {
                error = "compressed alpha ran out of input mid fill";
                return false;
            }

            std::memset(out.m_texel + written, data[read++], count);
        } else {
            if (read + count > available) {
                error = "compressed alpha ran out of input mid copy";
                return false;
            }

            std::memcpy(out.m_texel + written, data + read, count);
            read += count;
        }

        written += count;
    }

    return true;
}

bool DecodeOne(const uint8_t* data, size_t available, uint32_t layerFlags,
    const AlphaFormat& format, AlphaMap& out, std::string& error) {
    if (format.m_bigAlpha && (layerFlags & kLayerCompressed)) {
        if (!DecodeCompressed(data, available, out, error)) {
            return false;
        }
    } else if (format.m_bigAlpha) {
        if (available < static_cast<size_t>(kAlphaTexels)) {
            error = "big alpha layer is short";
            return false;
        }

        std::memcpy(out.m_texel, data, kAlphaTexels);
    } else {
        if (available < static_cast<size_t>(kAlphaTexels / 2)) {
            error = "packed alpha layer is short";
            return false;
        }

        for (int32_t i = 0; i < kAlphaTexels / 2; ++i) {
            uint8_t lo = data[i] & 0x0F;
            uint8_t hi = (data[i] >> 4) & 0x0F;

            out.m_texel[i * 2 + 0] = static_cast<uint8_t>(lo | (lo << 4));
            out.m_texel[i * 2 + 1] = static_cast<uint8_t>(hi | (hi << 4));
        }
    }

    if (!format.m_doNotFix) {
        FixLastRowAndColumn(out);
    }

    return true;
}

uint8_t ToNibble(uint8_t value) {
    return static_cast<uint8_t>((value * 15 + 127) / 255);
}

void EncodeOne(const AlphaMap& map, const AlphaFormat& format, std::vector<uint8_t>& out) {
    if (format.m_bigAlpha) {
        out.insert(out.end(), map.m_texel, map.m_texel + kAlphaTexels);
        return;
    }

    for (int32_t i = 0; i < kAlphaTexels / 2; ++i) {
        uint8_t lo = ToNibble(map.m_texel[i * 2 + 0]);
        uint8_t hi = ToNibble(map.m_texel[i * 2 + 1]);
        out.push_back(static_cast<uint8_t>(lo | (hi << 4)));
    }
}

size_t McalInsertPoint(const Mcnk& mcnk) {
    const uint32_t before[] = { kMCRF, kMCLY, kMCNR, kMCVT };

    for (uint32_t id : before) {
        for (size_t i = 0; i < mcnk.m_subs.size(); ++i) {
            if (mcnk.m_subs[i].m_id == id) {
                return i + 1;
            }
        }
    }

    return mcnk.m_subs.size();
}
}

bool ReadMapFlags(const char* mapName, uint32_t& flags, std::string& error) {
    flags = 0;

    if (!mapName || !*mapName) {
        error = "no map name";
        return false;
    }

    char path[512];
    std::snprintf(path, sizeof(path), "World\\Maps\\%s\\%s.wdt", mapName, mapName);

    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(path, bytes, error)) {
        return false;
    }

    uint32_t at = 0;
    while (static_cast<size_t>(at) + 8 <= bytes.size()) {
        uint32_t id = 0;
        uint32_t size = 0;
        std::memcpy(&id, bytes.data() + at, 4);
        std::memcpy(&size, bytes.data() + at + 4, 4);

        if (id == ChunkId("MPHD")) {
            if (size < 4 || static_cast<size_t>(at) + 8 + 4 > bytes.size()) {
                error = "MPHD too short";
                return false;
            }

            std::memcpy(&flags, bytes.data() + at + 8, 4);
            return true;
        }

        at += 8 + size;
    }

    error = "no MPHD in the WDT";
    return false;
}

AlphaFormat FormatFor(const Mcnk& mcnk, uint32_t mapFlags) {
    AlphaFormat out;
    out.m_bigAlpha = (mapFlags & 0x4) != 0;
    out.m_doNotFix = (mcnk.m_header.m_flags & kChunkDoNotFixAlpha) != 0;
    return out;
}

bool ReadChunkAlpha(const Mcnk& mcnk, const AlphaFormat& format, ChunkAlpha& out,
    std::string& error) {
    out.m_layers.clear();
    out.m_maps.clear();

    uint32_t count = mcnk.m_header.m_nLayers;
    if (!count) {
        return true;
    }

    const SubChunk* mcly = mcnk.Find(kMCLY);
    if (!mcly || mcly->m_data.size() < count * sizeof(SMLayer)) {
        error = "MCLY missing or too short for the layer count";
        return false;
    }

    out.m_layers.resize(count);
    std::memcpy(out.m_layers.data(), mcly->m_data.data(), count * sizeof(SMLayer));
    out.m_maps.resize(count);

    const SubChunk* mcal = mcnk.Find(kMCAL);
    size_t available = mcal ? mcal->m_data.size() : 0;

    for (uint32_t i = 0; i < count; ++i) {
        const SMLayer& layer = out.m_layers[i];
        if (!(layer.m_flags & kLayerUseAlpha)) {
            continue;
        }

        if (!available || layer.m_offsetInMCAL >= available) {
            error = "layer alpha offset lands past the end of MCAL";
            return false;
        }

        if (!DecodeOne(mcal->m_data.data() + layer.m_offsetInMCAL, available - layer.m_offsetInMCAL,
                layer.m_flags, format, out.m_maps[i], error)) {
            return false;
        }
    }

    return true;
}

bool WriteChunkAlpha(const ChunkAlpha& in, const AlphaFormat& format, Mcnk& mcnk,
    std::string& error) {
    if (in.m_layers.size() != in.m_maps.size()) {
        error = "layer and mask counts disagree";
        return false;
    }

    std::vector<SMLayer> layers = in.m_layers;
    std::vector<uint8_t> alpha;

    for (size_t i = 0; i < layers.size(); ++i) {
        SMLayer& layer = layers[i];

        if (i == 0) {
            layer.m_flags &= ~(kLayerUseAlpha | kLayerCompressed);
            layer.m_offsetInMCAL = 0;
            continue;
        }

        layer.m_flags |= kLayerUseAlpha;
        layer.m_flags &= ~kLayerCompressed;
        layer.m_offsetInMCAL = static_cast<uint32_t>(alpha.size());

        AlphaMap map = in.m_maps[i];
        if (!format.m_doNotFix) {
            FixLastRowAndColumn(map);
        }

        EncodeOne(map, format, alpha);
    }

    SubChunk* mcly = mcnk.Find(kMCLY);
    if (!mcly) {
        error = "chunk has no MCLY to write layers into";
        return false;
    }

    mcly->m_data.resize(layers.size() * sizeof(SMLayer));
    if (!layers.empty()) {
        std::memcpy(mcly->m_data.data(), layers.data(), mcly->m_data.size());
    }

    SubChunk* mcal = mcnk.Find(kMCAL);
    if (!mcal) {
        SubChunk fresh;
        fresh.m_id = kMCAL;
        mcnk.m_subs.insert(mcnk.m_subs.begin() + McalInsertPoint(mcnk), std::move(fresh));
        mcal = mcnk.Find(kMCAL);
    }

    mcal->m_data = std::move(alpha);
    mcnk.m_header.m_nLayers = static_cast<uint32_t>(layers.size());
    return true;
}

}
