#ifndef HOT_RUNTIME_LINKWATCHDOG_H
#define HOT_RUNTIME_LINKWATCHDOG_H

namespace HoT {
class Module;
}

// The client pings every 30 seconds but never checks for the pong, so a half open link is never noticed. This disconnects when pongs stop.
namespace HoT::LinkWatchdog {

Module* GetModule();

}

#endif
