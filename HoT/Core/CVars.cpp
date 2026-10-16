#include "HoT/Core/CVars.h"
#include <cstdlib>
#include "Engine/Source/ConsoleBase/ConsoleVar.h"
#include "Engine/Source/ConsoleBase/Types.h"

namespace HoT::CVars {

static bool OnChanged(CVar* cvar, const char* previous, const char* value, void* arg) {
    if (arg) {
        reinterpret_cast<ChangeFn>(arg)(value ? value : "");
    }

    return true;
}

CVar* Register(const char* name, const char* help, const char* defaultValue, ChangeFn onChange) {
    CVar* cvar = CVar::Register(name, help, 1, defaultValue, OnChanged, CATEGORY_DEFAULT, false, reinterpret_cast<void*>(onChange), false);

    if (onChange) {
        onChange(cvar && cvar->m_stringValue.m_str ? cvar->m_stringValue.m_str : defaultValue);
    }

    return cvar;
}

int32_t Int(const char* value) {
    return value ? atoi(value) : 0;
}

bool Bool(const char* value) {
    return Int(value) != 0;
}

float Float(const char* value) {
    return value ? static_cast<float>(atof(value)) : 0.0f;
}

}
