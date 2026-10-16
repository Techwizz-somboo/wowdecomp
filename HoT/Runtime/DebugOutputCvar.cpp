#include <cstdarg>
#include <cstdio>
#include <string>
#include "HoT/Core/CVars.h"
#include "HoT/Core/Chat.h"
#include "HoT/Core/Log.h"
#include "HoT/Runtime/Cvars.h"
#include "HoT/Runtime/DebugOutput.h"

namespace HoT::DebugOutput {

static bool s_enabled = false;

bool Enabled() {
    return s_enabled;
}

void SetEnabled(bool enabled) {
    s_enabled = enabled;
}

void Printf(const char* format, ...) {
    if (!s_enabled) {
        return;
    }

    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int len = vsnprintf(nullptr, 0, format, copy);
    va_end(copy);

    if (len < 0) {
        va_end(args);
        return;
    }

    std::string buffer(static_cast<size_t>(len) + 1, '\0');
    vsnprintf(&buffer[0], buffer.size(), format, args);
    va_end(args);
    buffer.resize(static_cast<size_t>(len));

    LOG_DEBUG << buffer;
    Chat::Print("%s", buffer.c_str());
}

}

namespace HoT::DebugOutputCvar {

static void OnChanged(const char* value) {
    DebugOutput::SetEnabled(CVars::Bool(value));
}

void Register() {
    CVars::Register("debugOutput", "Enable verbose DLL debug logging.", "0", OnChanged);
}

}
