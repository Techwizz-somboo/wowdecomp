#include "HoT/Streaming/BlobContainer.h"
#include <cstdint>
#include <cstring>
#include "Engine/Source/BlizzardCore/Source/Packages/Mopaq/SComp.h"
#include "HoT/Streaming/ArchiveManifest.h"

namespace HoT::Streaming::BlobContainer {

static constexpr size_t s_headerSize = 12;
static constexpr uint8_t s_version = 1;
static constexpr uint8_t s_encodingRaw = 0;
static constexpr uint8_t s_encodingZlib = 1;

static bool Fail(std::string* error, const char* what) {
    if (error) {
        *error = what;
    }

    return false;
}

bool Unpack(const std::string& container, std::string& raw, std::string* error) {
    raw.clear();

    if (container.size() < s_headerSize || std::memcmp(container.data(), "HOTB", 4) != 0) {
        return Fail(error, "not a HOTB blob");
    }

    const uint8_t* h = reinterpret_cast<const uint8_t*>(container.data());

    if (h[4] != s_version) {
        return Fail(error, "unsupported blob version");
    }

    uint32_t rawSize = static_cast<uint32_t>(h[8]) | (static_cast<uint32_t>(h[9]) << 8) | (static_cast<uint32_t>(h[10]) << 16) | (static_cast<uint32_t>(h[11]) << 24);

    if (rawSize > 0x7FFFFFFFu) {
        return Fail(error, "blob is too large");
    }

    const size_t payloadLen = container.size() - s_headerSize;
    const uint8_t* payload = h + s_headerSize;

    switch (h[5]) {
        case s_encodingRaw: {
            if (payloadLen != rawSize) {
                return Fail(error, "raw blob size does not match its header");
            }

            raw.assign(reinterpret_cast<const char*>(payload), payloadLen);
            return true;
        }
        case s_encodingZlib: {
            if (rawSize == 0) {
                return true;
            }

            if (payloadLen > 0xFFFFFFFFu) {
                return Fail(error, "blob is too large");
            }

            raw.resize(rawSize);
            uint32_t got = rawSize;

            if (!System_SComp::ZlibDecompress(raw.data(), &got, payload, static_cast<uint32_t>(payloadLen)) || got != rawSize) {
                raw.clear();
                return Fail(error, "blob inflates to the wrong size");
            }

            return true;
        }
        default:
            return Fail(error, "unknown blob encoding");
    }
}

std::string RelativeBlobPath(const std::string& digest) {
    return "blob/" + digest.substr(0, 2) + "/" + digest;
}

std::string RelativeIndexPath(const std::string& contentId) {
    return "index/" + contentId + ArchiveManifest::s_indexExtension;
}

}
