#ifndef HOT_LUA_BUILTINS_H
#define HOT_LUA_BUILTINS_H

namespace HoT {
class Module;
}

// General purpose Lua functions the HoT interface relies on.
namespace HoT::Builtins {

Module* GetModule();

}

#endif
