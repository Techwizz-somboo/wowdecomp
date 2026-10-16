#include "HoT/Net/Handlers.h"
#include <vector>
#include "HoT/Core/Log.h"
#include "HoT/Net/Opcodes.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

namespace HoT::Net {

struct StoredHandler {
    uint32 opcode;
    MESSAGE_HANDLER handler;
    void* param;
};

struct StoredObserver {
    uint32 opcode;
    Observer observer;
};

static std::vector<StoredHandler>& WorldHandlers() {
    static std::vector<StoredHandler> handlers;
    return handlers;
}

static std::vector<StoredHandler>& RealmHandlers() {
    static std::vector<StoredHandler> handlers;
    return handlers;
}

static std::vector<StoredObserver>& Observers() {
    static std::vector<StoredObserver> observers;
    return observers;
}

static bool ValidOpcode(uint32 opcode) {
    if (opcode < NUM_HANDLER_SLOTS) {
        return true;
    }

    LOG_ERROR << "opcode " << opcode << " is outside the handler table";
    return false;
}

void RegisterWorldHandler(uint32 opcode, MESSAGE_HANDLER handler, void* param) {
    if (ValidOpcode(opcode)) {
        WorldHandlers().push_back({ opcode, handler, param });
    }
}

void RegisterRealmHandler(uint32 opcode, MESSAGE_HANDLER handler, void* param) {
    if (ValidOpcode(opcode)) {
        RealmHandlers().push_back({ opcode, handler, param });
    }
}

void RegisterObserver(uint32 opcode, Observer observer) {
    Observers().push_back({ opcode, observer });
}

void ApplyWorldHandlers() {
    for (const StoredHandler& h : WorldHandlers()) {
        ClientServices::SetMessageHandler(static_cast<NETMESSAGE>(h.opcode), h.handler, h.param);
    }
}

void ApplyRealmHandlers(NetClient* connection) {
    for (const StoredHandler& h : RealmHandlers()) {
        connection->SetMessageHandler(static_cast<NETMESSAGE>(h.opcode), h.handler, h.param);
    }
}

void Observe(uint32 opcode, CDataStore* msg) {
    for (const StoredObserver& o : Observers()) {
        if (o.opcode == opcode) {
            uint32_t read = msg->m_read;
            o.observer(opcode, msg);
            msg->m_read = read;
        }
    }
}

}
