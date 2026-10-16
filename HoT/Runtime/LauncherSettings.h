#ifndef HOT_RUNTIME_LAUNCHERSETTINGS_H
#define HOT_RUNTIME_LAUNCHERSETTINGS_H

#include <cstdint>
#include <string>

namespace HoT {
class Module;
}

// The launcher's <exeDir>\.hotlauncher\settings.json, read once on first use and written back one key at a time.
namespace HoT::LauncherSettings {

bool HdPatch();
double MaxDownloadMBps();
int64_t MaxDownloadBytesPerSecond();
int32_t SelectedRealmId();
std::string InstallDir();

// Full path of the launcher exe as the player last ran it, which may be a renamed copy.
std::string LauncherPath();

void SetHdPatch(bool enabled);
void SetMaxDownloadMBps(double mbps);

// Registers IsHDPatchEnabled, SetHDPatchEnabled, AreHDCharacterModelsAvailable, GetDownloadSpeedLimit and SetDownloadSpeedLimit.
Module* GetModule();

}

#endif
