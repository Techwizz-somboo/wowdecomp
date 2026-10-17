#include "Controller/Bindings.h"
#include <cstdio>
#include <cstring>
#include "Controller/Buttons.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Ui/UIBindings.h"

namespace Controller::Bindings {

static const char* BINDINGS_FILE = "WTF\\GamePadBindings.wtf";

static const char* s_defaultBindings[][2] = {
    { "PAD1", "JUMP" },
    { "PAD2", "GAMEPADINTERACT" },
    { "PAD3", "ACTIONBUTTON1" },
    { "PAD4", "ACTIONBUTTON2" },
    { "SHIFT-PAD1", "ACTIONBUTTON3" },
    { "SHIFT-PAD2", "ACTIONBUTTON4" },
    { "SHIFT-PAD3", "ACTIONBUTTON5" },
    { "SHIFT-PAD4", "ACTIONBUTTON6" },
    { "CTRL-PAD1", "ACTIONBUTTON7" },
    { "CTRL-PAD2", "ACTIONBUTTON8" },
    { "CTRL-PAD3", "ACTIONBUTTON9" },
    { "CTRL-PAD4", "ACTIONBUTTON10" },
    { "CTRL-PADLSHOULDER", "ACTIONBUTTON11" },
    { "CTRL-PADRSHOULDER", "ACTIONBUTTON12" },
    { "PADLSHOULDER", "TARGETNEARESTFRIEND" },
    { "PADRSHOULDER", "TARGETNEARESTENEMY" },
    { "SHIFT-PADLSHOULDER", "TARGETSELF" },
    { "SHIFT-PADRSHOULDER", "TARGETPREVIOUSENEMY" },
    { "PADDUP", "TOGGLECHARACTER0" },
    { "PADDDOWN", "OPENALLBAGS" },
    { "PADDLEFT", "TOGGLESPELLBOOK" },
    { "PADDRIGHT", "TOGGLEWORLDMAP" },
    { "SHIFT-PADDUP", "TOGGLEQUESTLOG" },
    { "SHIFT-PADDDOWN", "TOGGLESOCIAL" },
    { "SHIFT-PADDLEFT", "TOGGLETALENTS" },
    { "SHIFT-PADDRIGHT", "TOGGLEACHIEVEMENT" },
    { "PADLSTICK", "TOGGLERUN" },
    { "PADRSTICK", "TOGGLEAUTORUN" },
    { "PADBACK", "TOGGLEGAMEPADCURSOR" },
};

// Same layout as a stock binding set, flag 1 marks an untouched default so only changes are saved.
static TSHashTable<KEYBINDING, HASHKEY_STRI> s_bindings;

bool IsPadKey(const char* key) {
    if (!key || !*key) {
        return false;
    }

    const char* separator = strrchr(key, '-');
    return ButtonFromBinding(separator ? separator + 1 : key) >= 0;
}

// Rewrites SHIFT-ctrl-pad1 style input as CTRL-SHIFT-PAD1, the order key events arrive in.
static bool Normalize(const char* key, char* buffer, int32_t length) {
    if (!IsPadKey(key)) {
        return false;
    }

    uint32_t modifiers = 0;
    const char* name = key;
    CGUIBindings::AddModifiers(modifiers, name);

    if (strchr(name, '-')) {
        return false;
    }

    int32_t button = ButtonFromBinding(name);

    if (button < 0 || !CGUIBindings::AddMetaPrefix(modifiers, buffer, length)) {
        return false;
    }

    SStrCopy(buffer, ButtonBinding(button), length);
    return true;
}

static int32_t NumKeys(const char* command) {
    int32_t count = 0;

    for (KEYBINDING* binding = s_bindings.m_fulllist.Head(); binding; binding = s_bindings.m_fulllist.Next(binding)) {
        const char* existing = binding->m_data[BINDING_MODE_0].m_command.m_str;

        if (existing && !SStrCmpI(existing, command, STORM_MAX_STR)) {
            count++;
        }
    }

    return count;
}

static void AdjustIndices(const char* command, int32_t index) {
    for (KEYBINDING* binding = s_bindings.m_fulllist.Head(); binding; binding = s_bindings.m_fulllist.Next(binding)) {
        KEYBINDINGDATA& data = binding->m_data[BINDING_MODE_0];

        if (data.m_command.m_str && !SStrCmpI(data.m_command.m_str, command, STORM_MAX_STR) && data.m_index > index) {
            data.m_index--;
        }
    }
}

static bool Bind(const char* key, const char* command, bool isDefault) {
    char normalized[128];

    if (!Normalize(key, normalized, sizeof(normalized))) {
        return false;
    }

    if (!command || !*command) {
        command = "NONE";
    }

    KEYBINDING* binding = s_bindings.Ptr(normalized);

    if (!binding) {
        binding = s_bindings.New(normalized, 0, 0);
    }

    if (isDefault) {
        binding->m_flags |= 1u;
    } else {
        binding->m_flags &= ~1u;
    }

    KEYBINDINGDATA& data = binding->m_data[BINDING_MODE_0];

    if (!data.m_command.m_str || SStrCmp(data.m_command.m_str, command, STORM_MAX_STR)) {
        if (data.m_command.m_str) {
            AdjustIndices(data.m_command.m_str, data.m_index);
        }

        int32_t index = NumKeys(command);
        data.m_command.Copy(command);
        data.m_index = index;
    }

    return true;
}

static void LoadDefaults() {
    s_bindings.Clear();

    for (const auto& binding : s_defaultBindings) {
        Bind(binding[0], binding[1], true);
    }
}

void Load() {
    LoadDefaults();

    FILE* file = fopen(BINDINGS_FILE, "r");

    if (!file) {
        return;
    }

    char line[1024];

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = 0;

        if (SStrCmpI(line, "bind ", 5)) {
            continue;
        }

        char* key = line + 5;
        char* command = strchr(key, ' ');

        if (command) {
            *command++ = 0;
            Bind(key, command, false);
        }
    }

    fclose(file);
}

void Save() {
    FILE* file = fopen(BINDINGS_FILE, "w");

    if (!file) {
        return;
    }

    for (KEYBINDING* binding = s_bindings.m_fulllist.Head(); binding; binding = s_bindings.m_fulllist.Next(binding)) {
        const char* command = binding->m_data[BINDING_MODE_0].m_command.m_str;

        if (!(binding->m_flags & 1) && command) {
            fprintf(file, "bind %s %s\n", binding->m_key.m_str, command);
        }
    }

    fclose(file);
}

void Reset() {
    LoadDefaults();
    Save();
}

bool Find(const char* key, KEYBINDING*& binding) {
    if (!IsPadKey(key)) {
        return false;
    }

    binding = s_bindings.Ptr(key);
    return true;
}

bool Set(const char* key, const char* command) {
    if (!Bind(key, command, false)) {
        return false;
    }

    Save();
    return true;
}

const char* GetAction(const char* key) {
    char normalized[128];

    if (!Normalize(key, normalized, sizeof(normalized))) {
        return nullptr;
    }

    KEYBINDING* binding = s_bindings.Ptr(normalized);
    const char* command = binding ? binding->m_data[BINDING_MODE_0].m_command.m_str : nullptr;
    return command && SStrCmpI(command, "NONE", STORM_MAX_STR) ? command : nullptr;
}

const char* GetKey(const char* command, int32_t index) {
    if (!command || !*command) {
        return nullptr;
    }

    for (KEYBINDING* binding = s_bindings.m_fulllist.Head(); binding; binding = s_bindings.m_fulllist.Next(binding)) {
        const KEYBINDINGDATA& data = binding->m_data[BINDING_MODE_0];

        if (data.m_command.m_str && !SStrCmpI(data.m_command.m_str, command, STORM_MAX_STR) && data.m_index == index) {
            return binding->m_key.m_str;
        }
    }

    return nullptr;
}

}
