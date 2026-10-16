#ifndef HOT_RUNTIME_DEBUGOUTPUT_H
#define HOT_RUNTIME_DEBUGOUTPUT_H

// Verbose HoT debug lines behind the debugOutput CVar, written to the log and to chat.
namespace HoT::DebugOutput {

bool Enabled();
void SetEnabled(bool enabled);
void Printf(const char* format, ...);

}

#endif
