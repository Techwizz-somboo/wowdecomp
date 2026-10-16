#ifndef HOT_DB_CUSTOM_H
#define HOT_DB_CUSTOM_H

#include <cstdint>
#include "WoW/Source/DB/WowClientDB.h"

class SFileImpl;

namespace HoT::DB {

// Tables that only exist in HoT patches, every one of them is optional.

class SpellClassMaskExtensionRec {
    public:
    static const int32_t s_numColumns = 13;
    static const int32_t s_rowSize = 52;
    static const bool s_generatedID = false;

    struct Block {
        int32_t m_spellClassSet;
        int32_t m_spellClassMask[3];
    };

    /* 0000 */ int32_t m_id;
    /* 0004 */ Block m_blocks[3];

    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

enum SpellCustomAttributes : uint32_t {
    SPELL_ATTR0_CU_REQ_DUAL_WIELD = 0x04000000,
};

class SpellCustomAttrRec {
    public:
    static const int32_t s_numColumns = 3;
    static const int32_t s_rowSize = 12;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ uint32_t m_attributes;
    /* 0008 */ uint32_t m_attributes2;

    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

enum HDCharacterManifestKind : uint32_t {
    HD_MANIFEST_MODEL = 1,
    HD_MANIFEST_TEXTURE = 2,
    HD_MANIFEST_SLOT = 3,
    HD_MANIFEST_LAYER = 4,
    HD_MANIFEST_HIDE = 5,
    HD_MANIFEST_SHOW = 6,
    HD_MANIFEST_HELM = 7,
};

enum HDCharacterManifestRect : uint32_t {
    HD_MANIFEST_RECT_WHOLE = 0,
    HD_MANIFEST_RECT_FACE = 1,
    HD_MANIFEST_RECT_BODY = 2,
    HD_MANIFEST_RECT_SECTION = 10,
};

// One line of the HD character dressing rules, rows for the same race and sex make up its manifest.
class HDCharacterManifestRec {
    public:
    static const int32_t s_numColumns = 16;
    static const int32_t s_rowSize = 64;
    static const bool s_generatedID = false;

    struct Cond {
        uint32_t m_option;
        uint32_t m_value;
    };

    /* 0000 */ int32_t m_id;
    /* 0004 */ uint32_t m_race;
    /* 0008 */ uint32_t m_sex;
    /* 000C */ uint32_t m_kind;
    /* 0010 */ uint32_t m_target;
    /* 0014 */ uint32_t m_value;
    /* 0018 */ uint32_t m_layerOrder;
    /* 001C */ uint32_t m_rect;
    /* 0020 */ uint32_t m_blend;
    /* 0024 */ const char* m_path;
    /* 0028 */ Cond m_conds[3];

    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

enum HDNpcDisplayFlags : uint32_t {
    HD_NPC_DISPLAY_HD_RACE = 0x00000001,
    HD_NPC_DISPLAY_BAKE = 0x00000002,
    HD_NPC_DISPLAY_NPC_LOOK = 0x00000004,
    HD_NPC_DISPLAY_FACE = 0x00000008,
    HD_NPC_DISPLAY_HAIR = 0x00000010,
};

// Keyed by CreatureDisplayInfoExtra id.
class HDNpcDisplayRec {
    public:
    static const int32_t s_numColumns = 6;
    static const int32_t s_rowSize = 24;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ uint32_t m_flags;
    /* 0008 */ uint32_t m_race;
    /* 000C */ uint32_t m_sex;
    /* 0010 */ uint32_t m_retailExtraID;
    /* 0014 */ const char* m_bakeTexture;

    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

enum HDDisplaySwapFlags : uint32_t {
    HD_DISPLAY_SWAP_EVERYONE = 0x00000000,
    HD_DISPLAY_SWAP_PLAYERS_ONLY = 0x00000001,
    HD_DISPLAY_SWAP_NPCS_ONLY = 0x00000002,
};

// Keyed by CreatureDisplayInfo id, rendered as m_hdDisplayID while the HD patch is on.
class HDDisplaySwapRec {
    public:
    static const int32_t s_numColumns = 3;
    static const int32_t s_rowSize = 12;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ uint32_t m_hdDisplayID;
    /* 0008 */ uint32_t m_flags;

    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

static const uint32_t HD_HELM_HIDE_EXTRA_SLOTS = 6;

// Keyed by HelmetGeosetVisData id * 100 + race.
class HDHelmHideRec {
    public:
    static const int32_t s_numColumns = 10;
    static const int32_t s_rowSize = 40;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ uint32_t m_visID;
    /* 0008 */ uint32_t m_race;
    /* 000C */ uint32_t m_maskGroups;
    /* 0010 */ uint32_t m_groups[HD_HELM_HIDE_EXTRA_SLOTS];

    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

extern WowClientDB<SpellClassMaskExtensionRec> g_spellClassMaskExtensionDB;
extern WowClientDB<SpellCustomAttrRec> g_spellCustomAttrDB;
extern WowClientDB<HDCharacterManifestRec> g_hdCharacterManifestDB;
extern WowClientDB<HDNpcDisplayRec> g_hdNpcDisplayDB;
extern WowClientDB<HDDisplaySwapRec> g_hdDisplaySwapDB;
extern WowClientDB<HDHelmHideRec> g_hdHelmHideDB;

void LoadCustomTables();

uint32_t GetSpellCustomAttributes(int32_t spellId);

}

#endif
