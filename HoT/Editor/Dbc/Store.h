#ifndef HOT_EDITOR_DBC_STORE_H
#define HOT_EDITOR_DBC_STORE_H

#include <cstdint>
#include <vector>

class SpellRec;
class WowClientDB_Base;

// Raw record access to the loaded client tables the editor can change.
namespace HoT::Editor::Dbc {

struct Store {
    const char* stem;
    WowClientDB_Base* db;
    uint32_t recordSize;
    bool (*read)(int32_t id, uint8_t* out);
    bool (*readIndex)(int32_t index, uint8_t* out);
    // Overwrites the row in place or appends it, every lookup of the table sees it afterwards.
    bool (*write)(int32_t id, const uint8_t* record, bool* inserted);
};

const std::vector<Store>& Stores();

// Accepts "Spell", "Spell.dbc" or "DBFilesClient\Spell.dbc", case-insensitive.
const Store* FindStore(const char* table);

using SpellPatch = void (*)(SpellRec& rec);

// Client lookups see the patched copy until UnpatchSpell, the editor keeps reading and writing the real row.
bool PatchSpell(int32_t id, SpellPatch patch);
void UnpatchSpell(int32_t id);

}

#endif
