#include "HoT/Spells/Targeting.h"
#include <cmath>
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/WorldFrame.h"
#include "WoW/Source/WorldClient/Map.h"

namespace HoT::Spells {

static const uint32_t TERRAIN_TRACE_FLAGS = 0x10111;
static const float PROBE_DEPTH = 500.0f;
static const float SEARCH_PRECISION = 0.5f;

static bool s_castAtCursor;

// Set while the range clamp probes candidate points, those tracks must not fire a cursor cast.
static bool s_probing;

static float Dot(const C3Vector& a, const C3Vector& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static C3Vector Normalized(const C3Vector& v) {
    float lengthSq = Dot(v, v);

    if (lengthSq <= 0.0f) {
        return C3Vector(0.0f, 0.0f, 0.0f);
    }

    return v * (1.0f / sqrtf(lengthSq));
}

static C3Vector PointAtDistance(const C3Vector& start, const C3Vector& end, float distance) {
    C3Vector dir = end - start;
    float length = sqrtf(Dot(dir, dir));

    if (length == 0.0f) {
        return start;
    }

    return start + dir * (distance / length);
}

static void ClickTerrain(const C3Vector& point) {
    CTerrainClickEvent evt;
    evt.m_guid = WOWGUID();
    evt.m_point = point;
    evt.m_button = MOUSE_BUTTON_LEFT;
    CGGameUI::HandleTerrainClick(evt);
}

static bool IsTargetingTerrain() {
    return Spell_C_IsTargeting() && Spell_C_CanTargetTerrain();
}

// Runs the stock terrain track for a candidate hit, true when the spell shadow came out acceptable.
static bool TrackAccepts(CGWorldFrame* frame, const C3Vector& start, const C3Vector& end, const C3Vector& hit, float distance) {
    HITTESTRESULT test;
    test.m_guid = WOWGUID();
    test.m_point = hit;
    test.m_distance = distance;
    test.m_segStart = start;
    test.m_segEnd = end;

    s_probing = true;
    frame->OnLayerTrackTerrain(test);
    s_probing = false;

    return WorldFrame_GetSpellShadowStyle() == 0;
}

bool CastSpellByNameAt(lua_State* L, lua_CFunction cast) {
    if (lua_type(L, 2) != LUA_TSTRING) {
        return false;
    }

    const char* target = lua_tolstring(L, 2, nullptr);

    if (!SStrCmpI(target, "cursor", 6)) {
        lua_settop(L, 1);
        s_castAtCursor = true;
        return false;
    }

    if (SStrCmpI(target, "self", 4)) {
        return false;
    }

    lua_settop(L, 1);
    cast(L);

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (player && IsTargetingTerrain()) {
        ClickTerrain(player->GetPosition());
    }

    return true;
}

bool ConsumeCastAtCursor(const HITTESTRESULT& result) {
    if (!s_castAtCursor || s_probing) {
        return false;
    }

    s_castAtCursor = false;

    // A spell that never asked for a ground target leaves the flag behind, a stray click would clear the target or the cursor.
    if (!IsTargetingTerrain()) {
        return false;
    }

    ClickTerrain(result.m_point);
    return true;
}

void ClampGroundTarget(CGWorldFrame* frame, C3Vector* start, C3Vector* end) {
    if (!frame || !IsTargetingTerrain() || !s_targetingSpell) {
        return;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return;
    }

    float minRange = 0.0f;
    float maxRange = 0.0f;
    Spell_C::GetSpellRange(player, s_targetingSpell->m_data.m_spellId, &minRange, &maxRange, nullptr);

    C3Vector hit;
    float distance = 1.0f;

    if (CMap::Intersect(start, end, &hit, &distance, TERRAIN_TRACE_FLAGS, nullptr) && TrackAccepts(frame, *start, *end, hit, distance)) {
        return;
    }

    // Search along the camera ray from the point closest to the player, probing straight down for ground.
    C3Vector segStart = *start;
    C3Vector segEnd = *end;
    C3Vector dir = Normalized(segEnd - segStart);
    float along = Dot(player->GetPosition() - segStart, dir);

    if (along > 0.0f) {
        segStart = segStart + dir * along;
    }

    float rangeSq = maxRange * maxRange;
    float left = 0.0f;
    float right = maxRange * 3.0f;
    float bestDistSq = -1.0f;
    C3Vector bestPoint;
    C3Vector bestBelow;

    while (right - left > SEARCH_PRECISION) {
        float mid = (left + right) * 0.5f;
        C3Vector point = PointAtDistance(segStart, segEnd, mid);
        C3Vector below(point.x, point.y, point.z - PROBE_DEPTH);
        distance = 1.0f;

        if (!CMap::Intersect(&point, &below, &hit, &distance, TERRAIN_TRACE_FLAGS, nullptr)) {
            right = mid;
            continue;
        }

        float distSq = player->GetDistanceToPos(hit);

        if (distSq < rangeSq) {
            left = mid;
            continue;
        }

        if (TrackAccepts(frame, point, below, hit, distance)) {
            bestDistSq = distSq;
            bestPoint = point;
            bestBelow = below;
            left = mid;
        } else {
            right = mid;
        }
    }

    if (bestDistSq > rangeSq) {
        *start = bestPoint;
        *end = bestBelow;
    }
}

}
