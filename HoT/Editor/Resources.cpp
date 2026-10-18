#include "HoT/Editor/Resources.h"
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Engine/Source/BlizzardCore/Source/System/Memory/MemoryStorm.h"

namespace HoT::Editor::Resources {

extern const EmbeddedFile g_embeddedFiles[];
extern const size_t g_embeddedFileCount;
extern const EmbeddedFile g_profilerFiles[];
extern const size_t g_profilerFileCount;

struct EmbeddedTable {
    const char* prefix;
    const EmbeddedFile* files;
    const size_t* count;
};

static const EmbeddedTable s_tables[] = {
    { "Interface\\GameEditor\\", g_embeddedFiles, &g_embeddedFileCount },
    { "Interface\\GraphicsProfiler\\", g_profilerFiles, &g_profilerFileCount },
};

// Segoe UI ships with Windows, so the editor reads it from there instead of embedding it.
struct SystemFont {
    const char* name;
    const char* file;
};

static const SystemFont s_systemFonts[] = {
    { "Interface\\GameEditor\\Fonts\\segoeui.ttf", "segoeui.ttf" },
    { "Interface\\GameEditor\\Fonts\\segoeuisb.ttf", "seguisb.ttf" },
};

static bool SamePath(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        char ca = *a == '/' ? '\\' : *a;
        char cb = *b == '/' ? '\\' : *b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }

        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }

        if (ca != cb) {
            return false;
        }
    }

    return *a == *b;
}

static bool StartsWith(const char* name, const char* prefix) {
    for (size_t i = 0; prefix[i]; i++) {
        char c = name[i] == '/' ? '\\' : name[i];
        char p = prefix[i];

        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }

        if (p >= 'A' && p <= 'Z') {
            p = static_cast<char>(p - 'A' + 'a');
        }

        if (c != p) {
            return false;
        }
    }

    return true;
}

static const EmbeddedTable* TableFor(const char* name) {
    if (!name) {
        return nullptr;
    }

    for (const EmbeddedTable& table : s_tables) {
        if (StartsWith(name, table.prefix)) {
            return &table;
        }
    }

    return nullptr;
}

static bool HasPrefix(const char* name) {
    return TableFor(name) != nullptr;
}

static const EmbeddedFile* FindEmbedded(const char* name) {
    const EmbeddedTable* table = TableFor(name);

    if (!table) {
        return nullptr;
    }

    for (size_t i = 0; i < *table->count; i++) {
        if (SamePath(table->files[i].path, name)) {
            return &table->files[i];
        }
    }

    return nullptr;
}

static const SystemFont* FindSystemFont(const char* name) {
    for (const SystemFont& font : s_systemFonts) {
        if (SamePath(font.name, name)) {
            return &font;
        }
    }

    return nullptr;
}

static const std::vector<uint8_t>* LoadSystemFont(const SystemFont& font) {
    static std::vector<uint8_t> s_cache[sizeof(s_systemFonts) / sizeof(s_systemFonts[0])];
    static bool s_tried[sizeof(s_systemFonts) / sizeof(s_systemFonts[0])] = {};

    size_t slot = &font - s_systemFonts;

    if (s_tried[slot]) {
        return s_cache[slot].empty() ? nullptr : &s_cache[slot];
    }

    s_tried[slot] = true;

    char directory[MAX_PATH];
    UINT length = GetWindowsDirectoryA(directory, sizeof(directory));

    if (!length || length >= sizeof(directory)) {
        return nullptr;
    }

    std::string path = std::string(directory) + "\\Fonts\\" + font.file;
    FILE* file = fopen(path.c_str(), "rb");

    if (!file) {
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size > 0) {
        s_cache[slot].resize(static_cast<size_t>(size));

        if (fread(s_cache[slot].data(), 1, s_cache[slot].size(), file) != s_cache[slot].size()) {
            s_cache[slot].clear();
        }
    }

    fclose(file);

    return s_cache[slot].empty() ? nullptr : &s_cache[slot];
}

bool Exists(const char* name) {
    if (!HasPrefix(name)) {
        return false;
    }

    if (FindEmbedded(name)) {
        return true;
    }

    const SystemFont* font = FindSystemFont(name);

    return font && LoadSystemFont(*font);
}

bool Load(const char* name, uint8_t*& data, uint32_t& size) {
    if (!HasPrefix(name)) {
        return false;
    }

    const uint8_t* source = nullptr;
    size_t length = 0;

    if (const EmbeddedFile* file = FindEmbedded(name)) {
        source = file->data;
        length = file->size;
    } else if (const SystemFont* font = FindSystemFont(name)) {
        if (const std::vector<uint8_t>* bytes = LoadSystemFont(*font)) {
            source = bytes->data();
            length = bytes->size();
        }
    }

    if (!source) {
        return false;
    }

    data = static_cast<uint8_t*>(Blizzard::Memory::Allocate(static_cast<uint32_t>(length), 0, __FILE__, __LINE__));
    memcpy(data, source, length);
    size = static_cast<uint32_t>(length);

    return true;
}

}
