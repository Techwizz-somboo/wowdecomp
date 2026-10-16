#ifndef HOT_UI_ATLAS_H
#define HOT_UI_ATLAS_H

namespace HoT {
class Module;
}

namespace HoT::Atlas {

// Installs C_Texture.LoadAtlasData, GetAtlasInfo, SetAtlas and the global SetAtlas in the glue and game states.
Module* GetModule();

}

#endif
