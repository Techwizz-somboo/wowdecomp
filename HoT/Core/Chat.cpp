#include "HoT/Core/Chat.h"
#include <cstdarg>
#include <cstdio>
#include "WoW/Source/Ui/ChatFrame.h"

namespace HoT::Chat {

void Print(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    CGChat::AddChatMessage(buffer, 0, nullptr, 0, nullptr, nullptr, nullptr, WOWGUID(), 0, WOWGUID(), 0, false, nullptr);
}

}
