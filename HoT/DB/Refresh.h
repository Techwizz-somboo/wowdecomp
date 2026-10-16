#ifndef HOT_DB_REFRESH_H
#define HOT_DB_REFRESH_H

namespace HoT {
class Module;
}

// Rebuilds the client lookups derived from a table after HoT::DB::ReloadTable swaps its rows.
namespace HoT::DB {

Module* GetRefreshModule();

}

#endif
