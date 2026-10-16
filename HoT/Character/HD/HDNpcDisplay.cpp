#include "HoT/Character/HD/HDNpcDisplay.h"
#include "HoT/Core/Log.h"
#include "HoT/DB/Custom.h"

namespace HoT::HD::NpcDisplay {

template <class T>
static int32_t RowCount(const WowClientDB<T>& db) {
    return db.m_loaded && db.m_records ? db.m_numRecords : 0;
}

template <class T>
static T* RowById(const WowClientDB<T>& db, uint32_t id) {
    return RowCount(db) > 0 ? db.GetRecord(static_cast<int32_t>(id)) : nullptr;
}

const DB::HDNpcDisplayRec* Find(uint32_t displayExtraId) {
    if (!displayExtraId) {
        return nullptr;
    }

    return RowById(DB::g_hdNpcDisplayDB, displayExtraId);
}

const char* BakePath(const DB::HDNpcDisplayRec* row) {
    if (!row || !(row->m_flags & DB::HD_NPC_DISPLAY_BAKE) || !row->m_bakeTexture) {
        return nullptr;
    }

    return *row->m_bakeTexture ? row->m_bakeTexture : nullptr;
}

uint32_t Swap(uint32_t displayId, bool isPlayer) {
    if (!displayId) {
        return 0;
    }

    const DB::HDDisplaySwapRec* row = RowById(DB::g_hdDisplaySwapDB, displayId);

    if (!row || !row->m_hdDisplayID) {
        return 0;
    }

    if ((row->m_flags & DB::HD_DISPLAY_SWAP_PLAYERS_ONLY) && !isPlayer) {
        return 0;
    }

    if ((row->m_flags & DB::HD_DISPLAY_SWAP_NPCS_ONLY) && isPlayer) {
        return 0;
    }

    return row->m_hdDisplayID;
}

bool HasSwaps() {
    return RowCount(DB::g_hdDisplaySwapDB) != 0;
}

void Report() {
    int32_t displays = RowCount(DB::g_hdNpcDisplayDB);

    if (!displays) {
        LOG_INFO << "hd characters: HDNpcDisplay.dbc has no rows, npc models stay stock";
        return;
    }

    int32_t baked = 0;

    for (int32_t i = 0; i < displays; i++) {
        if (DB::g_hdNpcDisplayDB.m_records[i].m_flags & DB::HD_NPC_DISPLAY_BAKE) {
            baked++;
        }
    }

    LOG_INFO << "hd characters: " << displays << " display extras are hd capable, " << baked << " have a baked hd texture, " << RowCount(DB::g_hdDisplaySwapDB) << " display ids are replaced";
}

}
