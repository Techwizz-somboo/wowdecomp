#ifndef HOT_CHARACTER_HANDITEMDISPLAY_H
#define HOT_CHARACTER_HANDITEMDISPLAY_H

#include <cstdint>

class CGUnit_C;
class CGUnitVirtualItem;
class CM2Model;
class ItemDisplayInfoRec;

// Weapon and shield models are loaded from whichever folder holds the file, so a shield can sit in either hand.
namespace HoT::HandItemDisplay {

// True when the model loads from Item\ObjectComponents\Shield, preferShield decides when the file is in both folders or neither.
bool UsesShieldFolder(const ItemDisplayInfoRec* rec, bool preferShield);

// SheatheHandItem's view of UsesShieldFolder for a virtual item slot, the ranged slot never counts as a shield.
bool IsShieldModel(const CGUnit_C* unit, uint32_t hand, const CGUnitVirtualItem* item);

// Mirrors a shield model held in the right hand so it faces out, applied false resets the transform.
void SetMainHandShieldTransform(CM2Model* child, CM2Model* parent, bool applied);

// Applies the main hand shield transform to the model attached to the right hand of parent.
void MirrorMainHandShield(CM2Model* parent);

}

#endif
