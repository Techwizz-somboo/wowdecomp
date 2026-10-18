-- Equipment page of the creature inspector, creature_equip_template sets of the entry.

GameEditorCreature = GameEditorCreature or {};
local Creature = GameEditorCreature;
local Common = GameEditorObject;

local ROW_HEIGHT = 46;
local SLOT_NAMES = { "Main hand", "Off hand", "Ranged" };

local function ItemText(id)
	id = tonumber(id) or 0;

	if ( id == 0 ) then
		return "empty";
	end

	return GameEditor_ResolveRef("item", id) or format("item %d", id);
end

local function CreateItemEdit(parent, slot)
	local edit = NoggitLineEdit_Create(parent, 84);
	edit:SetHeight(22);
	edit:SetNumeric(true);
	edit:SetMaxLetters(9);
	edit.name = Common.Label(parent, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	edit.name:SetPoint("TOPLEFT", edit, "BOTTOMLEFT", 2, -2);
	edit.name:SetWidth(84);
	edit.name:SetHeight(14);
	edit:SetScript("OnTextChanged", function(self)
		self.name:SetText(ItemText(self:GetText()));
	end);
	edit:SetScript("OnEnter", function(self)
		NoggitTooltip_Show(self, SLOT_NAMES[slot].."\n"..ItemText(self:GetText()));
	end);
	edit:SetScript("OnLeave", NoggitTooltip_Hide);
	return edit;
end

local function ItemValues(edits)
	return tonumber(edits[1]:GetText()) or 0, tonumber(edits[2]:GetText()) or 0, tonumber(edits[3]:GetText()) or 0;
end

function Creature.CreateEquipmentPage(parent)
	local page = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureEquipment"), parent);
	page.rows = {};

	local intro = Common.Paragraph(page, "Weapon sets of this entry (creature_equip_template). Item ids are main hand, off hand and ranged. Each set saves on its own.");
	intro:SetPoint("TOPLEFT", page, "TOPLEFT", 2, -2);
	intro:SetPoint("TOPRIGHT", page, "TOPRIGHT", -2, -2);
	intro:SetHeight(30);

	local columns = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureEquipmentColumns"), page);
	columns:SetHeight(16);
	columns:SetPoint("TOPLEFT", intro, "BOTTOMLEFT", 0, -4);
	columns:SetPoint("TOPRIGHT", intro, "BOTTOMRIGHT", 0, -4);

	for slot, name in ipairs(SLOT_NAMES) do
		local label = Common.Label(columns, name, "NoggitFontBold");
		label:SetPoint("LEFT", columns, "LEFT", 46 + (slot - 1) * 90, 0);
	end

	-- Bottom area: spawn set and the new set row.
	local bottom = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureEquipmentBottom"), page);
	bottom:SetHeight(120);
	bottom:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 0, 0);
	bottom:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", 0, 0);

	local newTitle = Common.Label(bottom, "New set", "NoggitFontBold");
	newTitle:SetPoint("TOPLEFT", bottom, "TOPLEFT", 2, 0);

	page.newId = NoggitSpinBox_Create(bottom, 40, 1, 255, 1, 0);
	page.newId:SetPoint("TOPLEFT", newTitle, "BOTTOMLEFT", -2, -4);
	Common.Tooltip(page.newId, "Set id, 1 to 255");

	page.newItems = {};

	for slot = 1, 3 do
		local edit = CreateItemEdit(bottom, slot);
		edit:SetPoint("LEFT", page.newId, "RIGHT", 6 + (slot - 1) * 90, 0);
		page.newItems[slot] = edit;
	end

	local create = Common.Button(bottom, "Add", 60, function()
		local id = page.newId.value;

		for _, set in ipairs(page.sets or {}) do
			if ( set.id == id ) then
				GameEditor_Notify(format("Set %d already exists, edit it above", id), "info");
				return;
			end
		end

		CreatureEditor_Equipment(0, id, ItemValues(page.newItems));

		for slot = 1, 3 do
			page.newItems[slot]:SetText("");
		end
	end, "Create the set with these items");
	create:SetPoint("LEFT", page.newItems[3], "RIGHT", 6, 0);
	page.create = create;

	local spawnTitle = Common.Label(bottom, "This spawn uses", "NoggitFontBold");
	spawnTitle:SetPoint("TOPLEFT", page.newId, "BOTTOMLEFT", 2, -24);

	page.spawnSet = NoggitSpinBox_Create(bottom, 60, -1, 255, 1, 0);
	page.spawnSet:SetPoint("LEFT", spawnTitle, "RIGHT", 8, 0);

	page.applySpawn = Common.Button(bottom, "Apply", 60, function()
		CreatureEditor_SetFields(1, { equipment_id = page.spawnSet.value });
	end, "Save creature.equipment_id of this spawn and respawn it");
	page.applySpawn:SetPoint("LEFT", page.spawnSet, "RIGHT", 6, 0);

	page.liveText = Common.Paragraph(bottom, "");
	page.liveText:SetPoint("TOPLEFT", bottom, "TOPLEFT", 2, -88);
	page.liveText:SetPoint("TOPRIGHT", bottom, "TOPRIGHT", -2, -88);

	local scroll, child = GameEditor_CreateScrollFrame(page);
	scroll:SetPoint("TOPLEFT", columns, "BOTTOMLEFT", 0, -4);
	scroll:SetPoint("BOTTOMRIGHT", bottom, "TOPRIGHT", 0, 8);
	page.scroll = scroll;
	page.child = child;

	page.empty = Common.Label(child, "This entry has no equipment sets yet.", nil, NOGGIT_COLOR_DISABLED);
	page.empty:SetPoint("TOPLEFT", child, "TOPLEFT", 4, -4);

	local function CreateRow(index)
		local row = CreateFrame("Frame", NoggitUI_NewName("GameEditorCreatureEquipmentRow"), child);
		row:SetHeight(ROW_HEIGHT);
		row:SetPoint("TOPLEFT", child, "TOPLEFT", 0, -(index - 1) * ROW_HEIGHT);
		row:SetPoint("TOPRIGHT", child, "TOPRIGHT", 0, -(index - 1) * ROW_HEIGHT);

		row.label = Common.Label(row, "", "NoggitFontBold");
		row.label:SetPoint("TOPLEFT", row, "TOPLEFT", 2, -4);
		row.label:SetWidth(40);

		row.items = {};

		for slot = 1, 3 do
			local edit = CreateItemEdit(row, slot);
			edit:SetPoint("TOPLEFT", row, "TOPLEFT", 44 + (slot - 1) * 90, 0);
			row.items[slot] = edit;
		end

		row.save = Common.Button(row, "Save", 46, function()
			CreatureEditor_Equipment(0, row.id, ItemValues(row.items));
		end, "Save this set");
		row.save:SetPoint("TOPLEFT", row, "TOPLEFT", 316, 0);

		row.delete = Common.Button(row, "Del", 40, function()
			local id = row.id;
			NoggitMessageBox_Show("Delete equipment set", format("Delete equipment set %d of this entry?", id), {
				{ text = "Delete", onClick = function() CreatureEditor_Equipment(1, id); end },
				{ text = "Cancel" },
			});
		end, "Delete this set");
		row.delete:SetPoint("LEFT", row.save, "RIGHT", 4, 0);

		return row;
	end

	page.Load = function(self, data)
		self.sets = data and data.equipment or {};
		local highest = 0;

		for index, set in ipairs(self.sets) do
			local row = self.rows[index] or CreateRow(index);
			self.rows[index] = row;
			row.id = set.id;
			row.label:SetText(format("Set %d", set.id));

			for slot = 1, 3 do
				local edit = row.items[slot];

				if ( not edit:HasFocus() ) then
					edit:SetText(tostring((set.items and set.items[slot]) or 0));
				end
			end

			row:Show();
			highest = max(highest, set.id);
		end

		for index = #self.sets + 1, #self.rows do
			self.rows[index]:Hide();
		end

		if ( #self.sets == 0 ) then
			self.empty:Show();
		else
			self.empty:Hide();
		end

		self.child:SetHeight(max(24, #self.sets * ROW_HEIGHT + 4));
		self.scroll.UpdateRange();
		NoggitSpinBox_SetValue(self.newId, min(255, highest + 1), true);

		local isSpawn = data and data.spawn ~= nil;
		Common.SetEnabled(self.applySpawn, isSpawn);
		self.spawnSet:SetEnabledState(isSpawn);

		if ( isSpawn ) then
			NoggitSpinBox_SetValue(self.spawnSet, tonumber(data.spawn.equipment_id) or 0, true);
		end

		self.liveText:SetText(format("The creature shows set %d right now. -1 picks a random set on spawn, 0 shows no weapons.%s", data and data.equipmentId or 0, isSpawn and "" or " This creature is not a database spawn."));
	end;

	return page;
end
