#ifndef CONTROLLER_LUA_H
#define CONTROLLER_LUA_H

#include <cstdint>

class CStatus;

namespace Controller::Lua {

enum Event : int32_t {
    EVENT_ACTIVE_CHANGED = 0,
    EVENT_HOVER_CHANGED,
    EVENT_CONFIGS_CHANGED,
    EVENT_CONNECTED,
    EVENT_DISCONNECTED,
    EVENT_POWER_CHANGED,
    EVENT_COUNT,
};

const char** ScriptEvents(bool game, const char** stockEvents, uint32_t stockCount, uint32_t* count);
void Signal(Event event, const char* format, ...);
void LoadGameUI(CStatus* status);

}

#endif
