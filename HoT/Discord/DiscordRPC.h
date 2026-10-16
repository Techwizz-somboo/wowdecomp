#ifndef HOT_DISCORD_DISCORDRPC_H
#define HOT_DISCORD_DISCORDRPC_H

// Discord rich presence on a worker thread. discord_game_sdk.dll is loaded at runtime and everything is a no-op without it.
namespace HoT::Discord::RPC {

// Starts the worker, false under Wine or when it is already running.
bool Init();
void Shutdown();
bool IsInitialized();

// Queues an activity, the worker sends it once Discord is connected and resends it after a reconnect.
void UpdateActivity(const char* state, const char* details, const char* largeImage = "", const char* largeText = "", const char* smallImage = "", const char* smallText = "");

}

#endif
