#include "HoT/Core/Shell.h"
#include "Engine/Source/Os/Gui.h"

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace HoT::Shell {

void FlashGameWindow() {
#if defined(_WIN32)
    HWND window = static_cast<HWND>(OsGuiGetWindow(0));

    if (!window || GetForegroundWindow() == window) {
        return;
    }

    FLASHWINFO info = {};
    info.cbSize = sizeof(info);
    info.hwnd = window;
    info.dwFlags = FLASHW_TIMERNOFG | FLASHW_TRAY;
    info.uCount = static_cast<UINT>(-1);
    info.dwTimeout = 500;
    FlashWindowEx(&info);
#endif
}

void OpenUrl(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWNORMAL);
#endif
}

}
