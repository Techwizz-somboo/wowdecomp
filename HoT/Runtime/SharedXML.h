#ifndef HOT_RUNTIME_SHAREDXML_H
#define HOT_RUNTIME_SHAREDXML_H

class CStatus;

namespace HoT::SharedXML {

// Loads Interface\SharedXML\SharedXML.toc after FrameXML.toc ("frame") or GlueXML.toc ("glue") when it exists.
void Load(const char* state, CStatus* status);

}

#endif
