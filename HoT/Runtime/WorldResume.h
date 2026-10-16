#ifndef HOT_RUNTIME_WORLDRESUME_H
#define HOT_RUNTIME_WORLDRESUME_H

class NetClient;

namespace HoT {
class Module;
}

// Keeps the client in the world when the game server drops and replays the login underneath it when the server returns.
namespace HoT::WorldResume {

// From CGlueMgr::NetDisconnectHandler, true means the teardown is suppressed and the world is held.
bool OnDisconnect(NetClient* client);

bool IsActive();

Module* GetModule();

}

#endif
