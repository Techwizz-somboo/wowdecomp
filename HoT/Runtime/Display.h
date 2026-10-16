#ifndef HOT_RUNTIME_DISPLAY_H
#define HOT_RUNTIME_DISPLAY_H

#include "Engine/Source/Storm/H/stpl.h"

class CGxMonitorMode;

// Enumerating display modes is slow, so the list is reused until Windows reports a display change.
namespace HoT::Display {

bool GetCachedMonitorModes(TSGrowableArray<CGxMonitorMode>& modes);
void CacheMonitorModes(TSGrowableArray<CGxMonitorMode>& modes);
void InvalidateMonitorModes();

}

#endif
