-- Editor sections of the spell DBC row: grids for the plain groups, the attribute list and the effect cards.

local SE = GameEditorSpell;
local Data = GameEditorData or {};

SE.SECTIONS = SE.SECTIONS or {};

local function GroupsByTitle(titles)
	local result = {};

	for _, title in ipairs(titles) do
		for _, group in ipairs(Data.Groups and Data.Groups.Spell or {}) do
			if ( group.title == title ) then
				table.insert(result, { title = group.title, columns = group.columns, collapsed = group.collapsed });
			end
		end
	end

	return result;
end

SE.GroupsByTitle = GroupsByTitle;

local function ColumnsOf(groups)
	local columns = {};

	for _, group in ipairs(groups) do
		for _, column in ipairs(group.columns) do
			table.insert(columns, column);
		end
	end

	return columns;
end

SE.ColumnsOf = ColumnsOf;

function SE.AddSection(section)
	table.insert(SE.SECTIONS, section);
	return section;
end

-- Context menu for spell grid rows ------------------------------------------------------------------

local contextColumn;

local function FieldOf(column)
	return Data.Fields and Data.Fields.Spell and Data.Fields.Spell[column] or {};
end

local FIELD_MENU = {
	{ text = "Revert this field", onClick = function()
		if ( contextColumn and SE.cur ) then
			SE.SetValue(contextColumn, SE.cur.original[contextColumn]);
		end
	end },
	{ text = "Set to zero or empty", onClick = function()
		if ( contextColumn and SE.cur ) then
			SE.SetValue(contextColumn, FieldOf(contextColumn).type == "string" and "" or 0);
		end
	end },
	{ separator = true },
	{ text = "Open the referenced spell", onClick = function()
		local value = contextColumn and tonumber(SE.GetValue(contextColumn));

		if ( value and value > 0 and FieldOf(contextColumn).ref == "spell" ) then
			SE.Open(value);
		end
	end },
};

local function FieldContext(_, column)
	contextColumn = column;
	NoggitContextMenu_Show("GameEditorSpellFieldContext", FIELD_MENU);
end

-- Grid bound to the open spell. Edits go straight into the shared values table.
function SE.CreateSpellGrid(parent, groups, options)
	options = options or {};
	local grid = GameEditor_CreatePropertyGrid(parent, {
		table = SE.TABLE,
		groups = groups,
		readOnly = { ID = true },
		filter = options.filter,
		visible = options.visible,
		labelWidth = options.labelWidth or 190,
		onChange = function(self, column)
			SE.Changed(column, self);
		end,
		onRowContext = FieldContext,
	});
	SE.RegisterGrid(grid);
	return grid;
end

-- A section that is a title line plus one spell grid.
local function GridSection(key, title, description, groups, options)
	return SE.AddSection({
		key = key,
		title = title,
		group = "spell",
		columns = ColumnsOf(groups),
		build = function(parent)
			local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellSection"), parent);
			local y = SE.SectionTitle(frame, title, description, 0);
			local grid = SE.CreateSpellGrid(frame, groups, options);
			grid.frame:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y - 2);
			grid.frame:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
			frame.grid = grid;
			frame:SetScript("OnShow", function() SE.SyncGrid(grid); end);
			return frame;
		end,
	});
end

-- General ----------------------------------------------------------------------------------------------

GridSection("general", "General", "Identity, casting, cost and the conditions the caster and target must meet.",
	GroupsByTitle({ "Base", "Casting", "Power and Cost", "Requirements", "Misc" }));

-- Attributes -----------------------------------------------------------------------------------------

local ATTRIBUTE_COLUMNS = {
	{ column = "Attributes", flags = "SpellAttr0", title = "Attributes 0" },
	{ column = "AttributesEx", flags = "SpellAttr1", title = "Attributes 1" },
	{ column = "AttributesEx2", flags = "SpellAttr2", title = "Attributes 2" },
	{ column = "AttributesEx3", flags = "SpellAttr3", title = "Attributes 3" },
	{ column = "AttributesEx4", flags = "SpellAttr4", title = "Attributes 4" },
	{ column = "AttributesEx5", flags = "SpellAttr5", title = "Attributes 5" },
	{ column = "AttributesEx6", flags = "SpellAttr6", title = "Attributes 6" },
	{ column = "AttributesEx7", flags = "SpellAttr7", title = "Attributes 7" },
};

local function HexValue(value)
	value = tonumber(value) or 0;

	if ( value < 0 ) then
		value = value + 4294967296;
	end

	return format("0x%08X", value);
end

local function BuildAttributes(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellAttributes", parent);
	local y = SE.SectionTitle(frame, "Attributes", "Every attribute bit of the eight attribute words. Hover a flag for what it does, the grey text is the core constant.", 0);

	frame.words = {};
	frame.onlySet = false;
	frame.filter = "";

	local search = SE.SearchBox(frame, 240, "Filter attributes", function(text)
		frame.filter = SE.Lower(strtrim(text));
		frame:Layout();
	end);
	search:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	search:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);

	local onlySet = NoggitCheckBox_Create(frame, "Only set flags", false, function(_, checked)
		frame.onlySet = checked;
		frame:Layout();
	end);
	onlySet:SetPoint("LEFT", search, "RIGHT", 12, 0);

	local expand = SE.Button(frame, "Expand all", 80, function()
		for _, word in ipairs(frame.words) do
			word.collapsed = false;
		end

		frame:Layout();
	end);
	expand:SetPoint("LEFT", onlySet, "RIGHT", 12, 0);
	local collapse = SE.Button(frame, "Collapse all", 84, function()
		for _, word in ipairs(frame.words) do
			word.collapsed = true;
		end

		frame:Layout();
	end);
	collapse:SetPoint("LEFT", expand, "RIGHT", 4, 0);

	frame.summary = SE.Label(frame, "", nil, NOGGIT_COLOR_DISABLED);
	frame.summary:SetPoint("LEFT", collapse, "RIGHT", 12, 0);

	local scroll, child = GameEditor_CreateScrollFrame(frame);
	scroll:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y - 28);
	scroll:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);
	frame.scroll = scroll;
	frame.child = child;

	for _, definition in ipairs(ATTRIBUTE_COLUMNS) do
		local word = { column = definition.column, title = definition.title, rows = {} };
		local byBit = {};

		for _, flag in ipairs(Data.Flags and Data.Flags[definition.flags] or {}) do
			byBit[flag.bit] = flag;
		end

		local header = CreateFrame("Button", NoggitUI_NewName("GameEditorSpellAttrHeader"), child, "NoggitBoxTemplate");
		header:SetHeight(26);
		NoggitBox_Set(header, NOGGIT_COLOR_MID, nil);
		header.arrow = NoggitUI_CreateCaret(header);
		header.arrow:SetPoint("LEFT", header, "LEFT", 8, 0);
		header.title = SE.Label(header, format("%s  |cff8f8f8f%s|r", definition.title, definition.column), "NoggitFontBold");
		header.title:SetPoint("LEFT", header, "LEFT", 24, 0);
		header.count = SE.Label(header, "", nil, NOGGIT_COLOR_DISABLED);
		header.count:SetPoint("RIGHT", header, "RIGHT", -110, 0);
		header.hex = NoggitLineEdit_Create(header, 96);
		header.hex:SetHeight(20);
		header.hex:SetPoint("RIGHT", header, "RIGHT", -4, 0);
		header.hex:SetScript("OnEnterPressed", function(self)
			self:ClearFocus();
		end);
		header.hex:SetScript("OnEditFocusLost", function(self)
			local text = self:GetText();
			local value = text ~= self.shownText and GameEditor_ParseNumber(text);

			if ( value ) then
				value = floor(value);
				local current = SE.Number(SE.GetValue(word.column));

				-- Same bits as the current value, the hex view is unsigned and the column is signed.
				if ( value >= 2147483648 and value < 4294967296 and current < 0 ) then
					value = value - 4294967296;
				end

				if ( value ~= current ) then
					SE.SetValue(word.column, value, frame);
				end
			end

			frame:Refresh();
		end);
		SE.Tooltip(header.hex, "Raw value, hex with 0x or decimal");
		header:SetScript("OnClick", function()
			word.collapsed = not word.collapsed;
			frame:Layout();
		end);
		header:SetScript("OnEnter", function(self) NoggitBox_Set(self, NOGGIT_COLOR_HOVER, nil); end);
		header:SetScript("OnLeave", function(self) NoggitBox_Set(self, NOGGIT_COLOR_MID, nil); end);
		word.header = header;

		for bit = 0, 31 do
			local flag = byBit[bit] or { bit = bit, name = "Unused bit "..bit };
			local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellAttrRow"), child);
			row:SetHeight(22);
			row.bit = bit;
			row.flag = flag;
			row.bg = row:CreateTexture(nil, "BACKGROUND");
			row.bg:SetAllPoints(row);
			row.bg:SetTexture(0, 0, 0, 0);

			local title = flag.alias or flag.name or ("Bit "..bit);
			local subtitle = (flag.alias and flag.name and flag.name ~= flag.alias) and flag.name or nil;
			row.box = NoggitCheckBox_Create(row, title, false, function(_, checked)
				local value = SE.Number(SE.GetValue(word.column));
				SE.SetValue(word.column, GameEditor_SetBit(value, bit, checked), frame);
				frame:RefreshWord(word);
			end);
			row.box:SetPoint("LEFT", row, "LEFT", 14, 0);
			row.box.tooltip = format("%s\n|cff8f8f8fbit %d  0x%X  %s|r%s%s", title, bit, 2 ^ bit, flag.const or "",
				subtitle and ("\n\n"..subtitle) or "", flag.tooltip and ("\n\n"..flag.tooltip) or "");

			row.detail = SE.Label(row, format("%s%s", subtitle and (subtitle.."   ") or "", flag.const or ""), "NoggitFont10", NOGGIT_COLOR_DISABLED, "RIGHT");
			row.detail:SetPoint("RIGHT", row, "RIGHT", -8, 0);
			row.detail:SetPoint("LEFT", row.box, "RIGHT", 10, 0);
			row.search = SE.Lower(table.concat({ title, flag.name or "", flag.const or "", flag.tooltip or "", "bit "..bit }, " "));
			table.insert(word.rows, row);
		end

		table.insert(frame.words, word);
	end

	frame.RefreshWord = function(self, word)
		local value = SE.Number(SE.GetValue(word.column));
		local count = 0;
		local original = SE.cur and SE.Number(SE.cur.original[word.column]) or value;

		for _, row in ipairs(word.rows) do
			local set = GameEditor_HasBit(value, row.bit);
			NoggitCheckBox_SetChecked(row.box, set);

			if ( set ) then
				count = count + 1;
			end

			if ( set ~= GameEditor_HasBit(original, row.bit) ) then
				row.bg:SetTexture(NOGGIT_COLOR_ACCENT[1], NOGGIT_COLOR_ACCENT[2], NOGGIT_COLOR_ACCENT[3], 0.22);
			else
				row.bg:SetTexture(0, 0, 0, 0);
			end
		end

		word.count = count;
		word.header.count:SetText(format("%d set", count));

		if ( not word.header.hex:HasFocus() ) then
			word.header.hex.shownText = HexValue(value);
			word.header.hex:SetText(word.header.hex.shownText);
		end
	end;

	frame.Refresh = function(self)
		local total = 0;

		for _, word in ipairs(self.words) do
			self:RefreshWord(word);
			total = total + word.count;
		end

		self.summary:SetText(format("%d attributes set", total));
		self.stamp = SE.stamp;

		if ( self.onlySet ) then
			self:Layout();
		end
	end;

	frame.Layout = function(self)
		local y = 0;
		local width = self.child:GetWidth();
		local filtering = self.filter ~= "" or self.onlySet;

		for _, word in ipairs(self.words) do
			local value = SE.Number(SE.GetValue(word.column));
			local visible = {};

			for _, row in ipairs(word.rows) do
				local show = true;

				if ( self.filter ~= "" and not string.find(row.search, self.filter, 1, true) ) then
					show = false;
				end

				if ( show and self.onlySet and not GameEditor_HasBit(value, row.bit) ) then
					show = false;
				end

				if ( show ) then
					table.insert(visible, row);
				else
					row:Hide();
				end
			end

			if ( filtering and #visible == 0 ) then
				word.header:Hide();
			else
				word.header:ClearAllPoints();
				word.header:SetPoint("TOPLEFT", self.child, "TOPLEFT", 0, -y);
				word.header:SetPoint("TOPRIGHT", self.child, "TOPRIGHT", 0, -y);
				word.header:Show();
				local open = not word.collapsed or filtering;
				word.header.arrow:SetOpen(open);
				y = y + 28;

				for _, row in ipairs(visible) do
					if ( open ) then
						row:ClearAllPoints();
						row:SetPoint("TOPLEFT", self.child, "TOPLEFT", 0, -y);
						row:SetPoint("TOPRIGHT", self.child, "TOPRIGHT", 0, -y);
						row:Show();
						y = y + 22;
					else
						row:Hide();
					end
				end

				y = y + 4;
			end
		end

		self.child:SetHeight(max(10, y));
		self.scroll.UpdateRange();
	end;

	frame:SetScript("OnShow", function(self)
		if ( self.stamp ~= SE.stamp ) then
			self:Refresh();
		end

		self:Layout();
	end);

	SE.On("VALUE_CHANGED", function(column, source)
		if ( source ~= frame and frame:IsVisible() and string.find(column or "", "^Attributes") ) then
			frame:Refresh();
		end
	end);
	SE.On("SPELL_LOADED", function()
		if ( frame:IsVisible() ) then
			frame:Refresh();
			frame:Layout();
		end
	end);

	frame.stamp = -1;
	return frame;
end

local attributeColumns = {};

for _, definition in ipairs(ATTRIBUTE_COLUMNS) do
	table.insert(attributeColumns, definition.column);
end

SE.AddSection({ key = "attributes", title = "Attributes", group = "spell", columns = attributeColumns, build = BuildAttributes });

-- Targeting --------------------------------------------------------------------------------------------

do
	local groups = GroupsByTitle({ "Targeting" });
	local targeting = { title = "Targeting", columns = {} };

	for _, column in ipairs(groups[1] and groups[1].columns or {}) do
		table.insert(targeting.columns, column);
	end

	table.insert(targeting.columns, "RangeIndex");
	groups = { targeting };

	for index = 1, 3 do
		table.insert(groups, {
			title = "Effect "..index.." targets",
			columns = { "EffectImplicitTargetA"..index, "EffectImplicitTargetB"..index, "EffectRadiusIndex"..index, "EffectChainTarget"..index },
			when = function(values) return SE.Number(values["Effect"..index]) ~= 0; end,
		});
	end

	GridSection("targeting", "Targeting", "Who the spell can be cast on, and the implicit targets of each effect. Effects that do nothing are left out.", groups);
end

-- Effects ----------------------------------------------------------------------------------------------

local CLASS_MASK_LETTERS = { "A", "B", "C" };

function SE.EffectGroups(index)
	local letter = CLASS_MASK_LETTERS[index];

	return {
		{ title = "Type", columns = { "Effect"..index, "EffectApplyAuraName"..index, "EffectMechanic"..index } },
		{ title = "Value", columns = { "EffectBasePoints"..index, "EffectDieSides"..index, "EffectRealPointsPerLevel"..index, "EffectPointsPerComboPoint"..index,
			"EffectMultipleValue"..index, "EffectDamageMultiplier"..index, "EffectBonusMultiplier"..index } },
		{ title = "Targets", columns = { "EffectImplicitTargetA"..index, "EffectImplicitTargetB"..index, "EffectRadiusIndex"..index, "EffectChainTarget"..index } },
		{ title = "Parameters", columns = { "EffectAmplitude"..index, "EffectMiscValue"..index, "EffectMiscValueB"..index, "EffectTriggerSpell"..index, "EffectItemType"..index } },
		{ title = "Spell class mask", columns = { "EffectSpellClassMask"..letter.."1", "EffectSpellClassMask"..letter.."2", "EffectSpellClassMask"..letter.."3" } },
	};
end

function SE.EffectColumns(index)
	return ColumnsOf(SE.EffectGroups(index));
end

local function MiscInfo(values, index)
	local effect = SE.Number(values["Effect"..index]);
	local aura = SE.Number(values["EffectApplyAuraName"..index]);
	local misc = Data.MiscValue or {};
	return (aura ~= 0 and misc.Aura and misc.Aura[aura]) or (misc.Effect and misc.Effect[effect]);
end

local function MiscText(info, slot, value)
	if ( not info or not info[slot] ) then
		return nil;
	end

	local meaning;

	if ( info[slot.."Enum"] ) then
		meaning = SE.EnumText(info[slot.."Enum"], value);
	elseif ( info[slot.."Flags"] ) then
		local names = {};

		for _, flag in ipairs(Data.Flags and Data.Flags[info[slot.."Flags"]] or {}) do
			if ( GameEditor_HasBit(value, flag.bit) ) then
				table.insert(names, flag.alias or flag.name);
			end
		end

		meaning = #names > 0 and table.concat(names, ", ") or "none";
	elseif ( info[slot.."Ref"] ) then
		meaning = GameEditor_ResolveRef(info[slot.."Ref"], value);
	end

	return format("%s %s%s", info[slot], tostring(value), meaning and ("  |cffffffff"..meaning.."|r") or "");
end

local function ClassMaskText(values, index)
	local family = SE.Number(values.SpellFamilyName);
	local names = Data.ClassMask and Data.ClassMask[family];
	local letter = CLASS_MASK_LETTERS[index];
	local found = {};
	local any = false;

	for word = 0, 2 do
		local mask = SE.Number(values["EffectSpellClassMask"..letter..(word + 1)]);

		if ( mask ~= 0 ) then
			any = true;

			for bit = 0, 31 do
				if ( GameEditor_HasBit(mask, bit) ) then
					local name = names and names[word * 32 + bit];
					table.insert(found, name and (type(name) == "table" and name.name or tostring(name)) or format("bit %d", word * 32 + bit));
				end
			end
		end
	end

	if ( not any ) then
		return nil;
	end

	return "Affects "..table.concat(found, ", ");
end

-- Short description of an effect for the chips and the card header.
function SE.EffectSummary(values, index)
	local effect = SE.Number(values["Effect"..index]);

	if ( effect == 0 ) then
		return "No effect", nil;
	end

	local name = SE.EnumText("SpellEffect", effect);
	local aura = SE.Number(values["EffectApplyAuraName"..index]);

	if ( aura ~= 0 ) then
		name = name..": "..SE.EnumText("SpellAura", aura);
	end

	return name, SE.EffectRangeText(values, index);
end

local function BuildEffects(parent)
	local frame = CreateFrame("Frame", "GameEditorSpellEffects", parent);
	local y = SE.SectionTitle(frame, "Effects", "Each spell has three effect slots. The value range and the misc value meanings follow the effect and aura type.", 0);

	frame.selected = 1;
	frame.showAll = false;
	frame.chips = {};
	frame.grids = {};

	local chipHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellEffectChips"), frame);
	chipHolder:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, y);
	chipHolder:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, y);
	chipHolder:SetHeight(58);

	for index = 1, 3 do
		local chip = CreateFrame("Button", NoggitUI_NewName("GameEditorSpellEffectChip"), chipHolder, "NoggitBoxTemplate");
		chip:SetHeight(58);
		chip.index = index;
		chip.title = SE.Label(chip, "Effect "..index, "NoggitFontBold", NOGGIT_COLOR_WHITE);
		chip.title:SetPoint("TOPLEFT", chip, "TOPLEFT", 10, -6);
		chip.name = SE.Label(chip, "");
		chip.name:SetPoint("TOPLEFT", chip, "TOPLEFT", 10, -23);
		chip.name:SetPoint("RIGHT", chip, "RIGHT", -8, 0);
		chip.value = SE.Label(chip, "", nil, NOGGIT_COLOR_DISABLED);
		chip.value:SetPoint("TOPLEFT", chip, "TOPLEFT", 10, -39);
		chip.value:SetPoint("RIGHT", chip, "RIGHT", -8, 0);
		chip.stripe = NoggitUI_SolidTexture(chip, "ARTWORK", NOGGIT_COLOR_ACCENT);
		chip.stripe:SetHeight(3);
		chip.stripe:SetPoint("TOPLEFT", chip, "TOPLEFT", 0, 0);
		chip.stripe:SetPoint("TOPRIGHT", chip, "TOPRIGHT", 0, 0);
		chip:SetScript("OnClick", function(self) frame:Select(self.index); end);
		chip:SetScript("OnEnter", function(self) self.hover = true; frame:RefreshChips(); end);
		chip:SetScript("OnLeave", function(self) self.hover = false; frame:RefreshChips(); end);
		frame.chips[index] = chip;
	end

	local function LayoutChips()
		local chipWidth = max(60, floor(((chipHolder:GetWidth() or 300) - 12) / 3));

		for index, chip in ipairs(frame.chips) do
			chip:SetWidth(chipWidth);
			chip:ClearAllPoints();
			chip:SetPoint("TOPLEFT", chipHolder, "TOPLEFT", (index - 1) * (chipWidth + 6), 0);
		end
	end

	chipHolder:SetScript("OnSizeChanged", LayoutChips);

	local card = SE.Box(frame, "GameEditorSpellEffectCard", NOGGIT_COLOR_DARK, NOGGIT_COLOR_MID);
	card:SetPoint("TOPLEFT", chipHolder, "BOTTOMLEFT", 0, -8);
	card:SetPoint("TOPRIGHT", chipHolder, "BOTTOMRIGHT", 0, -8);
	card:SetHeight(112);
	frame.card = card;

	card.lines = {};

	for line = 1, 4 do
		local text = SE.Label(card, "");
		text:SetPoint("TOPLEFT", card, "TOPLEFT", 10, -8 - (line - 1) * 17);
		text:SetPoint("RIGHT", card, "RIGHT", -10, 0);
		card.lines[line] = text;
	end

	NoggitText_SetColor(card.lines[1], NOGGIT_COLOR_WHITE);

	local buttons = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellEffectButtons"), card);
	buttons:SetHeight(24);
	buttons:SetPoint("BOTTOMLEFT", card, "BOTTOMLEFT", 8, 6);
	buttons:SetPoint("BOTTOMRIGHT", card, "BOTTOMRIGHT", -8, 6);

	frame.openTrigger = SE.Button(buttons, "Open trigger spell", 120, function()
		local id = SE.Number(SE.GetValue("EffectTriggerSpell"..frame.selected));

		if ( id > 0 ) then
			SE.Open(id);
		end
	end, "Opens the spell this effect triggers");
	frame.openTrigger:SetPoint("LEFT", buttons, "LEFT", 0, 0);

	local copyMenus = {};

	local function CopyEffect(from, to, swap)
		local source = SE.EffectColumns(from);
		local target = SE.EffectColumns(to);
		local values = SE.cur and SE.cur.values;

		if ( not values ) then
			return;
		end

		for position, column in ipairs(source) do
			local value = values[column];

			if ( swap ) then
				values[column] = values[target[position]];
			end

			values[target[position]] = value;
		end

		SE.Changed("Effect"..to);
		frame:Select(to);
	end

	for from = 1, 3 do
		local definition = {};

		for to = 1, 3 do
			if ( to ~= from ) then
				table.insert(definition, { text = format("Copy effect %d over effect %d", from, to), onClick = function() CopyEffect(from, to, false); end });
				table.insert(definition, { text = format("Swap effect %d with effect %d", from, to), onClick = function() CopyEffect(from, to, true); end });
			end
		end

		table.insert(definition, { separator = true });
		table.insert(definition, { text = format("Clear effect %d", from), onClick = function()
			if ( SE.cur ) then
				for _, column in ipairs(SE.EffectColumns(from)) do
					SE.cur.values[column] = 0;
				end

				SE.cur.values["EffectDamageMultiplier"..from] = 1;
				SE.cur.values["EffectBonusMultiplier"..from] = 1;
				SE.Changed("Effect"..from);
			end
		end });
		copyMenus[from] = definition;
	end

	local copy = SE.Button(buttons, "Copy, swap or clear", 130, function()
		NoggitContextMenu_Show("GameEditorSpellEffectMenu"..frame.selected, copyMenus[frame.selected]);
	end);
	copy:SetPoint("LEFT", frame.openTrigger, "RIGHT", 4, 0);

	local showAll = NoggitCheckBox_Create(buttons, "Show unused fields", false, function(_, checked)
		frame.showAll = checked;

		for _, grid in pairs(frame.grids) do
			grid:Layout();
		end
	end);
	showAll:SetPoint("RIGHT", buttons, "RIGHT", 0, 0);

	local gridHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellEffectGrids"), frame);
	gridHolder:SetPoint("TOPLEFT", card, "BOTTOMLEFT", 0, -6);
	gridHolder:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0);

	local function Visible(index)
		local effectColumn = "Effect"..index;
		local auraColumn = "EffectApplyAuraName"..index;

		return function(column, values)
			if ( frame.showAll or column == effectColumn ) then
				return true;
			end

			local value = values[column];
			local zero = (tonumber(value) or 0) == 0;
			local effect = SE.Number(values[effectColumn]);

			if ( effect == 0 ) then
				return not zero;
			end

			if ( column == auraColumn ) then
				local info = Data.MiscValue and Data.MiscValue.Effect and Data.MiscValue.Effect[effect];
				return not zero or (info and info.usesAura) or false;
			end

			return true;
		end;
	end

	frame.Grid = function(self, index)
		if ( not self.grids[index] ) then
			local grid = SE.CreateSpellGrid(gridHolder, SE.EffectGroups(index), { visible = Visible(index), labelWidth = 200 });
			grid.frame:SetAllPoints(gridHolder);
			self.grids[index] = grid;
		end

		return self.grids[index];
	end;

	frame.RefreshChips = function(self)
		local values = SE.cur and SE.cur.values or {};

		for index, chip in ipairs(self.chips) do
			local name, value = SE.EffectSummary(values, index);
			local changed = SE.ChangeCount(SE.EffectColumns(index));
			chip.title:SetText(format("Effect %d%s", index, changed > 0 and format("  |cffffcc00%d changed|r", changed) or ""));
			chip.name:SetText(name);
			chip.value:SetText(value and ("Value "..value) or "");
			NoggitText_SetColor(chip.name, SE.Number(values["Effect"..index]) == 0 and NOGGIT_COLOR_DISABLED or NOGGIT_COLOR_TEXT);

			if ( index == self.selected ) then
				NoggitBox_Set(chip, NOGGIT_COLOR_BASE, NOGGIT_COLOR_ACCENT);
				chip.stripe:Show();
			else
				NoggitBox_Set(chip, chip.hover and NOGGIT_COLOR_HOVER or NOGGIT_COLOR_MID, nil);
				chip.stripe:Hide();
			end
		end
	end;

	frame.RefreshCard = function(self)
		local values = SE.cur and SE.cur.values or {};
		local index = self.selected;
		local effect = SE.Number(values["Effect"..index]);
		local lines = {};

		if ( effect == 0 ) then
			lines[1] = format("Effect %d does nothing. Pick an effect type in the first row.", index);
		else
			local name, value = SE.EffectSummary(values, index);
			local text = format("%s   |cff8f8f8fvalue|r %s", name, value);
			local amplitude = SE.Number(values["EffectAmplitude"..index]);
			local duration = SE.DurationInfo(SE.Number(values.DurationIndex)) or 0;

			if ( amplitude > 0 and duration > 0 ) then
				local ticks = floor(duration / amplitude);
				text = text..format("   |cff8f8f8f%d ticks every %s sec, total|r %s", ticks, SE.Trim(amplitude / 1000), SE.EffectRangeText(values, index, ticks));
			end

			lines[1] = text;

			local targetA = SE.EnumText("SpellImplicitTarget", SE.Number(values["EffectImplicitTargetA"..index]));
			local targetB = SE.EnumText("SpellImplicitTarget", SE.Number(values["EffectImplicitTargetB"..index]));
			local _, radius = SE.RadiusInfo(SE.Number(values["EffectRadiusIndex"..index]));
			local chain = SE.Number(values["EffectChainTarget"..index]);
			lines[2] = format("|cff8f8f8fTargets|r %s / %s%s%s", targetA, targetB,
				radius and ("   |cff8f8f8fradius|r "..radius) or "", chain > 0 and ("   |cff8f8f8fchain|r "..chain) or "");

			local info = MiscInfo(values, index);
			local misc = {};
			local miscA = MiscText(info, "a", SE.Number(values["EffectMiscValue"..index]));
			local miscB = MiscText(info, "b", SE.Number(values["EffectMiscValueB"..index]));

			if ( miscA ) then
				table.insert(misc, miscA);
			end

			if ( miscB ) then
				table.insert(misc, miscB);
			end

			local trigger = SE.Number(values["EffectTriggerSpell"..index]);

			if ( trigger ~= 0 ) then
				local triggerName, triggerRank = SE.SpellName(trigger);
				table.insert(misc, format("|cff8f8f8fTriggers|r %d %s%s", trigger, triggerName or "|cffff6060missing|r",
					(triggerRank and triggerRank ~= "") and (" ("..triggerRank..")") or ""));
			end

			lines[3] = #misc > 0 and table.concat(misc, "   ") or nil;
			lines[4] = ClassMaskText(values, index);
		end

		for line = 1, 4 do
			self.card.lines[line]:SetText(lines[line] or "");
		end

		SE.SetEnabled(self.openTrigger, SE.Number(values["EffectTriggerSpell"..index]) > 0);
	end;

	frame.Select = function(self, index)
		self.selected = index;

		for other, grid in pairs(self.grids) do
			if ( other ~= index ) then
				grid.frame:Hide();
			end
		end

		local grid = self:Grid(index);
		grid.frame:Show();
		SE.SyncGrid(grid);
		self:RefreshChips();
		self:RefreshCard();
	end;

	frame:SetScript("OnShow", function(self)
		LayoutChips();
		self:Select(self.selected);
	end);

	SE.On("REFRESH", function()
		if ( frame:IsVisible() ) then
			frame:RefreshChips();
			frame:RefreshCard();
		end
	end);

	return frame;
end

do
	local columns = {};

	for index = 1, 3 do
		for _, column in ipairs(SE.EffectColumns(index)) do
			table.insert(columns, column);
		end
	end

	SE.AddSection({ key = "effects", title = "Effects", group = "spell", columns = columns, build = BuildEffects });
end

-- Procs, items, all fields ---------------------------------------------------------------------------

GridSection("procs", "Procs", "When an aura of this spell procs. The server side rules for the same spell are in the Proc rules tab under Server.",
	GroupsByTitle({ "Procs" }));
