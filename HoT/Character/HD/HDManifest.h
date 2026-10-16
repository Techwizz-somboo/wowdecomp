#ifndef HOT_CHARACTER_HD_HDMANIFEST_H
#define HOT_CHARACTER_HD_HDMANIFEST_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class CCharacterComponent;

// The per race and sex dressing rules built from DBFilesClient\HDCharacterManifest.dbc.
namespace HoT::HD::Manifest {

static const int32_t OPTION_SKIN_COLOR = 1;
static const int32_t OPTION_FACE = 2;
static const int32_t OPTION_HAIR_STYLE = 3;
static const int32_t OPTION_HAIR_COLOR = 4;
static const int32_t OPTION_FACIAL = 5;
static const int32_t OPTION_DEATH_KNIGHT = 6;
static const int32_t OPTION_COUNT = 7;

static const int32_t TARGET_SKIN = 1;
static const int32_t TARGET_SKIN_EXTRA = 2;
static const int32_t TARGET_HAIR = 10;
static const int32_t TARGET_PELVIS = 13;
static const int32_t TARGET_TORSO = 14;

static const int32_t RECT_WHOLE = -1;
static const int32_t RECT_FACE = -2;
static const int32_t RECT_BODY = -3;

struct Look {
    int32_t m_values[OPTION_COUNT] = {};
};

struct Cond {
    int32_t m_option;
    int32_t m_value;
};

struct Entry {
    std::vector<Cond> m_conds;

    bool Matches(const Look& look) const;
};

struct TexLine : Entry {
    int32_t m_target = 0;
    std::string m_path;
};

struct SlotLine : Entry {
    int32_t m_type = 0;
    std::string m_path;
};

struct LayerLine : Entry {
    int32_t m_order = 0;
    int32_t m_target = 0;
    int32_t m_rect = RECT_WHOLE;
    int32_t m_blend = 0;
    std::string m_path;
};

struct ShowLine : Entry {
    int32_t m_id = 0;
};

struct Range {
    int32_t m_from;
    int32_t m_to;
};

struct Data {
    std::string m_model;
    std::vector<TexLine> m_textures;
    std::vector<SlotLine> m_slots;
    std::vector<LayerLine> m_layers;
    std::vector<Range> m_hides;
    std::vector<ShowLine> m_shows;
    std::map<int32_t, int32_t> m_helms;
};

// Rebuilds every manifest from the loaded table, at client DB init and when the table reloads.
void Build();

const Data* Get(int32_t race, int32_t sex);
const Data* Of(const CCharacterComponent* component);
Look LookOf(const CCharacterComponent* component);
const char* Texture(const Data& data, int32_t target, const Look& look);
const char* Slot(const Data& data, int32_t type, const Look& look);
void ResolveLayers(const Data& data, const Look& look, std::vector<const LayerLine*>& out);
void ResolveShows(const Data& data, const Look& look, std::vector<int32_t>& out, bool keepMarkers = false);

}

#endif
