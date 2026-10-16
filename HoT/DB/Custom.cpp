#include "HoT/DB/Custom.h"
#include "Engine/Source/BlizzardCore/SFile2-Core.h"
#include "HoT/Core/Log.h"
#include "HoT/DB/ClientDB.h"

namespace HoT::DB {

WowClientDB<SpellClassMaskExtensionRec> g_spellClassMaskExtensionDB;
WowClientDB<SpellCustomAttrRec> g_spellCustomAttrDB;
WowClientDB<HDCharacterManifestRec> g_hdCharacterManifestDB;
WowClientDB<HDNpcDisplayRec> g_hdNpcDisplayDB;
WowClientDB<HDDisplaySwapRec> g_hdDisplaySwapDB;
WowClientDB<HDHelmHideRec> g_hdHelmHideDB;

template <class T>
static bool ReadRow(SFileImpl* file, T* row) {
    return SFileReadFile(file, row, T::s_rowSize, nullptr, nullptr, 0) != 0;
}

const char* SpellClassMaskExtensionRec::GetFilename() {
    return "DBFilesClient\\SpellClassMaskExtension.dbc";
}

bool SpellClassMaskExtensionRec::Read(SFileImpl* file, const char* stringBuffer) {
    return ReadRow(file, this);
}

const char* SpellCustomAttrRec::GetFilename() {
    return "DBFilesClient\\SpellCustomAttr.dbc";
}

bool SpellCustomAttrRec::Read(SFileImpl* file, const char* stringBuffer) {
    return ReadRow(file, this);
}

template <class V>
static bool ReadValue(SFileImpl* file, V& value) {
    return SFileReadFile(file, &value, sizeof(value), nullptr, nullptr, 0) != 0;
}

static const char* StringAt(const char* stringBuffer, uint32_t offset) {
    return stringBuffer ? &stringBuffer[offset] : "";
}

const char* HDCharacterManifestRec::GetFilename() {
    return "DBFilesClient\\HDCharacterManifest.dbc";
}

bool HDCharacterManifestRec::Read(SFileImpl* file, const char* stringBuffer) {
    uint32_t pathOffset;

    bool ok = ReadValue(file, this->m_id)
        && ReadValue(file, this->m_race)
        && ReadValue(file, this->m_sex)
        && ReadValue(file, this->m_kind)
        && ReadValue(file, this->m_target)
        && ReadValue(file, this->m_value)
        && ReadValue(file, this->m_layerOrder)
        && ReadValue(file, this->m_rect)
        && ReadValue(file, this->m_blend)
        && ReadValue(file, pathOffset)
        && ReadValue(file, this->m_conds);

    if (!ok) {
        return false;
    }

    this->m_path = StringAt(stringBuffer, pathOffset);
    return true;
}

const char* HDNpcDisplayRec::GetFilename() {
    return "DBFilesClient\\HDNpcDisplay.dbc";
}

bool HDNpcDisplayRec::Read(SFileImpl* file, const char* stringBuffer) {
    uint32_t bakeOffset;

    bool ok = ReadValue(file, this->m_id)
        && ReadValue(file, this->m_flags)
        && ReadValue(file, this->m_race)
        && ReadValue(file, this->m_sex)
        && ReadValue(file, this->m_retailExtraID)
        && ReadValue(file, bakeOffset);

    if (!ok) {
        return false;
    }

    this->m_bakeTexture = StringAt(stringBuffer, bakeOffset);
    return true;
}

const char* HDDisplaySwapRec::GetFilename() {
    return "DBFilesClient\\HDDisplaySwap.dbc";
}

bool HDDisplaySwapRec::Read(SFileImpl* file, const char* stringBuffer) {
    return ReadRow(file, this);
}

const char* HDHelmHideRec::GetFilename() {
    return "DBFilesClient\\HDHelmHide.dbc";
}

bool HDHelmHideRec::Read(SFileImpl* file, const char* stringBuffer) {
    return ReadRow(file, this);
}

static_assert(sizeof(SpellClassMaskExtensionRec) == SpellClassMaskExtensionRec::s_rowSize);
static_assert(sizeof(SpellCustomAttrRec) == SpellCustomAttrRec::s_rowSize);
static_assert(sizeof(HDDisplaySwapRec) == HDDisplaySwapRec::s_rowSize);
static_assert(sizeof(HDHelmHideRec) == HDHelmHideRec::s_rowSize);

template <class T>
static void LoadOptional(WowClientDB<T>& db) {
    if (TryLoad(db, __FILE__, __LINE__)) {
        LOG_INFO << T::GetFilename() << ": " << db.m_numRecords << " rows";
    }
}

void LoadCustomTables() {
    LoadOptional(g_spellClassMaskExtensionDB);
    LoadOptional(g_spellCustomAttrDB);
    LoadOptional(g_hdCharacterManifestDB);
    LoadOptional(g_hdNpcDisplayDB);
    LoadOptional(g_hdDisplaySwapDB);
    LoadOptional(g_hdHelmHideDB);
}

uint32_t GetSpellCustomAttributes(int32_t spellId) {
    SpellCustomAttrRec* rec = g_spellCustomAttrDB.m_loaded ? g_spellCustomAttrDB.GetRecord(spellId) : nullptr;
    return rec ? rec->m_attributes : 0;
}

}
