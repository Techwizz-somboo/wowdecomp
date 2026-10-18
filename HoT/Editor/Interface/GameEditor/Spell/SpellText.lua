-- Resolves the $ variables of spell descriptions and draws a game style tooltip preview.

local SE = GameEditorSpell;

local MAX_DEPTH = 4;
local VALUE_COLOR = "|cffffffff";
local UNKNOWN_COLOR = "|cffff5050";

local function N(values, column)
	return tonumber(values[column]) or 0;
end

-- Effect value range at the player's level, the way the client computes it for tooltips.
function SE.EffectRange(values, index)
	local base = N(values, "EffectBasePoints"..index);
	local die = N(values, "EffectDieSides"..index);
	local perLevel = N(values, "EffectRealPointsPerLevel"..index);
	local bonus = 0;

	if ( perLevel ~= 0 ) then
		local level = UnitLevel("player") or 1;
		local maxLevel = N(values, "MaximumLevel");

		if ( maxLevel > 0 and level > maxLevel ) then
			level = maxLevel;
		elseif ( level < N(values, "BaseLevel") ) then
			level = N(values, "BaseLevel");
		end

		bonus = floor(perLevel * (level - N(values, "SpellLevel")));
	end

	local low, high = base + bonus, base + bonus;

	if ( die ~= 0 ) then
		low = base + bonus + min(1, die);
		high = base + bonus + max(1, die);
	end

	return low, high;
end

function SE.EffectRangeText(values, index, multiplier)
	local low, high = SE.EffectRange(values, index);
	multiplier = multiplier or 1;
	low, high = abs(low * multiplier), abs(high * multiplier);

	if ( low > high ) then
		low, high = high, low;
	end

	if ( low == high ) then
		return SE.Trim(low), low;
	end

	return SE.Trim(low).." to "..SE.Trim(high), low;
end

local function DurationMs(values)
	local ms = SE.DurationInfo(N(values, "DurationIndex"));
	return ms or 0;
end

local function Ticks(values, index)
	local amplitude = N(values, "EffectAmplitude"..index);
	local duration = DurationMs(values);

	if ( amplitude <= 0 or duration <= 0 ) then
		return 1;
	end

	return floor(duration / amplitude);
end

local function DurationText(values)
	local ms = DurationMs(values);

	if ( ms < 0 ) then
		return "until cancelled";
	end

	return SE.FormatTime(ms);
end

local STAT_VARIABLES = {
	ap = function() local base, plus, minus = UnitAttackPower("player"); return (base or 0) + (plus or 0) + (minus or 0); end,
	rap = function() local base, plus, minus = UnitRangedAttackPower("player"); return (base or 0) + (plus or 0) + (minus or 0); end,
	sp = function() return GetSpellBonusDamage and GetSpellBonusDamage(2) or 0; end,
	spfi = function() return GetSpellBonusDamage and GetSpellBonusDamage(3) or 0; end,
	spn = function() return GetSpellBonusDamage and GetSpellBonusDamage(4) or 0; end,
	spfr = function() return GetSpellBonusDamage and GetSpellBonusDamage(5) or 0; end,
	sps = function() return GetSpellBonusDamage and GetSpellBonusDamage(6) or 0; end,
	spa = function() return GetSpellBonusDamage and GetSpellBonusDamage(7) or 0; end,
	sph = function() return GetSpellBonusDamage and GetSpellBonusDamage(2) or 0; end,
	spd = function() return GetSpellBonusDamage and GetSpellBonusDamage(2) or 0; end,
	pl = function() return UnitLevel("player") or 1; end,
	mws = function() return (UnitAttackSpeed("player")) or 0; end,
};

-- Token letter -> function(values, index, context) returning text and an optional number.
local TOKENS = {};

TOKENS.s = function(values, index, context)
	return SE.EffectRangeText(values, index, context.multiplier);
end;

TOKENS.m = function(values, index, context)
	local low = SE.EffectRange(values, index);
	low = abs(low * (context.multiplier or 1));
	return SE.Trim(low), low;
end;

TOKENS.M = function(values, index, context)
	local _, high = SE.EffectRange(values, index);
	high = abs(high * (context.multiplier or 1));
	return SE.Trim(high), high;
end;

TOKENS.o = function(values, index, context)
	return SE.EffectRangeText(values, index, Ticks(values, index) * (context.multiplier or 1));
end;

TOKENS.t = function(values, index, context)
	local seconds = N(values, "EffectAmplitude"..index) / 1000 * (context.multiplier or 1);
	return SE.Trim(seconds), seconds;
end;

TOKENS.a = function(values, index)
	local radius = SE.RadiusInfo(N(values, "EffectRadiusIndex"..index)) or 0;
	return SE.Trim(radius), radius;
end;

TOKENS.d = function(values)
	local ms = DurationMs(values);
	return DurationText(values), ms / 1000;
end;

TOKENS.h = function(values) return tostring(N(values, "ProcChance")), N(values, "ProcChance"); end;
TOKENS.n = function(values) return tostring(N(values, "ProcCharges")), N(values, "ProcCharges"); end;
TOKENS.u = function(values) return tostring(N(values, "StackAmount")), N(values, "StackAmount"); end;
TOKENS.i = function(values) return tostring(N(values, "MaximumAffectedTargets")), N(values, "MaximumAffectedTargets"); end;
TOKENS.v = function(values) return tostring(N(values, "MaximumTargetLevel")), N(values, "MaximumTargetLevel"); end;
TOKENS.c = function(values) return tostring(N(values, "ManaCost")), N(values, "ManaCost"); end;
TOKENS.p = function(values) return tostring(N(values, "ManaPerSecond")), N(values, "ManaPerSecond"); end;
TOKENS.z = function() return "(hearthstone location)"; end;

TOKENS.x = function(values, index)
	local count = N(values, "EffectChainTarget"..index);
	return tostring(count), count;
end;

TOKENS.q = function(values, index)
	local value = N(values, "EffectMiscValue"..index);
	return tostring(value), value;
end;

TOKENS.e = function(values, index)
	local value = N(values, "EffectMultipleValue"..index);
	return SE.Trim(value), value;
end;

TOKENS.b = function(values, index)
	local value = N(values, "EffectPointsPerComboPoint"..index);
	return SE.Trim(value), value;
end;

TOKENS.f = function(values, index)
	local value = N(values, "EffectDamageMultiplier"..index);
	return SE.Trim(value), value;
end;

TOKENS.r = function(values)
	local info = SE.RangeInfo(N(values, "RangeIndex"));
	local range = info and info.max or 0;
	return SE.Trim(range), range;
end;

local INDEXED = { s = true, m = true, M = true, o = true, t = true, a = true, x = true, q = true, e = true, b = true, f = true };

local function TokenFunction(letter)
	return TOKENS[letter] or TOKENS[string.lower(letter)];
end

local linkedCache = {};

local function LinkedValues(id, context)
	if ( id == context.id ) then
		return context.values;
	end

	if ( linkedCache[id] == nil ) then
		linkedCache[id] = SE.GetRow(SE.TABLE, id) or false;
	end

	return linkedCache[id] or nil;
end

local function Variables(values)
	local id = N(values, "SpellDescriptionVariableID");

	if ( id <= 0 ) then
		return nil;
	end

	local row = SE.GetRow("SpellDescriptionVariables", id);

	if ( not row or not row.Formula ) then
		return nil;
	end

	local variables = {};

	for name, formula in string.gmatch(tostring(row.Formula), "%$([%w_]+)=([^\r\n]+)") do
		variables[string.lower(name)] = formula;
	end

	return variables;
end

local Resolve;

local function Evaluate(expression, context)
	local numeric = Resolve(expression, context.values, context.depth + 1, { numeric = true, id = context.id, variables = context.variables });
	numeric = string.gsub(numeric, "|c%x%x%x%x%x%x%x%x", "");
	numeric = string.gsub(numeric, "|r", "");

	if ( not string.find(numeric, "^[%d%.%+%-%*/%(%)%s]+$") ) then
		return nil;
	end

	local chunk = loadstring("return "..numeric);

	if ( not chunk ) then
		return nil;
	end

	local ok, result = pcall(chunk);

	if ( ok and type(result) == "number" and result == result ) then
		return result;
	end
end

local function Mark(text, context)
	if ( context.numeric or not context.highlight ) then
		return text;
	end

	return VALUE_COLOR..text.."|r";
end

local function Unknown(text, context)
	if ( context.numeric ) then
		return "0";
	end

	if ( not context.highlight ) then
		return text;
	end

	return UNKNOWN_COLOR..text.."|r";
end

-- Reads a [text] group starting at position, returns the inner text and the position after it.
local function Bracket(text, position)
	if ( string.sub(text, position, position) ~= "[" ) then
		return nil, position;
	end

	local depth = 0;

	for index = position, #text do
		local char = string.sub(text, index, index);

		if ( char == "[" ) then
			depth = depth + 1;
		elseif ( char == "]" ) then
			depth = depth - 1;

			if ( depth == 0 ) then
				return string.sub(text, position + 1, index - 1), index + 1;
			end
		end
	end

	return nil, position;
end

local function ConditionTrue(condition)
	local _, _, negate, kind, id = string.find(condition, "^(!?)(%a)(%d+)");
	id = tonumber(id);
	local result = false;

	if ( id and kind == "s" and IsSpellKnown ) then
		local ok, known = pcall(IsSpellKnown, id);
		result = ok and known and true or false;
	end

	if ( negate == "!" ) then
		result = not result;
	end

	return result;
end

-- Resolve(text, values, depth, context) replaces every $ token the editor understands.
Resolve = function(text, values, depth, context)
	text = tostring(text or "");

	if ( depth > MAX_DEPTH or not string.find(text, "$", 1, true) ) then
		return text;
	end

	context = context or {};
	context.values = values;
	context.depth = depth;
	context.id = context.id or N(values, "ID");

	if ( context.variables == nil ) then
		context.variables = Variables(values) or false;
	end

	local out = {};
	local position = 1;
	local length = #text;
	local lastNumber;

	while ( position <= length ) do
		local start = string.find(text, "$", position, true);

		if ( not start ) then
			table.insert(out, string.sub(text, position));
			break;
		end

		table.insert(out, string.sub(text, position, start - 1));
		position = start + 1;
		local char = string.sub(text, position, position);
		local handled = false;

		if ( char == "{" ) then
			local close = string.find(text, "}", position, true);

			if ( close ) then
				local value = Evaluate(string.sub(text, position + 1, close - 1), context);
				position = close + 1;
				local _, _, decimals = string.find(text, "^%.(%d)", position);

				if ( decimals ) then
					position = position + 2;
				end

				if ( value ) then
					local shown = decimals and format("%."..decimals.."f", value) or SE.Trim(floor(value * 100 + 0.5) / 100);
					table.insert(out, Mark(shown, context));
					lastNumber = value;
				else
					table.insert(out, Unknown("${...}", context));
				end

				handled = true;
			end
		elseif ( char == "<" ) then
			local close = string.find(text, ">", position, true);

			if ( close ) then
				local name = string.lower(string.sub(text, position + 1, close - 1));
				position = close + 1;
				local formula = context.variables and context.variables[name];

				if ( formula ) then
					table.insert(out, Resolve(formula, values, depth + 1, { numeric = context.numeric, highlight = context.highlight, id = context.id, variables = context.variables }));
				else
					table.insert(out, Unknown("$<"..name..">", context));
				end

				handled = true;
			end
		elseif ( char == "?" ) then
			local branches = {};
			local cursor = position + 1;
			local elseText;

			while ( true ) do
				local open = string.find(text, "[", cursor, true);

				if ( not open ) then
					break;
				end

				local condition = string.sub(text, cursor, open - 1);
				local inner, after = Bracket(text, open);

				if ( not inner ) then
					break;
				end

				table.insert(branches, { condition = condition, text = inner });
				cursor = after;

				if ( string.sub(text, cursor, cursor) == "?" ) then
					cursor = cursor + 1;
				else
					elseText, cursor = Bracket(text, cursor);
					break;
				end
			end

			if ( #branches > 0 ) then
				local chosen = elseText or "";

				for _, branch in ipairs(branches) do
					if ( ConditionTrue(branch.condition) ) then
						chosen = branch.text;
						break;
					end
				end

				table.insert(out, Resolve(chosen, values, depth + 1, { numeric = context.numeric, highlight = context.highlight, id = context.id, variables = context.variables }));
				position = cursor;
				handled = true;
			end
		elseif ( char == "l" or char == "L" or char == "g" or char == "G" ) then
			local _, finish, first, second = string.find(text, "^[lLgG]([^:;]*):([^;]*);", position);

			if ( finish ) then
				local chosen;

				if ( char == "l" or char == "L" ) then
					chosen = (lastNumber == 1) and first or second;
				else
					chosen = (UnitSex("player") == 3) and second or first;
				end

				table.insert(out, chosen);
				position = finish + 1;
				handled = true;
			end
		elseif ( char == "/" or char == "*" ) then
			local _, finish, amount = string.find(text, "^[/%*]([%d%.]+);", position);

			if ( finish ) then
				amount = tonumber(amount) or 1;
				context.multiplier = (char == "/") and (amount ~= 0 and 1 / amount or 1) or amount;
				position = finish + 1;
				char = string.sub(text, position, position);
			end
		end

		if ( not handled ) then
			local _, wordEnd, word = string.find(text, "^(%a+)", position);
			local stat = word and STAT_VARIABLES[string.lower(word)];

			if ( stat and not string.find(string.sub(text, wordEnd + 1, wordEnd + 1), "%d") ) then
				local value = stat() or 0;
				table.insert(out, Mark(SE.Trim(value), context));
				lastNumber = value;
				position = wordEnd + 1;
				handled = true;
			end
		end

		if ( not handled ) then
			local _, finish, linked, letter, index = string.find(text, "^(%d*)(%a)(%d?)", position);

			if ( finish ) then
				local source = values;
				linked = tonumber(linked);

				if ( linked ) then
					source = LinkedValues(linked, context);
				end

				local func = TokenFunction(letter);
				local lower = string.lower(letter);

				if ( INDEXED[lower] and index == "" ) then
					index = "1";
				elseif ( not INDEXED[lower] and index ~= "" ) then
					finish = finish - 1;
					index = "";
				end

				if ( func and source ) then
					local shown, number = func(source, tonumber(index) or 1, context);

					if ( context.numeric and number ) then
						shown = tostring(number);
					end

					table.insert(out, Mark(shown, context));
					lastNumber = number;
				else
					table.insert(out, Unknown("$"..string.sub(text, position, finish), context));
				end

				context.multiplier = nil;
				position = finish + 1;
			else
				table.insert(out, "$");
			end
		end
	end

	return table.concat(out);
end

-- The client's own parser gives exactly what the player sees, the Lua resolver is the fallback.
local function NativeResolve(text, values)
	local id = values and tonumber(values.ID) or SE.CurrentId();

	if ( not DbcEditor_ParseSpellText or not id or not values ) then
		return nil;
	end

	local row = SE.Copy(values);
	row.SpellDescription0 = text;
	local ok, result = pcall(DbcEditor_ParseSpellText, id, row);
	return ok and result or nil;
end

function SE.ResolveText(text, values, highlight)
	if ( type(text) ~= "string" or text == "" ) then
		return "";
	end

	local native = NativeResolve(text, values);

	if ( native ) then
		return native;
	end

	linkedCache = {};
	return Resolve(text, values or {}, 0, { highlight = highlight });
end

-- What the client prints for the description, aura picks the buff text instead.
function SE.NativeSpellText(values, aura)
	local id = values and tonumber(values.ID) or SE.CurrentId();

	if ( not DbcEditor_ParseSpellText or not id ) then
		return nil;
	end

	local ok, result = pcall(DbcEditor_ParseSpellText, id, values, aura and true or false);
	return ok and result or nil;
end

-- One line summaries used by the tooltip preview ---------------------------------------------------

function SE.CostText(values)
	local powerType = N(values, "PowerType");
	local cost = N(values, "ManaCost");
	local percent = N(values, "ManaCostPercentage");
	local name = SE.EnumText("PowerType", powerType);

	if ( powerType == 1 or powerType == 6 ) then
		cost = cost / 10;
	end

	if ( percent > 0 ) then
		return format("%d%% of base %s", percent, string.lower(name));
	end

	if ( cost > 0 ) then
		local text = format("%s %s", SE.Trim(cost), name);

		if ( N(values, "ManaPerSecond") > 0 ) then
			text = text..format(", plus %d per sec", N(values, "ManaPerSecond"));
		end

		return text;
	end

	if ( powerType == 5 and N(values, "RuneCostID") > 0 ) then
		return "Runes";
	end

	return nil;
end

function SE.CastText(values)
	if ( GameEditor_HasBit(N(values, "AttributesEx"), 2) or GameEditor_HasBit(N(values, "AttributesEx"), 6) ) then
		return "Channeled";
	end

	if ( GameEditor_HasBit(N(values, "Attributes"), 2) or GameEditor_HasBit(N(values, "Attributes"), 10) ) then
		return "Next melee";
	end

	local ms = SE.CastTimeInfo(N(values, "CastingTimeIndex"));

	if ( not ms or ms <= 0 ) then
		return "Instant";
	end

	return SE.Trim(ms / 1000).." sec cast";
end

function SE.RangeText(values)
	local info = SE.RangeInfo(N(values, "RangeIndex"));

	if ( not info or info.max <= 0 ) then
		return nil;
	end

	if ( string.find(SE.Lower(info.name), "melee", 1, true) ) then
		return "Melee Range";
	end

	if ( info.min > 0 ) then
		return format("%s-%s yd range", SE.Trim(info.min), SE.Trim(info.max));
	end

	return format("%s yd range", SE.Trim(info.max));
end

function SE.CooldownText(values)
	local ms = max(N(values, "RecoveryTime"), N(values, "CategoryRecoveryTime"));

	if ( ms <= 0 ) then
		return nil;
	end

	return SE.FormatTime(ms).." cooldown";
end

function SE.ReagentText(values)
	local parts = {};

	for index = 1, 8 do
		local item = N(values, "Reagent"..index);

		if ( item > 0 ) then
			local name = GetItemInfo(item) or ("item "..item);
			local count = N(values, "ReagentCount"..index);
			table.insert(parts, count > 1 and format("%s (%d)", name, count) or name);
		end
	end

	if ( #parts == 0 ) then
		return nil;
	end

	return "Reagents: "..table.concat(parts, ", ");
end

-- Game style tooltip preview -----------------------------------------------------------------------

local TOOLTIP_BACKDROP = {
	bgFile = "Interface\\Tooltips\\UI-Tooltip-Background",
	edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
	tile = true, tileSize = 16, edgeSize = 16,
	insets = { left = 4, right = 4, top = 4, bottom = 4 },
};

local function TooltipLine(frame, font)
	local left = frame:CreateFontString(nil, "ARTWORK", font or "GameTooltipText");
	left:SetJustifyH("LEFT");
	local right = frame:CreateFontString(nil, "ARTWORK", "GameTooltipText");
	right:SetJustifyH("RIGHT");
	return { left = left, right = right };
end

local function CreateLuaTooltipPreview(parent, width)
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellTooltip"), parent);
	frame:SetWidth(width or 280);
	frame:SetHeight(60);
	frame:SetBackdrop(TOOLTIP_BACKDROP);
	frame:SetBackdropColor(0.03, 0.03, 0.08, 0.94);
	frame:SetBackdropBorderColor(0.6, 0.6, 0.6, 1);

	frame.header = TooltipLine(frame, "GameTooltipHeaderText");
	frame.lines = { TooltipLine(frame), TooltipLine(frame), TooltipLine(frame) };
	frame.description = frame:CreateFontString(nil, "ARTWORK", "GameTooltipText");
	frame.description:SetJustifyH("LEFT");
	frame.description:SetTextColor(1, 0.82, 0);
	frame.auraLabel = frame:CreateFontString(nil, "ARTWORK", "GameTooltipTextSmall");
	frame.auraLabel:SetJustifyH("LEFT");
	frame.auraLabel:SetTextColor(0.5, 0.5, 0.5);
	frame.auraLabel:SetText("Aura tooltip");
	frame.aura = frame:CreateFontString(nil, "ARTWORK", "GameTooltipText");
	frame.aura:SetJustifyH("LEFT");

	frame.Update = function(self, values)
		local inner = self:GetWidth() - 20;
		local y = -10;

		local function Place(line, leftText, rightText, r, g, b)
			if ( not leftText and not rightText ) then
				line.left:Hide();
				line.right:Hide();
				return;
			end

			line.left:ClearAllPoints();
			line.left:SetPoint("TOPLEFT", self, "TOPLEFT", 10, y);
			line.left:SetText(leftText or "");
			line.left:SetTextColor(r or 1, g or 1, b or 1);
			line.left:Show();
			line.right:ClearAllPoints();
			line.right:SetPoint("TOPRIGHT", self, "TOPRIGHT", -10, y);
			line.right:SetText(rightText or "");
			line.right:SetTextColor(0.5, 0.5, 0.5);
			line.right:Show();
			y = y - max(line.left:GetHeight(), 12) - 2;
		end

		if ( not values ) then
			Place(self.header, "No spell open", nil, 0.5, 0.5, 0.5);

			for _, line in ipairs(self.lines) do
				Place(line);
			end

			self.description:Hide();
			self.auraLabel:Hide();
			self.aura:Hide();
			self:SetHeight(-y + 8);
			return;
		end

		local name = values.SpellName0 ~= nil and tostring(values.SpellName0) or "";
		Place(self.header, name ~= "" and name or "(no name)", values.SpellRank0 ~= "" and values.SpellRank0 or nil);
		self.header.right:SetTextColor(0.5, 0.5, 0.5);
		Place(self.lines[1], SE.CostText(values), SE.RangeText(values));
		self.lines[1].right:SetTextColor(1, 1, 1);
		Place(self.lines[2], SE.CastText(values), SE.CooldownText(values));
		self.lines[2].right:SetTextColor(1, 1, 1);
		Place(self.lines[3], SE.ReagentText(values));

		local description = SE.ResolveText(values.SpellDescription0 or "", values, true);
		self.description:ClearAllPoints();
		self.description:SetPoint("TOPLEFT", self, "TOPLEFT", 10, y);
		self.description:SetWidth(inner);

		if ( description ~= "" ) then
			self.description:SetText(description);
			self.description:Show();
			y = y - self.description:GetHeight() - 6;
		else
			self.description:Hide();
		end

		local aura = SE.ResolveText(values.SpellToolTip0 or "", values, true);

		if ( aura ~= "" ) then
			self.auraLabel:ClearAllPoints();
			self.auraLabel:SetPoint("TOPLEFT", self, "TOPLEFT", 10, y - 2);
			self.auraLabel:Show();
			y = y - 16;
			self.aura:ClearAllPoints();
			self.aura:SetPoint("TOPLEFT", self, "TOPLEFT", 10, y);
			self.aura:SetWidth(inner);
			self.aura:SetText(aura);
			self.aura:Show();
			y = y - self.aura:GetHeight() - 4;
		else
			self.auraLabel:Hide();
			self.aura:Hide();
		end

		self:SetHeight(-y + 8);
	end;

	return frame;
end

-- Built by the client exactly like hovering the spell, unsaved values are patched in while it builds.
function SE.CreateTooltipPreview(parent, width)
	local frame = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellTooltipHolder"), parent);
	frame:SetWidth(width or 280);
	frame:SetHeight(60);

	frame.game = CreateFrame("GameTooltip", NoggitUI_NewName("GameEditorSpellGameTooltip"), frame, "GameTooltipTemplate");
	frame.fallback = CreateLuaTooltipPreview(frame, width);
	frame.fallback:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0);
	frame.fallback:Hide();

	frame.Update = function(self, values)
		local id = values and (tonumber(values.ID) or SE.CurrentId());
		local tip = self.game;
		local patched = id and DbcEditor_PatchSpellTooltip and DbcEditor_PatchSpellTooltip(id, values);

		if ( not patched ) then
			tip:Hide();
			self.fallback:SetWidth(self:GetWidth());
			self.fallback:Update(values);
			self.fallback:Show();
			self:SetHeight(self.fallback:GetHeight());
			return;
		end

		self.fallback:Hide();
		tip:SetOwner(self, "ANCHOR_NONE");
		tip:ClearAllPoints();
		tip:SetPoint("TOPLEFT", self, "TOPLEFT", 0, 0);
		pcall(tip.SetHyperlink, tip, "spell:"..id);
		DbcEditor_UnpatchSpellTooltip();

		local aura = values.SpellToolTip0 and values.SpellToolTip0 ~= "" and SE.NativeSpellText(values, true);

		if ( aura and aura ~= "" ) then
			tip:AddLine(" ");
			tip:AddLine("Aura tooltip", 0.5, 0.5, 0.5);
			tip:AddLine(aura, 1, 1, 1, 1);
		end

		tip:Show();
		self:SetHeight(max(20, tip:GetHeight()));
	end;

	return frame;
end

SE.TOKEN_HELP = table.concat({
	"|cffffcc00Description variables|r",
	"$s1 $s2 $s3  effect value (range when Die Sides > 1)",
	"$m1 / $M1  minimum / maximum value",
	"$o1  total over the duration (value times ticks)",
	"$t1  tick period in seconds   $a1  radius in yards",
	"$d  duration   $r  range   $h  proc chance   $n  proc charges",
	"$x1  chain targets   $q1  misc value   $e1  value multiplier",
	"$b1  points per combo point   $f1  chain damage multiplier",
	"$u  max stacks   $i  max targets   $v  max target level",
	"$c  power cost   $p  power per second   $z  hearthstone location",
	"$12345s1  value from another spell, works for every letter",
	"${$m1*2}  math, ${...}.1 keeps one decimal",
	"$/10;s1  divide the next value, $*3;s1 multiplies it",
	"$lpoint:points;  singular or plural from the last number",
	"$gmale:female;  text by the player's gender",
	"$?s12345[known][not known]  depends on a known spell",
	"$<name>  variable from Description Variables",
	"|cffff5050Red|r tokens could not be resolved.",
}, "\n");
