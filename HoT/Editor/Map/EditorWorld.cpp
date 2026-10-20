#include "HoT/Editor/Map/EditorWorld.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "Engine/Source/Gx/CGxDevice/CGxDevice.h"
#include "Engine/Source/Gx/Gx.h"
#include "WoW/Source/WorldClient/DayNight.h"
#include "WoW/Source/WorldClient/DayNight__DNInfo.h"
#include "WoW/Source/WorldClient/Map.h"
#include "WoW/Source/WorldClient/MapArea.h"
#include "WoW/Source/WorldClient/World.h"
#include "HoT/Editor/Map/EditorUndo.h"

namespace HoT::Editor::Map::EditorWorld {

struct SavedState {
    bool m_saved = false;
    uint32_t m_enables = 0;
    int32_t m_fog = 1;
};

static std::vector<CMapBaseObjLink*> s_parkedAreas;
static SavedState s_saved;
static bool s_timeOverride;
static float s_timeOfDay = 720.0f;

void ParkDirtyAreas(bool all) {
    const std::vector<uint32_t>& dirty = EditorUndo::GetDirtyAreas();

    if (dirty.empty()) {
        return;
    }

    int32_t minX = CWorld::gbChunkRect.minX >> 4;
    int32_t maxX = CWorld::gbChunkRect.maxX >> 4;
    int32_t minY = CWorld::gbChunkRect.minY >> 4;
    int32_t maxY = CWorld::gbChunkRect.maxY >> 4;

    // A jump that leaves no overlap makes PrepareUpdate purge every area, so park all dirty ones then.
    bool purgeAll = all || CWorld::status.m_chunksChanged != 0;

    for (CMapBaseObjLink* link = CMap::s_unk_ADFBEC.Head(); link;) {
        CMapBaseObjLink* next = CMap::s_unk_ADFBEC.Next(link);
        CMapArea* area = static_cast<CMapArea*>(link->m_owner);
        uint32_t index = static_cast<uint32_t>(64 * area->m_index.y + area->m_index.x);
        bool inside = area->m_index.x >= minX && area->m_index.x <= maxX && area->m_index.y >= minY && area->m_index.y <= maxY;

        if ((!inside || purgeAll) && !area->m_asyncObject && std::find(dirty.begin(), dirty.end(), index) != dirty.end()) {
            link->m_refLink.Unlink();
            s_parkedAreas.push_back(link);
        }

        link = next;
    }
}

void UnparkAreas() {
    for (CMapBaseObjLink* link : s_parkedAreas) {
        CMap::s_unk_ADFBEC.LinkToTail(link);
    }

    s_parkedAreas.clear();
}

void SaveState() {
    s_saved.m_saved = true;
    s_saved.m_enables = CWorld::enables;
    s_saved.m_fog = g_theGxDevicePtr ? g_theGxDevicePtr->MasterEnable(GxMasterEnable_Fog) : 1;
}

void RestoreState() {
    s_timeOverride = false;

    if (!s_saved.m_saved) {
        return;
    }

    s_saved.m_saved = false;
    CWorld::enables = s_saved.m_enables;

    if (g_theGxDevicePtr) {
        g_theGxDevicePtr->MasterEnableSet(GxMasterEnable_Fog, s_saved.m_fog);
    }
}

bool GetFog() {
    return g_theGxDevicePtr && g_theGxDevicePtr->MasterEnable(GxMasterEnable_Fog);
}

void SetFog(bool enabled) {
    if (g_theGxDevicePtr) {
        g_theGxDevicePtr->MasterEnableSet(GxMasterEnable_Fog, enabled ? 1 : 0);
    }
}

uint32_t TimeOfDay() {
    if (s_timeOverride) {
        return static_cast<uint32_t>(s_timeOfDay);
    }

    float progression = DayNight::GetInfo()->m_dayProgression;
    return static_cast<uint32_t>(std::min(std::max(progression, 0.0f), 1.0f) * 1440.0f) % 1440;
}

void SetTimeOfDay(float minutes) {
    s_timeOverride = true;
    s_timeOfDay = std::fmod(std::fmod(minutes, 1440.0f) + 1440.0f, 1440.0f);
}

bool HasTimeOfDay() {
    return s_timeOverride;
}

void ClearTimeOfDay() {
    s_timeOverride = false;
}

void ApplyTimeOfDay() {
    if (!s_timeOverride) {
        return;
    }

    DayNight::DNInfo* info = DayNight::GetInfo();
    info->m_dayProgression = s_timeOfDay / 1440.0f;
    info->m_timeOfDay = static_cast<uint32_t>(s_timeOfDay);
    info->m_day = 0.0f;
}

void Stats(uint32_t& areasLoaded, uint32_t& chunksDrawn) {
    areasLoaded = 0;

    for (CMapBaseObjLink* link = CMap::s_unk_ADFBEC.Head(); link; link = CMap::s_unk_ADFBEC.Next(link)) {
        CMapArea* area = static_cast<CMapArea*>(link->m_owner);

        if (!area->m_asyncObject) {
            areasLoaded++;
        }
    }

    chunksDrawn = CWorldScene::s_chunksRendered;
}

}
