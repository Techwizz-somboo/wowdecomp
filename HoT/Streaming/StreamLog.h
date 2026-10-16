#ifndef HOT_STREAMING_STREAMLOG_H
#define HOT_STREAMING_STREAMLOG_H

namespace HoT::Streaming {

// Writes to Logs\hotstream.log in every build, and to the HoT log at debug level.
void StreamLog(const char* format, ...);

}

#endif
