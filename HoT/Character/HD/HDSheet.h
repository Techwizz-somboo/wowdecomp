#ifndef HOT_CHARACTER_HD_HDSHEET_H
#define HOT_CHARACTER_HD_HDSHEET_H

class CCharacterComponent;

// The HD body sheet, composited on helper threads from the skin, item and manifest layer textures.
namespace HoT::HD::Sheet {

void Destroy(CCharacterComponent* component);
void ClearCache();

}

#endif
