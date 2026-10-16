#include "HoT/Runtime/LinkWatchdog.h"
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "WoW/Source/Net/NetClient/NetClient.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

namespace HoT::LinkWatchdog {

// The server pongs from its socket thread, so with pings 30 seconds apart a dead link shows up 10 to 40 seconds after it goes.
static const uint32_t PONG_TIMEOUT_MS = 10000;

// A minimised or loading client may have its pong waiting undispatched, so long frames only count for this much.
static const uint32_t MAX_FRAME_MS = 250;

static bool s_enabled = true;
static bool s_armed;
static bool s_waiting;
static bool s_proven;
static uint32_t s_pingSequence;
static uint32_t s_latencyEnd;
static uint32_t s_unansweredMs;
static uint32_t s_lastPumpMs;

static void Disarm() {
    s_armed = false;
    s_waiting = false;
    s_proven = false;
    s_unansweredMs = 0;
}

static void Pump() {
    uint32_t now = OsGetAsyncTimeMs();
    uint32_t frameMs = now - s_lastPumpMs;
    s_lastPumpMs = now;

    ClientConnection* netClient = ClientServices::s_currentConnection;

    if (!s_enabled || !netClient || netClient->m_netState != NS_CONNECTED) {
        Disarm();
        return;
    }

    if (!s_armed) {
        s_armed = true;
        s_pingSequence = netClient->m_pingSequence;
        s_latencyEnd = netClient->m_latencyEnd;
        return;
    }

    if (netClient->m_pingSequence != s_pingSequence) {
        s_pingSequence = netClient->m_pingSequence;
        s_waiting = true;
        s_unansweredMs = 0;
    }

    // HandlePong only accepts the pong for the latest ping.
    if (netClient->m_latencyEnd != s_latencyEnd) {
        s_latencyEnd = netClient->m_latencyEnd;
        s_waiting = false;
        s_proven = true;
    }

    // The first ping can sit behind the handshake of a freshly restarted server, so only judge a link that has answered before.
    if (!s_proven || !s_waiting) {
        return;
    }

    s_unansweredMs += frameMs < MAX_FRAME_MS ? frameMs : MAX_FRAME_MS;

    if (s_unansweredMs < PONG_TIMEOUT_MS) {
        return;
    }

    LOG_WARN << "LinkWatchdog: no pong for ping " << s_pingSequence << ", disconnecting";
    Disarm();

    // HandleDisconnect then runs from the event queue, the same as when the server closes the link.
    netClient->NetClient::Disconnect();
}

static void OnEnabledChanged(const char* value) {
    s_enabled = CVars::Bool(value);
}

class LinkWatchdogModule : public Module {
    public:
    const char* Name() const override { return "LinkWatchdog"; }

    void OnInitializeGlobal() override {
        CVars::Register("linkWatchdog", "Disconnect when the game server stops answering pings, instead of waiting forever for a socket that will never close.", "1", OnEnabledChanged);
    }

    void OnIdle() override {
        Pump();
    }
};

Module* GetModule() {
    static LinkWatchdogModule module;
    return &module;
}

}
