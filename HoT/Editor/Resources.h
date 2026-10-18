#ifndef HOT_EDITOR_RESOURCES_H
#define HOT_EDITOR_RESOURCES_H

#include <cstddef>
#include <cstdint>

// The game editor UI is compiled into the exe and served as Interface\GameEditor\... when no loose file or archive has it.
namespace HoT::Editor::Resources {

struct EmbeddedFile {
    const char* path;
    const uint8_t* data;
    size_t size;
};

bool Exists(const char* name);

// The buffer comes from Blizzard::Memory::Allocate so SFileImpl frees it like its own.
bool Load(const char* name, uint8_t*& data, uint32_t& size);

}

#endif
