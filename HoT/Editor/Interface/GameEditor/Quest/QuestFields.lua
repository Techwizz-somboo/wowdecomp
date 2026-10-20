-- Quest editor field data. Names follow HoTEmulator QuestDef.h and SharedDefines.h.
GameEditorData = GameEditorData or {}
GameEditorData.Fields = GameEditorData.Fields or {}
GameEditorData.Groups = GameEditorData.Groups or {}
GameEditorData.Enums = GameEditorData.Enums or {}
GameEditorData.Flags = GameEditorData.Flags or {}

local TEXT_CODES = "$B is a line break, $N the player name, $R the race, $C the class and $Gmale:female; picks by gender."

-- quest_template.QuestType, the quest method in Quest::GetQuestMethod
GameEditorData.Enums.QuestMethod = {
    [0] = "Auto complete",
    [1] = "Disabled",
    [2] = "Normal",
}

-- SharedDefines.h QuestTypes, values of QuestInfo.dbc
GameEditorData.Enums.QuestInfo = {
    [0] = "Normal",
    [1] = "Group (QUEST_TYPE_ELITE)",
    [21] = "Life",
    [41] = "PvP",
    [62] = "Raid",
    [81] = "Dungeon",
    [82] = "World Event",
    [83] = "Legendary",
    [84] = "Escort",
    [85] = "Heroic",
    [88] = "Raid (10)",
    [89] = "Raid (25)",
}

-- SharedDefines.h QuestSort, quest_template.QuestSortID holds these negated
GameEditorData.Enums.QuestSort = {
    [1] = "Epic",
    [21] = "Wailing Caverns (old)",
    [22] = "Seasonal",
    [23] = "Undercity (old)",
    [24] = "Herbalism",
    [25] = "Battlegrounds",
    [41] = "Uldaman (old)",
    [61] = "Warlock",
    [81] = "Warrior",
    [82] = "Shaman",
    [101] = "Fishing",
    [121] = "Blacksmithing",
    [141] = "Paladin",
    [161] = "Mage",
    [162] = "Rogue",
    [181] = "Alchemy",
    [182] = "Leatherworking",
    [201] = "Engineering",
    [221] = "Treasure Map",
    [241] = "Sunken Temple (old)",
    [261] = "Hunter",
    [262] = "Priest",
    [263] = "Druid",
    [264] = "Tailoring",
    [284] = "Special",
    [304] = "Cooking",
    [324] = "First Aid",
    [344] = "Legendary",
    [364] = "Darkmoon Faire",
    [365] = "Ahn'Qiraj War",
    [366] = "Lunar Festival",
    [367] = "Reputation",
    [368] = "Invasion",
    [369] = "Midsummer",
    [370] = "Brewfest",
    [371] = "Inscription",
    [372] = "Death Knight",
    [373] = "Jewelcrafting",
    [374] = "Noblegarden",
    [375] = "Pilgrim's Bounty",
    [376] = "Love is in the Air",
    [377] = "Transmutation",
}

-- QuestDef.h QuestFlags
GameEditorData.Flags.QuestFlags = {
    {bit = 0, name = "STAY_ALIVE", tooltip = "Not used by the server"},
    {bit = 1, name = "PARTY_ACCEPT", tooltip = "Party members that can take the quest get an accept prompt"},
    {bit = 2, name = "EXPLORATION", tooltip = "Not used by the server"},
    {bit = 3, name = "SHARABLE", tooltip = "The quest can be shared with the party"},
    {bit = 4, name = "HAS_CONDITION", tooltip = "Not used by the server"},
    {bit = 5, name = "HIDE_REWARD_POI", tooltip = "Not used by the server"},
    {bit = 6, name = "RAID", tooltip = "Can be completed while in a raid group"},
    {bit = 7, name = "TBC", tooltip = "Not used by the server"},
    {bit = 8, name = "NO_MONEY_FROM_XP", tooltip = "Experience is not turned into gold at max level, not used by the server"},
    {bit = 9, name = "HIDDEN_REWARDS", tooltip = "Item and money rewards only show when the quest is turned in"},
    {bit = 10, name = "TRACKING", tooltip = "Rewarded right on completion and never shown in the quest log"},
    {bit = 11, name = "DEPRECATE_REPUTATION", tooltip = "Not used by the server"},
    {bit = 12, name = "DAILY", tooltip = "Daily quest, needs Repeatable in the addon special flags"},
    {bit = 13, name = "FLAGS_PVP", tooltip = "Having the quest in the log turns PvP on"},
    {bit = 14, name = "UNAVAILABLE", tooltip = "Quest that is not generally available"},
    {bit = 15, name = "WEEKLY", tooltip = "Weekly quest"},
    {bit = 16, name = "AUTOCOMPLETE", tooltip = "The client completes it on accept when the objectives are already done"},
    {bit = 17, name = "DISPLAY_ITEM_IN_TRACKER", tooltip = "Shows the usable quest item in the objective tracker"},
    {bit = 18, name = "OBJ_TEXT", tooltip = "Uses the objective text as the completion text"},
    {bit = 19, name = "AUTO_ACCEPT", tooltip = "The client treats the quest as auto accepted"},
}

-- QuestDef.h QuestSpecialFlags, only the bits allowed in the database
GameEditorData.Flags.QuestSpecialFlags = {
    {bit = 0, name = "REPEATABLE", tooltip = "The quest can be done again, daily and weekly quests need it"},
    {bit = 1, name = "EXPLORATION_OR_EVENT", tooltip = "Completed by an area trigger, a script or a quest complete spell"},
    {bit = 2, name = "AUTO_ACCEPT", tooltip = "The quest is accepted on its own"},
    {bit = 3, name = "DF_QUEST", tooltip = "Used by the dungeon finder"},
    {bit = 4, name = "MONTHLY", tooltip = "Resets at the start of the month"},
    {bit = 5, name = "CAST", tooltip = "The creature objectives are credited by a spell cast instead of a kill"},
}

-- SharedDefines.h Classes, bit = class id - 1
GameEditorData.Flags.QuestClassMask = {
    {bit = 0, name = "Warrior", const = "CLASS_WARRIOR"},
    {bit = 1, name = "Paladin", const = "CLASS_PALADIN"},
    {bit = 2, name = "Hunter", const = "CLASS_HUNTER"},
    {bit = 3, name = "Rogue", const = "CLASS_ROGUE"},
    {bit = 4, name = "Priest", const = "CLASS_PRIEST"},
    {bit = 5, name = "Death Knight", const = "CLASS_DEATH_KNIGHT"},
    {bit = 6, name = "Shaman", const = "CLASS_SHAMAN"},
    {bit = 7, name = "Mage", const = "CLASS_MAGE"},
    {bit = 8, name = "Warlock", const = "CLASS_WARLOCK"},
    {bit = 10, name = "Druid", const = "CLASS_DRUID"},
}

-- Race bits match the spell editor RaceMask, which loads before this file.
GameEditorData.Flags.QuestRaceMask = GameEditorData.Flags.RaceMask or {
    {bit = 0, name = "Human"},
    {bit = 1, name = "Orc"},
    {bit = 2, name = "Dwarf"},
    {bit = 3, name = "Night Elf"},
    {bit = 4, name = "Undead"},
    {bit = 5, name = "Tauren"},
    {bit = 6, name = "Gnome"},
    {bit = 7, name = "Troll"},
    {bit = 9, name = "Blood Elf"},
    {bit = 10, name = "Draenei"},
}

local Fields = {}

Fields.quest_template = {
    ID = {label = "Quest ID", type = "int"},
    QuestType = {label = "Method", tooltip = "0 completes on accept, 1 is disabled, 2 is a normal quest", type = "int", enum = "QuestMethod"},
    QuestLevel = {label = "Quest Level", tooltip = "Level the quest is shown and rewarded at, -1 uses the level of the player", type = "int"},
    MinLevel = {label = "Min Level", tooltip = "Lowest player level that can take the quest", type = "int"},
    QuestSortID = {label = "Zone or Sort", tooltip = "A positive value is an AreaTable zone id, a negative value is a QuestSort id such as -61 for warlock quests", type = "int", ref = "quest_sort"},
    QuestInfoID = {label = "Quest Info", tooltip = "QuestInfo.dbc type shown in the quest log, for example Group or Dungeon", type = "int", enum = "QuestInfo"},
    SuggestedGroupNum = {label = "Suggested Players", tooltip = "Group size shown in the quest log, 0 shows nothing", type = "int"},
    TimeAllowed = {label = "Time Limit (sec)", tooltip = "Seconds to finish the quest, 0 means no limit", type = "int"},
    AllowableRaces = {label = "Allowed Races", tooltip = "Races that can take the quest, 0 allows every race", type = "int", flags = "QuestRaceMask"},
    Flags = {label = "Flags", type = "int", flags = "QuestFlags"},
    StartItem = {label = "Start Item", tooltip = "Item the player gets when accepting the quest, it is taken again on abandon", type = "int", ref = "item"},
    LogTitle = {label = "Title", type = "string"},
    LogDescription = {label = "Objectives Text", tooltip = "Short objective summary in the quest log. "..TEXT_CODES, type = "string"},
    QuestDescription = {label = "Quest Text", tooltip = "Main quest text the giver shows. "..TEXT_CODES, type = "string"},
    AreaDescription = {label = "Area Description", tooltip = "Text shown for quests completed by exploring an area", type = "string"},
    QuestCompletionLog = {label = "Completion Log Text", tooltip = "Quest log text once the objectives are done, for example Return to Marshal McBride", type = "string"},
    RequiredFactionId1 = {label = "Reputation Faction 1", tooltip = "Faction whose standing is an objective", type = "int", ref = "dbc:Faction"},
    RequiredFactionValue1 = {label = "Reputation Value 1", tooltip = "Standing needed with faction 1", type = "int"},
    RequiredFactionId2 = {label = "Reputation Faction 2", tooltip = "Faction whose standing is an objective", type = "int", ref = "dbc:Faction"},
    RequiredFactionValue2 = {label = "Reputation Value 2", tooltip = "Standing needed with faction 2", type = "int"},
    RequiredPlayerKills = {label = "Player Kills", tooltip = "Players of the other faction to kill", type = "int"},
    RewardNextQuest = {label = "Next Quest In Chain", tooltip = "Quest the giver offers right after turning this one in", type = "int", ref = "quest"},
    RewardXPDifficulty = {label = "XP Difficulty", tooltip = "Column 0 to 9 of QuestXP.dbc at the quest level, higher pays more experience", type = "int"},
    RewardMoney = {label = "Money", tooltip = "Copper rewarded, a negative value is money the quest takes", type = "int"},
    RewardBonusMoney = {label = "Max Level Money", tooltip = "Copper paid instead of experience at max level", type = "int"},
    RewardDisplaySpell = {label = "Displayed Spell", tooltip = "Spell shown as the reward and learned on turn in", type = "int", ref = "spell"},
    RewardSpell = {label = "Cast Spell", tooltip = "Spell cast on the player on turn in", type = "int", ref = "spell"},
    RewardHonor = {label = "Honor", tooltip = "Honor points rewarded", type = "int"},
    RewardKillHonor = {label = "Honor Multiplier", tooltip = "Honor of this many honorable kills at the player level", type = "float"},
    RewardTitle = {label = "Title Reward", tooltip = "Character title granted on turn in", type = "int", ref = "dbc:CharTitles"},
    RewardTalents = {label = "Talent Points", tooltip = "Extra talent points rewarded", type = "int"},
    RewardArenaPoints = {label = "Arena Points", type = "int"},
    POIContinent = {label = "POI Map", tooltip = "Map of the old style point of interest", type = "int", ref = "dbc:Map"},
    POIx = {label = "POI X", type = "float"},
    POIy = {label = "POI Y", type = "float"},
    POIPriority = {label = "POI Priority", type = "int"},
    Unknown0 = {label = "Unknown 0", tooltip = "Not read by the server", type = "int"},
    VerifiedBuild = {label = "Verified Build", type = "int"},
}

for slot = 1, 4 do
    Fields.quest_template["RequiredNpcOrGo"..slot] = {label = "Kill or Use "..slot, tooltip = "A positive value is a creature to kill or cast at, a negative value is a gameobject to use", type = "int", ref = "creature_or_gameobject"}
    Fields.quest_template["RequiredNpcOrGoCount"..slot] = {label = "Count "..slot, type = "int"}
    Fields.quest_template["ObjectiveText"..slot] = {label = "Objective Text "..slot, tooltip = "Replaces the creature or object name in the objective list", type = "string"}
    Fields.quest_template["ItemDrop"..slot] = {label = "Source Item "..slot, tooltip = "Item used for the quest that is taken away when the quest is done or abandoned", type = "int", ref = "item"}
    Fields.quest_template["ItemDropQuantity"..slot] = {label = "Source Count "..slot, type = "int"}
    Fields.quest_template["RewardItem"..slot] = {label = "Reward Item "..slot, tooltip = "Item every player gets", type = "int", ref = "item"}
    Fields.quest_template["RewardAmount"..slot] = {label = "Reward Count "..slot, type = "int"}
end

for slot = 1, 5 do
    Fields.quest_template["RewardFactionID"..slot] = {label = "Faction "..slot, type = "int", ref = "dbc:Faction"}
    Fields.quest_template["RewardFactionValue"..slot] = {label = "Value Index "..slot, tooltip = "Row of QuestFactionReward.dbc, a negative value takes reputation away", type = "int"}
    Fields.quest_template["RewardFactionOverride"..slot] = {label = "Override "..slot, tooltip = "Exact reputation in hundredths, 100 gives one point, it replaces the value index when not 0", type = "int"}
end

for slot = 1, 6 do
    Fields.quest_template["RequiredItemId"..slot] = {label = "Required Item "..slot, type = "int", ref = "item"}
    Fields.quest_template["RequiredItemCount"..slot] = {label = "Required Count "..slot, type = "int"}
    Fields.quest_template["RewardChoiceItemID"..slot] = {label = "Choice Item "..slot, tooltip = "The player picks one of the choice items", type = "int", ref = "item"}
    Fields.quest_template["RewardChoiceItemQuantity"..slot] = {label = "Choice Count "..slot, type = "int"}
end

Fields.quest_template_addon = {
    ID = {label = "Quest ID", type = "int"},
    MaxLevel = {label = "Max Level", tooltip = "Highest player level that can take the quest, 0 means no limit", type = "int"},
    AllowableClasses = {label = "Allowed Classes", tooltip = "Classes that can take the quest, 0 allows every class", type = "int", flags = "QuestClassMask"},
    SourceSpellID = {label = "Source Spell", tooltip = "Spell cast on the player when the quest is accepted", type = "int", ref = "spell"},
    PrevQuestID = {label = "Previous Quest", tooltip = "A positive id must be rewarded before, a negative id must be active at the same time", type = "int", ref = "quest"},
    NextQuestID = {label = "Next Quest", tooltip = "Quest that becomes available once this one is rewarded, only needed when the chain is not set by previous quest", type = "int", ref = "quest"},
    ExclusiveGroup = {label = "Exclusive Group", tooltip = "Quests sharing a positive group exclude each other, a negative group must all be done before the next quest", type = "int"},
    BreadcrumbForQuestId = {label = "Breadcrumb For", tooltip = "This quest leads to that quest and is hidden once that quest is taken or done", type = "int", ref = "quest"},
    RewardMailTemplateID = {label = "Mail Template", tooltip = "Mail sent after the quest is rewarded", type = "int", ref = "dbc:MailTemplate"},
    RewardMailDelay = {label = "Mail Delay (sec)", tooltip = "Seconds before the mail arrives", type = "int"},
    RewardMailSenderEntry = {label = "Mail Sender", tooltip = "Creature that sends the reward mail, stored in quest_mail_sender. 0 uses the quest ender", type = "int", ref = "creature"},
    RequiredSkillID = {label = "Required Skill", type = "int", ref = "dbc:SkillLine"},
    RequiredSkillPoints = {label = "Required Skill Value", type = "int"},
    RequiredMinRepFaction = {label = "Min Reputation Faction", type = "int", ref = "dbc:Faction"},
    RequiredMinRepValue = {label = "Min Reputation", tooltip = "Lowest standing with the faction to take the quest", type = "int"},
    RequiredMaxRepFaction = {label = "Max Reputation Faction", type = "int", ref = "dbc:Faction"},
    RequiredMaxRepValue = {label = "Max Reputation", tooltip = "Standing with the faction must stay below this to take the quest", type = "int"},
    ProvidedItemCount = {label = "Start Item Count", tooltip = "How many of the template start item the player gets", type = "int"},
    SpecialFlags = {label = "Special Flags", tooltip = "Server side quest flags", type = "int", flags = "QuestSpecialFlags"},
}

local EMOTE_DELAY = "Milliseconds after the previous emote before this one plays"

Fields.quest_details = {
    ID = {label = "Quest ID", type = "int"},
    VerifiedBuild = {label = "Verified Build", type = "int"},
}

Fields.quest_offer_reward = {
    ID = {label = "Quest ID", type = "int"},
    RewardText = {label = "Reward Text", tooltip = "Text the ender shows when the quest is turned in. "..TEXT_CODES, type = "string"},
    VerifiedBuild = {label = "Verified Build", type = "int"},
}

for slot = 1, 4 do
    Fields.quest_details["Emote"..slot] = {label = "Emote "..slot, tooltip = "Emote the giver plays while showing the quest", type = "int", enum = "Emote"}
    Fields.quest_details["EmoteDelay"..slot] = {label = "Delay "..slot, tooltip = EMOTE_DELAY, type = "int"}
    Fields.quest_offer_reward["Emote"..slot] = {label = "Emote "..slot, tooltip = "Emote the ender plays while showing the reward", type = "int", enum = "Emote"}
    Fields.quest_offer_reward["EmoteDelay"..slot] = {label = "Delay "..slot, tooltip = EMOTE_DELAY, type = "int"}
end

Fields.quest_request_items = {
    ID = {label = "Quest ID", type = "int"},
    EmoteOnComplete = {label = "Emote When Complete", tooltip = "Emote the ender plays when the objectives are done", type = "int", enum = "Emote"},
    EmoteOnIncomplete = {label = "Emote When Incomplete", tooltip = "Emote the ender plays when the objectives are not done yet", type = "int", enum = "Emote"},
    CompletionText = {label = "Progress Text", tooltip = "Text the ender shows while the quest is in progress. "..TEXT_CODES, type = "string"},
    VerifiedBuild = {label = "Verified Build", type = "int"},
}

for name, fields in pairs(Fields) do
    GameEditorData.Fields[name] = fields
end

local function Slots(count, ...)
    local columns = {}
    local prefixes = {...}

    for slot = 1, count do
        for _, prefix in ipairs(prefixes) do
            table.insert(columns, prefix..slot)
        end
    end

    return columns
end

local function Join(...)
    local columns = {}

    for _, list in ipairs({...}) do
        for _, column in ipairs(list) do
            table.insert(columns, column)
        end
    end

    return columns
end

GameEditorData.Groups.quest_template = {
    {title = "Basics", columns = {"ID", "LogTitle", "QuestType", "QuestLevel", "MinLevel", "QuestSortID", "QuestInfoID", "SuggestedGroupNum", "TimeAllowed", "Flags"}},
    {title = "Requirements", columns = {"AllowableRaces", "StartItem"}},
    {title = "Objectives", columns = Join(Slots(4, "RequiredNpcOrGo", "RequiredNpcOrGoCount", "ObjectiveText"), {"RequiredPlayerKills"})},
    {title = "Required Items", columns = Slots(6, "RequiredItemId", "RequiredItemCount")},
    {title = "Source Items", columns = Slots(4, "ItemDrop", "ItemDropQuantity"), collapsed = true},
    {title = "Reputation Objectives", columns = {"RequiredFactionId1", "RequiredFactionValue1", "RequiredFactionId2", "RequiredFactionValue2"}, collapsed = true},
    {title = "Rewards", columns = {"RewardXPDifficulty", "RewardMoney", "RewardBonusMoney", "RewardHonor", "RewardKillHonor", "RewardArenaPoints", "RewardTalents", "RewardTitle", "RewardDisplaySpell", "RewardSpell", "RewardNextQuest"}},
    {title = "Reward Items", columns = Slots(4, "RewardItem", "RewardAmount")},
    {title = "Choice Items", columns = Slots(6, "RewardChoiceItemID", "RewardChoiceItemQuantity")},
    {title = "Reputation Rewards", columns = Slots(5, "RewardFactionID", "RewardFactionValue", "RewardFactionOverride"), collapsed = true},
    {title = "Texts", columns = {"QuestDescription", "LogDescription", "AreaDescription", "QuestCompletionLog"}},
    {title = "Map Point", columns = {"POIContinent", "POIx", "POIy", "POIPriority"}, collapsed = true},
    {title = "Misc", columns = {"Unknown0", "VerifiedBuild"}, collapsed = true},
}

GameEditorData.Groups.quest_template_addon = {
    {title = "Chain", columns = {"ID", "PrevQuestID", "NextQuestID", "ExclusiveGroup", "BreadcrumbForQuestId"}},
    {title = "Requirements", columns = {"MaxLevel", "AllowableClasses", "RequiredSkillID", "RequiredSkillPoints", "RequiredMinRepFaction", "RequiredMinRepValue", "RequiredMaxRepFaction", "RequiredMaxRepValue"}},
    {title = "Start", columns = {"SourceSpellID", "ProvidedItemCount", "SpecialFlags"}},
    {title = "Reward Mail", columns = {"RewardMailTemplateID", "RewardMailDelay", "RewardMailSenderEntry"}},
}

GameEditorData.Groups.quest_details = {
    {columns = Slots(4, "Emote", "EmoteDelay")},
}

GameEditorData.Groups.quest_request_items = {
    {columns = {"EmoteOnComplete", "EmoteOnIncomplete", "CompletionText"}},
}

GameEditorData.Groups.quest_offer_reward = {
    {columns = Join(Slots(4, "Emote", "EmoteDelay"), {"RewardText"})},
}
