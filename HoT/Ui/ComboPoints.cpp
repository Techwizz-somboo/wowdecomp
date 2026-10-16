#include "HoT/Ui/ComboPoints.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"

namespace HoT::ComboPoints {

static const char* const ANCHOR_EVENT = "HOT_COMBO_POINT_ANCHOR";

static void OnAnchorChanged(const char* value) {
    if (!Lua::HasEvent(ANCHOR_EVENT)) {
        return;
    }

    Lua::SignalEvent(ANCHOR_EVENT, "%s", value ? value : "");
}

class ComboPointsModule : public Module {
    public:
    const char* Name() const override {
        return "ComboPoints";
    }

    void OnInitializeGlobal() override {
        CVars::Register("comboPointAnchor", "Unit frame the combo point display sits on: player or target.", "player", OnAnchorChanged);
    }
};

Module* GetModule() {
    static ComboPointsModule module;
    return &module;
}

}
