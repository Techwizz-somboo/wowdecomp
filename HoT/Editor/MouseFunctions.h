#ifndef HOT_EDITOR_MOUSEFUNCTIONS_H
#define HOT_EDITOR_MOUSEFUNCTIONS_H

#include <cstdint>

struct HITTESTRESULT;

namespace HoT::Editor::MouseFunctions {

void RegisterScripts();
void RecordHitTest(int32_t kind, const HITTESTRESULT* result);

}

#endif
