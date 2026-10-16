#include "HoT/Runtime/Display.h"
#include <vector>
#include "Engine/Source/Gx/CGxDevice/Types.h"

namespace HoT::Display {

static bool s_cacheValid = false;
static std::vector<CGxMonitorMode> s_modes;

bool GetCachedMonitorModes(TSGrowableArray<CGxMonitorMode>& modes) {
    if (!s_cacheValid) {
        return false;
    }

    modes.SetCount(static_cast<uint32_t>(s_modes.size()));

    for (uint32_t i = 0; i < s_modes.size(); i++) {
        modes.Ptr()[i] = s_modes[i];
    }

    return true;
}

void CacheMonitorModes(TSGrowableArray<CGxMonitorMode>& modes) {
    if (modes.Count() == 0) {
        return;
    }

    s_modes.assign(modes.Ptr(), modes.Ptr() + modes.Count());
    s_cacheValid = true;
}

void InvalidateMonitorModes() {
    s_cacheValid = false;
}

}
