#include <filesystem>
#include <string>
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "HoT/Runtime/Cvars.h"
#include "HoT/Runtime/LauncherSettings.h"

namespace HoT::StartupDefaults {

static void SetValue(const char* name, const char* value, bool updateConfig) {
    CVar* cvar = CVar::Lookup(name);

    if (!cvar) {
        return;
    }

    cvar->Set(value, true, false, true, updateConfig);
}

// The client reads the config before this runs and only writes one on exit, so a missing Config.wtf means a new install.
static bool IsFreshInstall() {
    std::error_code ec;
    std::filesystem::path root = LauncherSettings::InstallDir();

    if (root.empty()) {
        root = std::filesystem::current_path(ec);
    }

    if (root.empty()) {
        return false;
    }

    return !std::filesystem::exists(root / "WTF" / "Config.wtf", ec);
}

// Not written to the config, it is a fact about this launch.
void ApplyFreshInstall() {
    SetValue("firstRunFreshInstall", IsFreshInstall() ? "1" : "0", false);
}

// The glue UI cannot skip the startup cinematic on a fresh config because the client reads these before GlueXML loads.
void ApplyMovies() {
    SetValue("movie", "0", true);
    SetValue("expansionMovie", "0", true);
}

}
