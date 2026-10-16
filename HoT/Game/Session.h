#ifndef HOT_GAME_SESSION_H
#define HOT_GAME_SESSION_H

#include <cstdint>

namespace HoT {

class Module;

namespace Session {

int8_t SecurityLevel();
bool HasClearance(int8_t level);

bool InWorld();
void SetInWorld(bool inWorld);

// The game mode picked on character create, sent as a trailing byte of CMSG_CHAR_CREATE.
uint8_t CharCreateGameMode();
void SetCharCreateGameMode(uint8_t mode);

void SendSanityCheck(bool sendMpqs);

Module* GetModule();

}

}

#endif
