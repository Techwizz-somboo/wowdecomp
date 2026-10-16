#include "HoT/Spells/AutoRepeatDeadzone.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"

namespace HoT::AutoRepeatDeadzone {

static const float MELEE_RANGE_FLOOR = 5.0f;
static const float MELEE_RANGE_BONUS = 1.3333334f;

static int32_t s_percent = 0;

bool IsInMeleeRange(const CGUnit_C* unit, CGUnit_C* target) {
    if (s_percent >= 100) {
        return unit->IsInMeleeRange(target) != 0;
    }

    if (s_percent <= 0 || !unit || !target) {
        return false;
    }

    float reach = unit->m_unit->m_unitFieldCombatreach + target->m_unit->m_unitFieldCombatreach + MELEE_RANGE_BONUS;
    float radius = (reach > MELEE_RANGE_FLOOR ? reach : MELEE_RANGE_FLOOR) * (s_percent / 100.0f);
    return unit->GetDistanceToPos(target->GetPosition()) <= radius * radius;
}

static void OnPercentChanged(const char* value) {
    int32_t percent = CVars::Int(value);
    s_percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
}

class AutoRepeatDeadzoneModule : public Module {
    public:
    const char* Name() const override {
        return "AutoRepeatDeadzone";
    }

    void OnInitializeGlobal() override {
        CVars::Register("autoRepeatMeleeDeadzone", "Melee range at which auto-repeat shots stop, as a percent of the client default. 0 never stops them.", "0", OnPercentChanged);
    }
};

Module* GetModule() {
    static AutoRepeatDeadzoneModule module;
    return &module;
}

}
