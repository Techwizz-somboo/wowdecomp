-- Server side spell data: the whitelisted world tables keyed on the spell id.

local SE = GameEditorSpell;

local SCRIPT_KIND_SPELL = 0;

SE.worldEditors = {};
SE.scriptNames = nil;

local function SpellLabel(id)
	id = tonumber(id) or 0;

	if ( id == 0 ) then
		return "0";
	end

	local name, rank = SE.SpellName(abs(id));
	return format("%d %s%s", id, name or "|cffff6060unknown|r", (rank and rank ~= "") and (" ("..rank..")") or "");
end

local function Value(row, column)
	return SE.Trim(tonumber(row[column]) or 0);
end

-- Script names ------------------------------------------------------------------------------------------

local scriptNameSet = {};

local function LoadScriptNames()
	if ( not WorldTable_GetScriptNames ) then
		return;
	end

	local ok, names = pcall(WorldTable_GetScriptNames, SCRIPT_KIND_SPELL);

	if ( ok and type(names) == "table" and #names > 0 ) then
		table.sort(names);
		SE.scriptNames = names;
		scriptNameSet = {};

		for _, name in ipairs(names) do
			scriptNameSet[name] = true;
		end
	end
end

function SE.RequestScriptNames(force)
	if ( SE.scriptNames and not force ) then
		return;
	end

	LoadScriptNames();

	if ( (force or not SE.scriptNames) and WorldTable_RequestScriptNames and not SE.scriptNamesRequested ) then
		SE.scriptNamesRequested = true;
		pcall(WorldTable_RequestScriptNames, SCRIPT_KIND_SPELL);
	end
end

SE.On("WORLD_SCRIPT_NAMES", function(kind)
	if ( tonumber(kind) == SCRIPT_KIND_SPELL ) then
		SE.scriptNamesRequested = false;
		LoadScriptNames();
		SE.Fire("SCRIPT_NAMES");
	end
end);

local function PickScript(editor)
	if ( not editor.loaded ) then
		return;
	end

	SE.RequestScriptNames();

	if ( not SE.scriptNames ) then
		GameEditor_Notify("Asking the server for its spell script names, try again in a moment.", "info");
		return;
	end

	local items = {};

	for _, name in ipairs(SE.scriptNames) do
		table.insert(items, { value = "", text = name });
	end

	GameEditor_PickValue(format("Spell scripts (%d)", #items), items, nil, function(_, item)
		if ( not editor:SelectedRow() ) then
			editor:AddRow();
		end

		editor:SetSelectedValue("ScriptName", item.text);
	end);
end

-- Table definitions -------------------------------------------------------------------------------------

local WORLD_TABLES = {
	{
		key = "bonus", title = "Bonus data", table = "spell_bonus_data", keyColumn = "entry",
		description = "Spell power and attack power coefficients the server uses instead of its own estimate. Direct is the hit, dot is per tick.",
		summary = function(row)
			local effect = row.effectIndex ~= nil and (SE.EnumText("SpellEffectIndex", row.effectIndex)..": ") or "";
			return format("%sdirect %s, dot %s, ap %s, ap dot %s", effect, Value(row, "direct_bonus"), Value(row, "dot_bonus"), Value(row, "ap_bonus"), Value(row, "ap_dot_bonus"));
		end,
	},
	{
		key = "proc", title = "Proc rules", table = "spell_proc", keyColumn = "SpellId",
		description = "Server side proc rules of this spell. They override the proc flags, chance and charges of the DBC row.",
		summary = function(row)
			return format("chance %s, ppm %s, cooldown %s ms, charges %s", Value(row, "Chance"), Value(row, "ProcsPerMinute"), Value(row, "Cooldown"), Value(row, "Charges"));
		end,
	},
	{
		key = "scripts", title = "Scripts", table = "spell_script_names", keyColumn = "spell_id",
		description = "C++ SpellScripts and AuraScripts bound to this spell. The name must be registered in the server core, pick it from the list.",
		summary = function(row)
			local name = tostring(row.ScriptName or "");

			if ( name == "" ) then
				return "|cffff6060(empty script name)|r";
			end

			if ( SE.scriptNames and not scriptNameSet[name] ) then
				return name.."  |cffff6060not registered on the server|r";
			end

			return name;
		end,
		buttons = {
			{ text = "Pick script", width = 90, tooltip = "Choose from the script names the server core registered", onClick = PickScript },
			{ text = "Reload names", width = 96, tooltip = "Ask the server for its script names again", onClick = function() SE.RequestScriptNames(true); end },
		},
	},
	{
		key = "linked", title = "Linked spells", table = "spell_linked_spell", keyColumn = "spell_trigger",
		description = "Casts or removes other spells when this one is cast, hits, or its aura is applied or removed. A negative linked spell removes that aura.",
		summary = function(row)
			local effect = tonumber(row.spell_effect) or 0;
			return format("%s %s  |cff8f8f8fon %s|r", effect < 0 and "removes" or "casts", SpellLabel(abs(effect)), SE.EnumText("SpellLinkedType", row.type));
		end,
	},
	{
		key = "ranks", title = "Ranks", table = "spell_ranks", keyColumn = "first_spell_id",
		description = "The whole rank chain this spell belongs to, one row per rank. Saving rewrites every rank of the chain. To add this spell to another chain, type that chain's first spell in first_spell_id and press Open chain.",
		defaults = function(row, key, editor)
			local first, rank, hasCurrent = nil, 0, false;
			local current = SE.cur and not SE.cur.isNew and SE.cur.id;

			for _, existing in ipairs(editor and editor.rows or {}) do
				first = first or tonumber(existing.first_spell_id);
				rank = max(rank, tonumber(existing.rank) or 0);
				hasCurrent = hasCurrent or (current and tonumber(existing.spell_id) == current);
			end

			row.first_spell_id = first or key;
			row.rank = rank + 1;
			row.spell_id = (current and not hasCurrent) and current or 0;
		end,
		summary = function(row)
			return format("rank %s: %s", tostring(row.rank or 0), SpellLabel(row.spell_id));
		end,
		buttons = {
			{ text = "Open chain", width = 90, tooltip = "Load the chain whose first spell is in first_spell_id of the selected row", onClick = function(editor)
				local row = editor:SelectedRow();
				local first = row and tonumber(row.first_spell_id) or 0;

				if ( first <= 0 ) then
					GameEditor_Notify("Type the first spell of the chain in first_spell_id first", "info");
					return;
				end

				editor:SetKey(first);
				editor:Query();
			end },
		},
	},
	{
		key = "area", title = "Area", table = "spell_area", keyColumn = "spell",
		description = "Applies the spell while the player is in the area and meets the quest, aura, race and gender conditions.",
		summary = function(row)
			return format("area %s, quests %s / %s, aura %s, autocast %s", tostring(row.area or 0), tostring(row.quest_start or 0), tostring(row.quest_end or 0),
				tostring(row.aura_spell or 0), SE.EnumText("YesNo", row.autocast));
		end,
	},
	{
		key = "position", title = "Target position", table = "spell_target_position", keyColumn = "ID",
		description = "Destination of teleport and summon effects that use a database position.",
		summary = function(row)
			return format("%s: map %s  %s, %s, %s", SE.EnumText("SpellEffectIndex", row.EffectIndex), tostring(row.MapID or 0), Value(row, "PositionX"), Value(row, "PositionY"), Value(row, "PositionZ"));
		end,
		buttons = {
			{ text = "Use my position", width = 110, tooltip = "Fill the map, X, Y, Z and orientation of the selected row from your character.", onClick = function(editor)
				if ( not CreatureEditor_GetPlayerPosition ) then
					return;
				end

				local x, y, z, o = CreatureEditor_GetPlayerPosition();

				if ( not x ) then
					return;
				end

				if ( not editor:SelectedRow() ) then
					editor:AddRow();
				end

				local row = editor:SelectedRow();

				if ( row ) then
					row.PositionX, row.PositionY, row.PositionZ, row.Orientation = x, y, z, o or 0;

					if ( GetCurrentMapId ) then
						row.MapID = GetCurrentMapId();
					end
					editor:SetSelectedValue("PositionX", x);
				end
			end },
		},
	},
	{
		key = "groups", title = "Groups", table = "spell_group", keyColumn = "spell_id",
		description = "Groups this spell with others for the stacking rules. The rule of the selected group is edited below.",
		summary = function(row)
			return format("group %s", tostring(row.id or 0));
		end,
		listHeight = 86,
	},
	{
		key = "threat", title = "Threat", table = "spell_threat", keyColumn = "entry",
		description = "Extra threat for this spell: a flat amount, a multiplier of the damage threat and a share of attack power.",
		defaults = function(row) row.pctMod = 1; end,
		summary = function(row)
			return format("flat %s, multiplier %s, attack power %s", Value(row, "flatMod"), Value(row, "pctMod"), Value(row, "apPctMod"));
		end,
	},
	{
		key = "learn", title = "Learn spell", table = "spell_learn_spell", keyColumn = "entry",
		description = "Spells learned together with this one.",
		defaults = function(row) row.Active = 1; end,
		summary = function(row)
			return format("also learns %s%s", SpellLabel(row.SpellID), (tonumber(row.Active) or 0) ~= 0 and "" or "  |cff8f8f8finactive|r");
		end,
	},
	{
		key = "petauras", title = "Pet auras", table = "spell_pet_auras", keyColumn = "spell",
		description = "Auras the pet gets while its owner has this spell. Pet 0 means every pet.",
		summary = function(row)
			return format("%s: pet %s gets %s", SE.EnumText("SpellEffectIndex", row.effectId), tostring(row.pet or 0), SpellLabel(row.aura));
		end,
	},
	{
		key = "required", title = "Required", table = "spell_required", keyColumn = "spell_id",
		description = "Spells that must be known before this one can be learned.",
		summary = function(row)
			return "requires "..SpellLabel(row.req_spell);
		end,
	},
	{
		key = "custom", title = "Custom group", table = "custom_spell_group", keyColumn = "spellId",
		description = "HoT custom spell group membership of this spell.",
	},
};

local function BuildWorld(definition)
	return function(parent)
		local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellServer"), parent);
		local editor = SE.CreateWorldTableEditor(frame, definition.table, {
			title = definition.title,
			description = definition.description,
			keyColumn = definition.keyColumn,
			summary = definition.summary,
			defaults = definition.defaults,
			buttons = definition.buttons,
			listHeight = definition.listHeight,
		});
		editor.frame:SetAllPoints(frame);
		definition.editor = editor;
		definition.editors = { editor };

		if ( definition.table == "spell_group" ) then
			editor.frame:ClearAllPoints();
			editor.frame:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
			editor.frame:SetPoint("BOTTOMRIGHT", frame, "RIGHT", 0, -10);

			local rules = SE.CreateWorldTableEditor(frame, "spell_group_stack_rules", {
				keyColumn = "group_id",
				listHeight = 46,
				summary = function(row)
					return "stack rule "..SE.EnumText("SpellGroupStackRule", row.stack_rule);
				end,
			});
			rules.frame:SetPoint("TOPLEFT", frame, "LEFT", 0, -16);
			rules.frame:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
			-- The rules restore first so selecting the restored group does not query them again.
			definition.editors = { rules, editor };
			editor.options.onSelect = function(_, row)
				local group = row and tonumber(row.id) or 0;

				if ( group > 0 ) then
					if ( rules.key ~= group ) then
						rules:SetKey(group);
						rules:Query();
					end
				else
					rules:SetKey(nil);
				end
			end;
		end

		if ( definition.table == "spell_script_names" ) then
			SE.On("SCRIPT_NAMES", function()
				if ( editor.loaded ) then
					editor:RefreshList();
				end
			end);
			frame:SetScript("OnShow", function()
				SE.RequestScriptNames();
			end);
		end

		if ( SE.cur ) then
			editor:SetKey(SE.cur.isNew and nil or SE.cur.id);
		end

		return frame;
	end;
end

for _, definition in ipairs(WORLD_TABLES) do
	SE.AddSection({
		key = definition.key,
		title = definition.title,
		group = "server",
		definition = definition,
		build = BuildWorld(definition),
		badge = function()
			local editor = definition.editor;

			if ( not editor or not editor.key ) then
				return nil;
			end

			if ( editor:IsDirty() ) then
				return "|cffffcc00unsaved|r";
			end

			if ( editor.loaded ) then
				return #editor.rows > 0 and tostring(#editor.rows) or nil;
			end
		end,
	});
end

SE.WORLD_TABLES = WORLD_TABLES;

-- Every server table follows the active spell tab, rows are asked for shortly after a tab becomes active.
local QUERY_DELAY = 0.4;

local function GroupDirty(definition)
	for _, editor in ipairs(definition.editors or {}) do
		if ( editor:IsDirty() ) then
			return true;
		end
	end

	return false;
end

local function GroupSaving(definition)
	for _, editor in ipairs(definition.editors or {}) do
		if ( editor.saving ) then
			return true;
		end
	end

	return false;
end

-- Unsaved rows stay with the tab that is left, clean tables are asked for again when the tab comes back.
local function StashWorld(doc)
	doc.world = {};

	for _, definition in ipairs(WORLD_TABLES) do
		if ( GroupDirty(definition) and not GroupSaving(definition) ) then
			local states = {};

			for index, editor in ipairs(definition.editors) do
				states[index] = editor:CaptureState();
			end

			doc.world[definition.key] = states;
		end
	end
end

local function RestoreWorld(doc, key)
	for _, definition in ipairs(WORLD_TABLES) do
		local states = doc and doc.world and doc.world[definition.key];

		if ( states ) then
			for index, editor in ipairs(definition.editors) do
				if ( states[index] ) then
					editor:RestoreState(states[index]);
				end
			end
		elseif ( definition.editor ) then
			definition.editor:SetKey(key);
		end
	end

	if ( doc ) then
		doc.world = {};
	end
end

SE.On("SPELL_LOADED", function(cur)
	local key = (cur and not cur.isNew) and cur.id or nil;
	local previous = SE.worldOwner;

	if ( previous ~= cur ) then
		if ( previous and SE.DocIndex(previous) ) then
			StashWorld(previous);
		end

		SE.worldOwner = cur;
		RestoreWorld(cur, key);
	else
		for _, definition in ipairs(WORLD_TABLES) do
			if ( definition.editor and definition.editor.key ~= key ) then
				definition.editor:SetKey(key);
			end
		end
	end

	SE.worldQueryAt = key and QUERY_DELAY or nil;
end);

-- The active tab's rows live in the editors, the others in doc.world.
local function EachWorldState(doc, callback)
	if ( not doc ) then
		return;
	end

	for _, definition in ipairs(WORLD_TABLES) do
		local states = doc.world and doc.world[definition.key];

		for index, editor in ipairs(definition.editors or {}) do
			if ( doc == SE.worldOwner ) then
				if ( editor:IsDirty() ) then
					callback(editor, nil);
				end
			elseif ( states and states[index] and editor:IsStateDirty(states[index]) ) then
				callback(editor, states[index]);
			end
		end
	end
end

function SE.DocWorldDirty(doc)
	local dirty = false;

	EachWorldState(doc, function()
		dirty = true;
	end);

	return dirty;
end

function SE.DocWorldDirtyParts(doc)
	local parts = {};

	EachWorldState(doc, function(editor, state)
		table.insert(parts, state and editor:StateDirtyText(state) or editor:DirtyText());
	end);

	return parts;
end

function SE.SaveDocWorld(doc)
	EachWorldState(doc, function(editor, state)
		if ( state ) then
			editor:SaveState(state);
		else
			editor:Save();
		end
	end);

	if ( doc and doc ~= SE.worldOwner ) then
		doc.world = {};
	end
end

SE.On("SPELL_SAVED", function(id, doc)
	if ( doc and doc ~= SE.cur ) then
		return;
	end

	for _, definition in ipairs(WORLD_TABLES) do
		local editor = definition.editor;

		if ( editor and not editor.key ) then
			editor:SetKey(id);
			editor:Query();
		end
	end
end);

function SE.UpdateWorldQueries(elapsed)
	if ( not SE.worldQueryAt ) then
		return;
	end

	SE.worldQueryAt = SE.worldQueryAt - elapsed;

	if ( SE.worldQueryAt > 0 ) then
		return;
	end

	SE.worldQueryAt = nil;

	for _, definition in ipairs(WORLD_TABLES) do
		local editor = definition.editor;

		if ( editor ) then
			editor:EnsureLoaded();
		end
	end
end

-- Builds every server section so the sidebar can show row counts without visiting each tab.
function SE.BuildServerSections(builder)
	for _, section in ipairs(SE.SECTIONS) do
		if ( section.group == "server" ) then
			builder(section);
		end
	end
end
