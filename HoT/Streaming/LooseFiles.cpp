#include "HoT/Streaming/LooseFiles.h"
#include <cstdint>
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Core/Log.h"

namespace HoT::Streaming::LooseFiles {

static constexpr uint32_t s_lookupByPath = 1;

void Apply() {
    System_SFile2::s_directAccess |= s_lookupByPath;
    LOG_INFO << "loose files: direct access enabled, files under the game folder take precedence over archive content";
}

void Invalidate() {
    SFile::RebuildHash();
}

bool IsEnabled() {
    return (System_SFile2::s_directAccess & s_lookupByPath) != 0;
}

}
