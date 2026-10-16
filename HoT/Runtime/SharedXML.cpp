#include "HoT/Runtime/SharedXML.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/MD5.h"
#include "HoT/Core/Log.h"

namespace HoT::SharedXML {

static const char* const SHARED_TOC = "Interface\\SharedXML\\SharedXML.toc";

void Load(const char* state, CStatus* status) {
    if (!SFile::FileExistsEx(SHARED_TOC, 1)) {
        return;
    }

    LOG_DEBUG << "Loading " << SHARED_TOC << " (" << state << ")";

    char marker[64];
    SStrPrintf(marker, sizeof(marker), "HOT_LUA_STATE=\"%s\"", state);
    FrameScript_Execute(marker, "SharedXML", nullptr);

    // SharedXML is not part of the signed interface, so it gets its own digest that nothing checks.
    MD5_CTX md5;
    MD5Init(&md5);
    FrameXML_CreateFrames(SHARED_TOC, nullptr, &md5, status);
}

}
