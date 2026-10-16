#ifndef HOT_DB_TABLES_H
#define HOT_DB_TABLES_H

#include <vector>
#include "WoW/Source/DB/WowClientDB.h"

namespace HoT::DB {

struct Table {
    const char* filename;
    WowClientDB_Base* db;
    bool (*reload)(WowClientDB_Base* db);
};

const std::vector<Table>& ClientTables();

// Accepts "Spell", "Spell.dbc" or "DBFilesClient\Spell.dbc", case-insensitive.
const Table* FindTable(const char* name);

using ReloadListener = void (*)(const char* stem);

// Listeners rebuild state the client derived from a table at startup.
void OnTableReloaded(const char* stem, ReloadListener listener);

bool ReloadTable(const char* name);

}

#endif
