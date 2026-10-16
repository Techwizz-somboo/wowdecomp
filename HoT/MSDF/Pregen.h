#ifndef HOT_MSDF_PREGEN_H
#define HOT_MSDF_PREGEN_H

#include <cstddef>
#include <cstdint>
#include <freetype/freetype.h>

// Console tool that fills the disk cache for a whole locale range ahead of time.
// The DLL shipped it without a caller, it stays uncalled here as well.
namespace HoT::MSDF::Pregen {

void RegisterForPreGen(FT_Face face, const uint8_t* data, size_t size, FT_Long faceIndex);

// Opens a console and runs the interactive menu, false when nothing is registered or another instance holds the lock.
bool TryStartPreGen();

void Shutdown();

}

#endif
