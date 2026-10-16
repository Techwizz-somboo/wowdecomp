#ifndef HOT_CHARACTER_HD_HDNPCDISPLAY_H
#define HOT_CHARACTER_HD_HDNPCDISPLAY_H

#include <cstdint>

namespace HoT::DB {
class HDNpcDisplayRec;
}

// Which NPC display extras render on the HD models, their baked HD textures and the display ids swapped for HD ones.
namespace HoT::HD::NpcDisplay {

const DB::HDNpcDisplayRec* Find(uint32_t displayExtraId);
const char* BakePath(const DB::HDNpcDisplayRec* row);
uint32_t Swap(uint32_t displayId, bool isPlayer);
bool HasSwaps();
void Report();

}

#endif
