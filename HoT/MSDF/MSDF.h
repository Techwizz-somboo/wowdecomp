#ifndef HOT_MSDF_MSDF_H
#define HOT_MSDF_MSDF_H

#include <cstdint>
#include <freetype/freetype.h>
#include "HoT/MSDF/Types.h"

namespace HoT {
class Module;
}

// Multi channel signed distance field text for the Gxu font system.
namespace HoT::MSDF {

class Font;

// Registers the MSDFMode CVar.
Module* GetModule();

// Called by GxuFontInitialize once the FreeType library exists, and by GxuFontShutdown before it goes away.
void Initialize();
void Shutdown();
bool Enabled();

// Null when MSDF is off or the face is not usable as a distance field font, the face keeps the stock bitmap path then.
Font* OpenFont(FT_Face face, const void* data, uint32_t size);
void CloseFont(Font* font);

const GlyphMetrics* GetGlyph(Font* font, uint32_t codepoint);
uint32_t EvictionCount(const Font* font);

// Binds the MSDF shaders and the font's atlas pages to s12..s15, false when the shaders cannot be used.
bool BeginDraw(Font* font);

// Outline hint the pixel shader reads from TEXCOORD1.y, 2 for thick outlines, 1 for normal ones.
float OutlineHint(uint32_t fontFlags, bool is3d);

}

#endif
