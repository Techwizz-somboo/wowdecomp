#ifndef HOT_SPELLS_ACTIONBAR_H
#define HOT_SPELLS_ACTIONBAR_H

#include <cstdint>

class SpellRec;

namespace HoT::Spells {

// The multicast element bit (fire 0x1, earth 0x2, water 0x4, air 0x8) of a spell that summons a totem, 0 otherwise.
uint32_t GetTotemSlotMask(const SpellRec* spell);

// Accepts a HoT totem on the multicast slot of its element when the stock totem category check failed.
bool IsTotemForMultiCastSlot(int32_t slot, const SpellRec* spell);

// A newly learned spell would go into the empty slot, the UI decides through HOT_PLACE_ACTION.
void OfferLearnedSpell(const SpellRec* spell, int32_t slot);

}

#endif
