#ifndef HOT_EDITOR_MODEL_TEXTUREIMPORT_H
#define HOT_EDITOR_MODEL_TEXTUREIMPORT_H

#include <cstdint>
#include <string>
#include <vector>

// Image files to BLP2 textures the 3.3.5 client reads.
namespace HoT::Editor::Model::TextureImport {

struct BlpInfo {
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_alphaBits = 0;
    uint32_t m_mips = 0;
    std::string m_format;
};

// Reads png, jpg, bmp and gif through WIC, tga by hand and blp as is, then returns the BLP2 file bytes.
bool ConvertFile(const std::wstring& path, std::vector<uint8_t>& blp, std::string& message, std::string& error);

bool ReadInfo(const std::string& gamePath, BlpInfo& info);

bool IsImportExtension(const std::wstring& name);

}

#endif
