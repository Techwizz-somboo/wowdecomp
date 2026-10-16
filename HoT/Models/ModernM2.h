#ifndef HOT_MODELS_MODERNM2_H
#define HOT_MODELS_MODERNM2_H

#include <cstdint>

class M2Data;
class M2SkinProfile;

namespace HoT {
class Module;
}

// Native reading of the modern chunked M2 (MD21), its skins and its external files, called from Model2.
namespace HoT::Models {

class ModernModel;

// True when the loaded model bytes are an MD21 container.
bool IsModernContainer(const void* data, uint32_t size);

// Runs before M2Init: demuxes the container, slides the body to the buffer base, updates size and rewrites the records into the 3.3.5 shape. Null fails the load.
ModernModel* PrepareModel(M2Data* data, uint32_t& size, const char* fileName);

// Runs after M2Init: TXID texture names and the reference clamps.
void FinishModel(ModernModel* model, M2Data* data);

void Destroy(ModernModel* model);

// Rebuilds a modern skin profile into a new runtime layout skin (SIMD::AllocA16), or null on failure. May replace model header arrays.
M2SkinProfile* BuildSkin(ModernModel* model, M2Data* data, const M2SkinProfile* raw, uint32_t size, uint32_t maxBones);

// The skin file from the SFID chunk, or null.
const char* SkinFilePath(const ModernModel* model, uint32_t profile);

// The .anim file from the AFID chunk, or the name built from the source sequence id, or null.
const char* AnimFilePath(ModernModel* model, const char* modelFileName, uint32_t sequenceIndex, uint32_t id, uint32_t variation);

// Strips an AFM2 wrapper in place and returns the payload size, or returns size unchanged.
uint32_t UnwrapAnim(void* buffer, uint32_t size);

Module* GetModule();

}

#endif
