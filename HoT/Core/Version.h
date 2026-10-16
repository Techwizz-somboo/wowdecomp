#ifndef HOT_CORE_VERSION_H
#define HOT_CORE_VERSION_H

#include <cstdint>

namespace HoT::Version {

// The commit this client was built from, sent to the server at login and in sanity checks.
const char* Commit();
const uint8_t* CommitBytes();
bool Dirty();

}

#endif
