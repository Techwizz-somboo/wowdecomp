#ifndef HOT_MODELS_FILEDATAID_H
#define HOT_MODELS_FILEDATAID_H

#include <cstdint>

// FileDataID to client path, backed by TextureFilePath.db2 and ModelFilePath.db2.
namespace HoT::Models::Fdid {

// Loads the texture path table once, safe while the warm thread is still loading it.
void EnsureLoaded();

// Starts EnsureLoaded on a worker thread, only call it once the archives are mounted.
void BeginWarm();

// The returned strings live for the whole process. ResolveModel falls back to the texture table.
const char* ResolveTexture(uint32_t fileDataId);
const char* ResolveModel(uint32_t fileDataId);

// Model path to its FileDataID or 0, ignoring case, slash direction and extension.
uint32_t ResolveModelId(const char* modelPath);

}

#endif
