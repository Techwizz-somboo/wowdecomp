#ifndef HOT_DISCORD_DISCORD_H
#define HOT_DISCORD_DISCORD_H

namespace HoT {
class Module;
}

// The enableDiscord CVar and the UpdateDiscordPresence and ToggleDiscord Lua functions.
namespace HoT::Discord {

Module* GetModule();

}

#endif
