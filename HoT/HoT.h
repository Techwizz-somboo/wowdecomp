#ifndef HOT_HOT_H
#define HOT_HOT_H

#include <cstdint>

class CDataStore;
class NetClient;
struct lua_State;

// Entry points the client calls into. Each is a single call at one lifecycle point in the decomp.
namespace HoT {

void OnInitializeGlobal();
void OnClientDBInitialized();
void OnInitializeClient();
void OnRealmConnectionCreated(NetClient* connection);
void OnPlayerClientInitialize();
void OnNetMessage(uint32_t msgId, CDataStore* msg);
void OnGameScriptsLoaded(const char** stockEvents, uint32_t stockCount);
void OnGlueScriptsLoaded(const char** stockEvents, uint32_t stockCount);
void OnCharacterEnum();
void OnCharacterListUpdated();
void OnEnterWorld();
void OnLoadingScreenStarted();
void OnDestroyGame();

// True when a HoT window took the UseContainerItem(bag, slot) click.
bool OnUseContainerItem(lua_State* L);

}

#endif
