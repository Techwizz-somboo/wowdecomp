#include "HoT/MSDF/MSDF.h"
#include <algorithm>
#include <cstring>
#include <vector>
#include <msdfgen.h>
#include <msdfgen-ext.h>
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/CGxDevice/CGxShader.h"
#include "Engine/Source/Gx/Gx.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/MSDF/Font.h"
#include "HoT/MSDF/Manager.h"
#include "HoT/MSDF/ShaderCode.h"

namespace HoT::MSDF {

static int32_t s_mode = 1;
static bool s_settingsLatched = false;
static bool s_wanted = false;
static bool s_allowUnsafe = false;
static bool s_enabled = false;
static msdfgen::FreetypeHandle* s_freetype = nullptr;
static CGxShader* s_vertexShader = nullptr;
static CGxShader* s_pixelShader = nullptr;
static std::vector<Font*> s_fonts;

msdfgen::FreetypeHandle* FreetypeLibrary() {
    return s_freetype;
}

bool AllowUnsafeFonts() {
    return s_allowUnsafe;
}

static void OnModeChanged(const char* value) {
    s_mode = CVars::Int(value);
}

// The DLL turns MSDF off for these, their glyph sets are too large for live generation.
static bool IsCjkLocale() {
    CVar* locale = CVar::Lookup("locale");
    const char* name = locale && locale->m_stringValue.m_str ? locale->m_stringValue.m_str : "";

    return !strcmp(name, "zhCN") || !strcmp(name, "zhTW") || !strcmp(name, "koKR");
}

static bool DeviceSupportsMsdf() {
    EGxApi api = g_theGxDevicePtr->DeviceApi();

    if (api != GxApi_D3d9 && api != GxApi_D3d9Ex) {
        return false;
    }

    const CGxCaps& caps = g_theGxDevicePtr->Caps();

    return caps.m_shaderTargets[GxSh_Vertex] >= 3 && caps.m_shaderTargets[GxSh_Pixel] >= 4;
}

// A shader entry whose bytecode ships with the client instead of a .bls file, Gx rebuilds it from m_code after a device reset.
static CGxShader* CreateShader(EGxShTarget target, const char* name, const uint8_t* code, uint32_t size) {
    CGxShader* shader = g_theGxDevicePtr->m_shaderList[target].Ptr(name);

    if (!shader) {
        shader = g_theGxDevicePtr->m_shaderList[target].New(name, 0, 0);
        shader->m_target = target;
        shader->m_code.SetCount(size);
        memcpy(shader->m_code.m_data, code, size);
        shader->m_loaded = 0;
    }

    shader->m_refCount++;

    return shader;
}

void Initialize() {
    if (!s_settingsLatched) {
        // Like the DLL the mode is read once per session, a change applies on the next start.
        s_settingsLatched = true;
        s_allowUnsafe = s_mode > 1;
        s_wanted = s_mode > 0 && !IsCjkLocale();

        LOG_INFO << "MSDF mode: " << s_mode;
    }

    if (s_enabled || !s_wanted || !g_theGxDevicePtr || !DeviceSupportsMsdf()) {
        return;
    }

    s_freetype = msdfgen::initializeFreetype();

    if (!s_freetype) {
        return;
    }

    s_vertexShader = CreateShader(GxSh_Vertex, "HoT_MSDF", s_vertexShaderCode, sizeof(s_vertexShaderCode));
    s_pixelShader = CreateShader(GxSh_Pixel, "HoT_MSDF", s_pixelShaderCode, sizeof(s_pixelShaderCode));
    s_enabled = true;
}

void Shutdown() {
    for (Font* font : s_fonts) {
        font->Release();
    }

    if (s_vertexShader) {
        g_theGxDevicePtr->ShaderDestroy(s_vertexShader);
        s_vertexShader = nullptr;
    }

    if (s_pixelShader) {
        g_theGxDevicePtr->ShaderDestroy(s_pixelShader);
        s_pixelShader = nullptr;
    }

    Manager::FlushAll();

    if (s_freetype) {
        msdfgen::deinitializeFreetype(s_freetype);
        s_freetype = nullptr;
    }

    s_enabled = false;
}

bool Enabled() {
    return s_enabled;
}

Font* OpenFont(FT_Face face, const void* data, uint32_t size) {
    if (!s_enabled || !face || !data || !size) {
        return nullptr;
    }

    Font* font = new Font(face, static_cast<const uint8_t*>(data), size);

    if (!font->IsValid()) {
        delete font;
        return nullptr;
    }

    s_fonts.push_back(font);

    return font;
}

void CloseFont(Font* font) {
    if (!font) {
        return;
    }

    s_fonts.erase(std::remove(s_fonts.begin(), s_fonts.end(), font), s_fonts.end());
    delete font;
}

const GlyphMetrics* GetGlyph(Font* font, uint32_t codepoint) {
    return font ? font->GetGlyph(codepoint) : nullptr;
}

uint32_t EvictionCount(const Font* font) {
    return font ? font->EvictionCount() : 0;
}

bool BeginDraw(Font* font) {
    if (!font || !s_vertexShader || !s_pixelShader || !s_vertexShader->Valid() || !s_pixelShader->Valid()) {
        return false;
    }

    font->FlushDirtyPages();

    GxRsSet(GxRs_VertexShader, s_vertexShader);
    GxRsSet(GxRs_PixelShader, s_pixelShader);

    for (uint32_t page = 0; page < MAX_ATLAS_PAGES; page++) {
        GxRsSet(static_cast<EGxRenderState>(GxRs_Texture12 + page), font->PageTexture(page));
    }

    return true;
}

float OutlineHint(uint32_t fontFlags, bool is3d) {
    if (is3d) {
        return 0.0f;
    }

    if (fontFlags & 8) {
        return 2.0f;
    }

    return (fontFlags & 1) ? 1.0f : 0.0f;
}

class MSDFModule : public Module {
    public:
    const char* Name() const override {
        return "MSDF";
    }

    void OnInitializeGlobal() override {
        CVars::Register("MSDFMode", "0 = Disabled; 1 = Enabled; 2 = Enabled for unsafe/self-intersecting fonts", "1", OnModeChanged);
    }
};

Module* GetModule() {
    static MSDFModule module;
    return &module;
}

}
