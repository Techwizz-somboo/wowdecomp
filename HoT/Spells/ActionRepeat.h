#ifndef HOT_SPELLS_ACTIONREPEAT_H
#define HOT_SPELLS_ACTIONREPEAT_H

#include <cstdint>

namespace HoT {
class Module;
}

// Repeats a held ACTIONBUTTON1-12 key binding, CVar actionButtonHoldRepeat (0 off, 1 until it casts, 2 continuous).
namespace HoT::ActionRepeat {

// Lives for the whole of CGUIBindings::ExecKey, a release after a repeat must not use the action again.
class KeyScope {
    public:
    KeyScope(const char* key, const char* command, int32_t isDown);
    ~KeyScope();

    private:
    bool m_suppressing;
};

// True once for the CGActionBar::UseAction call a suppressed release would make.
bool ConsumeSuppressUse();

Module* GetModule();

}

#endif
