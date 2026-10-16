#ifndef HOT_FEATURES_ITEMS_H
#define HOT_FEATURES_ITEMS_H

#include <cstdint>

class ItemRec;
class ItemStats_C;

namespace HoT {
class Module;
}

// Item records for ids the client Item.dbc does not know, filled from the server and the item cache.
namespace HoT::Items {

// Server pushed records first, then the item cache, then the Item.dbc row.
ItemRec* Resolve(int32_t id, ItemRec* dbcRecord);

// The heroic tooltip line key, replaced by the HoT legendary, heirloom, paradoxical or corrupted label from the item's second flags.
const char* GetHeroicLabelKey(const ItemStats_C* stats, const char* defaultKey);

Module* GetModule();

}

#endif
