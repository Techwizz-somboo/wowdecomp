#include "HoT/Core/Version.h"
#include "HoT/BuildVersion.h"

namespace HoT::Version {

static_assert(sizeof(HOT_COMMIT_HASH) == 41, "BuildVersion.h did not produce a 40 character commit hash");

static uint8_t Nibble(char c) {
    if (c >= '0' && c <= '9') {
        return static_cast<uint8_t>(c - '0');
    }

    if (c >= 'a' && c <= 'f') {
        return static_cast<uint8_t>(c - 'a' + 10);
    }

    if (c >= 'A' && c <= 'F') {
        return static_cast<uint8_t>(c - 'A' + 10);
    }

    return 0;
}

const char* Commit() {
    return HOT_COMMIT_HASH;
}

const uint8_t* CommitBytes() {
    static uint8_t bytes[20];
    static bool decoded = false;

    if (!decoded) {
        for (uint32_t i = 0; i < sizeof(bytes); i++) {
            bytes[i] = static_cast<uint8_t>((Nibble(HOT_COMMIT_HASH[i * 2]) << 4) | Nibble(HOT_COMMIT_HASH[i * 2 + 1]));
        }

        decoded = true;
    }

    return bytes;
}

bool Dirty() {
    return HOT_COMMIT_DIRTY != 0;
}

}
