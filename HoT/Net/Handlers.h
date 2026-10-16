#ifndef HOT_NET_HANDLERS_H
#define HOT_NET_HANDLERS_H

#include "HoT/Core/Types.h"
#include "WoW/Source/Net/NetClient/NetClient.h"

namespace HoT::Net {

using Observer = void (*)(uint32 opcode, CDataStore* msg);

// World handlers are installed on every player login, realm handlers on every new realm connection.
void RegisterWorldHandler(uint32 opcode, MESSAGE_HANDLER handler, void* param = nullptr);
void RegisterRealmHandler(uint32 opcode, MESSAGE_HANDLER handler, void* param = nullptr);

// Observers see a message before its handler runs and must leave the read position as they found it.
void RegisterObserver(uint32 opcode, Observer observer);

void ApplyWorldHandlers();
void ApplyRealmHandlers(NetClient* connection);
void Observe(uint32 opcode, CDataStore* msg);

}

#endif
