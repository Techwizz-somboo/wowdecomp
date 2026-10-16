#include <cctype>
#include <string>
#include <vector>
#include "HoT/Core/Log.h"
#include "HoT/DB/ClientDB.h"
#include "HoT/DB/Tables.h"

namespace HoT::DB {

struct Listener {
    std::string stem;
    ReloadListener listener;
};

static std::vector<Listener>& Listeners() {
    static std::vector<Listener> listeners;
    return listeners;
}

static std::string Stem(const char* name) {
    std::string s(name ? name : "");
    size_t slash = s.find_last_of("\\/");

    if (slash != std::string::npos) {
        s = s.substr(slash + 1);
    }

    size_t dot = s.find_last_of('.');

    if (dot != std::string::npos) {
        s = s.substr(0, dot);
    }

    for (char& c : s) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return s;
}

const Table* FindTable(const char* name) {
    std::string want = Stem(name);

    for (const Table& t : ClientTables()) {
        if (Stem(t.filename) == want) {
            return &t;
        }
    }

    return nullptr;
}

void OnTableReloaded(const char* stem, ReloadListener listener) {
    Listeners().push_back({ Stem(stem), listener });
}

bool ReloadTable(const char* name) {
    const Table* table = FindTable(name);

    if (!table) {
        LOG_DEBUG << "no client table for " << name;
        return false;
    }

    if (!table->reload(table->db)) {
        LOG_ERROR << "reload of " << table->filename << " failed, keeping the loaded rows";
        return false;
    }

    std::string stem = Stem(table->filename);

    LOG_INFO << "reloaded " << table->filename << " (" << table->db->m_numRecords << " rows, " << RetiredBytes() << " bytes retired so far)";

    for (const Listener& l : Listeners()) {
        if (l.stem == stem) {
            l.listener(stem.c_str());
        }
    }

    return true;
}

}
