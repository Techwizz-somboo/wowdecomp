#ifndef HOT_RUNTIME_LAYOUT_H
#define HOT_RUNTIME_LAYOUT_H

#include <cstdint>

class CSimpleTop;

namespace HoT::Layout {

// Closes the empty frame levels of a strata in one pass, the stock version rescans every frame for every level.
void CompressStrata(CSimpleTop* top, int32_t strata);

}

#endif
