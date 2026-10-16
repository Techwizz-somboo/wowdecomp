#include "HoT/Core/CVars.h"
#include "HoT/Runtime/Cvars.h"

namespace HoT::StartupDownloaderCvar {

static bool s_enabled = true;

static void OnChanged(const char* value) {
    s_enabled = CVars::Bool(value);
}

void Register() {
    CVars::Register("enableStartupDownloader", "Run the background asset downloader on startup.", "1", OnChanged);
}

bool Enabled() {
    return s_enabled;
}

}
