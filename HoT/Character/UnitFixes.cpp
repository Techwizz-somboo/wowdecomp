#include "HoT/Character/UnitFixes.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace HoT::UnitFixes {

static const int32_t POWER_FOCUS = 2;
static const uint32_t UNIT_FLAG2_REGENERATE_POWER = 0x800;

void PredictFocusRegen(CGUnit_C* unit, uint32_t elapsed) {
    if (!unit || unit->m_obj->m_guid != ClntObjMgrGetActivePlayer()) {
        return;
    }

    if (unit->m_unit->m_unitFieldBytes0[3] == POWER_FOCUS) {
        return;
    }

    uint32_t maxFocus = unit->m_unit->m_unitFieldMaxpower[POWER_FOCUS];

    if (!maxFocus || !(unit->m_unit->m_unitFieldFlags2 & UNIT_FLAG2_REGENERATE_POWER) || unit->IsDeadOrGhost()) {
        return;
    }

    float rate = unit->GetPowerRegen(POWER_FOCUS, false);

    if (rate == 0.0f) {
        return;
    }

    static float s_focusFraction = 0.0f;
    s_focusFraction += rate * static_cast<float>(elapsed) * 0.001f;

    int32_t whole = static_cast<int32_t>(s_focusFraction);

    if (!whole) {
        return;
    }

    s_focusFraction -= static_cast<float>(whole);

    int32_t value = unit->GetPredictedPower(POWER_FOCUS) + whole;

    if (value < 0) {
        value = 0;
    } else if (value > static_cast<int32_t>(maxFocus)) {
        value = static_cast<int32_t>(maxFocus);
    }

    unit->SetPredictedPower(POWER_FOCUS, value);
}

void UpdateDismountedSelectionRadius(CGUnit_C* unit) {
    int32_t mountDisplayId = unit->m_mountDisplayId;
    unit->m_mountDisplayId = 0;
    unit->UpdateSelectionRadius();
    unit->m_mountDisplayId = mountDisplayId;
}

}
