-- Row set editor: the rows of one table and key in a list with a property grid for the selected row.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Common = GameEditorObject;

local ROW_HEIGHT = 20;

local function SameRow(columns, a, b)
	for _, column in ipairs(columns) do
		if ( not Common.Same(a[column.name], b[column.name]) ) then
			return false;
		end
	end

	return true;
end

local function Wrap(values, original)
	return { values = I.Copy(values), orig = original and I.Copy(values) or nil };
end

-- I.CreateRowEditor(parent, spec) builds the editor for spec.table. spec fields:
--   title, groups, columns, fields, readOnly, labelWidth, listWidth, single (one row, no list)
--   summary(values) list text, sort(a, b) on values, newValues(editor) for Add, unique = { columns } kept unique
--   whole (save replaces the table), canDelete (single mode may delete the row), onDirty(editor), onLoaded(editor)
function I.CreateRowEditor(parent, spec)
	local editor = CreateFrame("Frame", NoggitUI_NewName("GameEditorItemRows"), parent);
	editor.spec = spec;
	editor.tableName = spec.table;
	editor.wrappers = {};
	editor.columns = {};
	editor.removed = 0;
	editor.loaded = false;
	editor.loading = false;
	editor.saving = false;

	local top = CreateFrame("Frame", nil, editor);
	top:SetHeight(22);
	top:SetPoint("TOPLEFT", editor, "TOPLEFT", 0, 0);
	top:SetPoint("TOPRIGHT", editor, "TOPRIGHT", 0, 0);

	editor.title = Common.Label(top, spec.title or spec.table, "NoggitFont16", NOGGIT_COLOR_WHITE);
	editor.title:SetPoint("LEFT", top, "LEFT", 2, 0);

	editor.state = Common.Label(top, "", nil, NOGGIT_COLOR_DISABLED);
	editor.state:SetPoint("RIGHT", top, "RIGHT", -2, 0);
	editor.state:SetPoint("LEFT", editor.title, "RIGHT", 8, 0);
	editor.state:SetJustifyH("RIGHT");

	local bar = CreateFrame("Frame", nil, editor);
	bar:SetHeight(24);
	bar:SetPoint("BOTTOMLEFT", editor, "BOTTOMLEFT", 0, 0);
	bar:SetPoint("BOTTOMRIGHT", editor, "BOTTOMRIGHT", 0, 0);

	editor.saveButton = Common.Button(bar, "Save", 70, function() editor:Save(); end, spec.whole and "Replace the whole table on the server with these rows, the server reloads it" or "Write these rows to the server, the server reloads them");
	editor.saveButton:SetPoint("RIGHT", bar, "RIGHT", 0, 0);
	editor.revertButton = Common.Button(bar, "Revert", 64, function() editor:Revert(); end, "Throw away the unsaved changes of these rows");
	editor.revertButton:SetPoint("RIGHT", editor.saveButton, "LEFT", -4, 0);
	editor.reloadButton = Common.Button(bar, "Reload", 64, function() editor:ConfirmDiscard(function() editor:Reload(); end); end, "Load the rows again from the server");
	editor.reloadButton:SetPoint("RIGHT", editor.revertButton, "LEFT", -4, 0);

	if ( spec.single ) then
		if ( spec.canDelete ) then
			editor.deleteButton = Common.Button(bar, "Delete", 64, function() editor:DeleteRow(); end, "Delete this row, Save writes the delete");
			editor.deleteButton:SetPoint("LEFT", bar, "LEFT", 0, 0);
		end
	else
		editor.addButton = Common.Button(bar, "Add", 50, function() editor:AddRow(); end, "Add a new row");
		editor.addButton:SetPoint("LEFT", bar, "LEFT", 0, 0);
		editor.copyButton = Common.Button(bar, "Duplicate", 70, function() editor:DuplicateRow(); end, "Add a copy of the selected row");
		editor.copyButton:SetPoint("LEFT", editor.addButton, "RIGHT", 4, 0);
		editor.deleteButton = Common.Button(bar, "Delete", 56, function() editor:DeleteRow(); end, "Remove the selected row, Save writes it");
		editor.deleteButton:SetPoint("LEFT", editor.copyButton, "RIGHT", 4, 0);
	end

	editor.notice = Common.Paragraph(editor, "");
	editor.notice:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 4, -8);
	editor.notice:SetPoint("TOPRIGHT", top, "BOTTOMRIGHT", -4, -8);
	editor.notice:Hide();

	local gridLeft;

	if ( not spec.single ) then
		local filter = NoggitLineEdit_Create(editor, spec.listWidth or 220);
		filter:SetHeight(22);
		filter:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 0, -4);
		filter:SetWidth(spec.listWidth or 220);
		filter:SetAutoFocus(false);
		filter:SetScript("OnTextChanged", function() editor:RefreshList(); end);
		filter:SetScript("OnEscapePressed", function(self)
			self:SetText("");
			self:ClearFocus();
		end);
		filter:SetScript("OnEnterPressed", function(self) self:ClearFocus(); end);
		editor.filter = filter;

		local hint = Common.Label(filter, "Filter rows", nil, NOGGIT_COLOR_DISABLED);
		hint:SetPoint("LEFT", filter, "LEFT", 6, 0);
		filter:SetScript("OnEditFocusGained", function() hint:Hide(); end);
		filter:SetScript("OnEditFocusLost", function(self)
			if ( self:GetText() == "" ) then
				hint:Show();
			end
		end);

		editor.list = NoggitListBox_Create(editor, ROW_HEIGHT, function(row)
			row.mark = NoggitUI_FontString(row, "");
			row.mark:SetPoint("LEFT", row, "LEFT", 2, 0);
			row.mark:SetWidth(10);
			NoggitText_SetColor(row.mark, NOGGIT_COLOR_GOLD);
			row.text = NoggitUI_FontString(row, "");
			row.text:SetPoint("LEFT", row, "LEFT", 14, 0);
			row.text:SetPoint("RIGHT", row, "RIGHT", -2, 0);
		end, function(row, item)
			if ( not item ) then
				row.mark:SetText("");
				row.text:SetText("");
				return;
			end

			local wrapper = item.wrapper;
			local changed = not wrapper.orig or not SameRow(editor.columns, wrapper.values, wrapper.orig);
			row.mark:SetText(changed and "*" or "");
			row.text:SetText(editor:Summary(wrapper.values));
		end);
		editor.list:SetPoint("TOPLEFT", filter, "BOTTOMLEFT", 0, -4);
		editor.list:SetPoint("BOTTOMLEFT", bar, "TOPLEFT", 0, 4);
		editor.list:SetWidth(spec.listWidth or 220);
		editor.list.onSelect = function(item)
			editor:Select(item.wrapper);
		end;

		gridLeft = editor.list;
	end

	editor.grid = GameEditor_CreatePropertyGrid(editor, {
		table = spec.table,
		groups = spec.groups,
		columns = spec.columns,
		fields = spec.fields,
		readOnly = spec.readOnly,
		labelWidth = spec.labelWidth or 150,
		filter = spec.filterGrid,
		onChange = function(grid, column, value)
			if ( editor.current ) then
				editor.current.values[column] = value;
			end

			if ( spec.onChange ) then
				spec.onChange(editor, column, value);
			end

			editor:UpdateState();
		end,
	});

	if ( gridLeft ) then
		editor.grid.frame:SetPoint("TOPLEFT", gridLeft, "TOPRIGHT", 6, 26);
	else
		editor.grid.frame:SetPoint("TOPLEFT", top, "BOTTOMLEFT", 0, -4);
	end

	editor.grid.frame:SetPoint("BOTTOMRIGHT", bar, "TOPRIGHT", 0, 4);

	editor.Summary = function(self, values)
		if ( spec.summary ) then
			local ok, text = pcall(spec.summary, values);

			if ( ok and text ) then
				return text;
			end
		end

		local parts = {};

		for index, column in ipairs(self.columns) do
			if ( index > 4 ) then
				break;
			end

			table.insert(parts, tostring(values[column.name] or ""));
		end

		return table.concat(parts, "  ");
	end;

	editor.Items = function(self)
		local text = string.lower(strtrim(self.filter and self.filter:GetText() or ""));
		local items = {};

		for _, wrapper in ipairs(self.wrappers) do
			local summary = self:Summary(wrapper.values);

			if ( text == "" or string.find(string.lower(summary), text, 1, true) ) then
				table.insert(items, { wrapper = wrapper });
			end
		end

		return items;
	end;

	editor.RefreshList = function(self, keepOffset)
		if ( not self.list ) then
			return;
		end

		local items = self:Items();
		NoggitListBox_SetItems(self.list, items, keepOffset);
		self.list.selected = nil;

		for index, item in ipairs(items) do
			if ( item.wrapper == self.current ) then
				self.list.selected = index;
				break;
			end
		end

		NoggitListBox_Refresh(self.list);
	end;

	editor.ShowGrid = function(self, shown)
		if ( shown ) then
			self.grid.frame:Show();
		else
			self.grid.frame:Hide();
		end
	end;

	editor.Select = function(self, wrapper)
		self.current = wrapper;

		if ( wrapper ) then
			self.grid.original = I.Copy(wrapper.orig or {});
			self.grid:SetValues(wrapper.values, true);
			self:ShowGrid(true);
		else
			self:ShowGrid(false);
		end

		if ( self.list ) then
			self:RefreshList(true);
		end

		self:UpdateState();
	end;

	editor.ChangeCount = function(self)
		local count = self.removed;

		for _, wrapper in ipairs(self.wrappers) do
			if ( not wrapper.orig or not SameRow(self.columns, wrapper.values, wrapper.orig) ) then
				count = count + 1;
			end
		end

		return count;
	end;

	editor.HasChanges = function(self)
		return self.loaded and self:ChangeCount() > 0;
	end;

	editor.UpdateState = function(self)
		local count = self.loaded and self:ChangeCount() or 0;
		local text;

		if ( self.key == nil ) then
			text = "";
		elseif ( self.saving ) then
			text = "Saving...";
		elseif ( self.loading ) then
			text = "Loading...";
		elseif ( self.error ) then
			text = "|cffff6060"..self.error.."|r";
		elseif ( count > 0 ) then
			text = format("|cffffcc00%d unsaved change%s|r", count, count == 1 and "" or "s");
		elseif ( spec.single ) then
			text = #self.wrappers == 0 and "No row yet" or "Saved";
		else
			text = format("%d row%s", #self.wrappers, #self.wrappers == 1 and "" or "s");
		end

		self.state:SetText(text);

		local ready = self.loaded and not self.saving;
		Common.SetEnabled(self.saveButton, ready and count > 0);
		Common.SetEnabled(self.revertButton, ready and count > 0);
		Common.SetEnabled(self.reloadButton, self.key ~= nil and not self.saving);

		if ( self.addButton ) then
			Common.SetEnabled(self.addButton, ready);
			Common.SetEnabled(self.copyButton, ready and self.current ~= nil);
		end

		if ( self.deleteButton ) then
			Common.SetEnabled(self.deleteButton, ready and self.current ~= nil);
		end

		if ( self.list ) then
			NoggitListBox_Refresh(self.list);
		end

		if ( spec.onDirty ) then
			spec.onDirty(self);
		end
	end;

	editor.SetNotice = function(self, text)
		if ( text and text ~= "" ) then
			self.notice:SetText(text);
			self.notice:Show();
			self:ShowGrid(false);

			if ( self.list ) then
				self.list:Hide();
				self.filter:Hide();
			end
		else
			self.notice:Hide();

			if ( self.list ) then
				self.list:Show();
				self.filter:Show();
			end

			self:ShowGrid(self.current ~= nil);
		end
	end;

	editor.SetTitle = function(self, text)
		self.title:SetText(text or spec.title or spec.table);
	end;

	-- Points the editor at a key and loads its rows, seed is a row to start with when the key has none.
	editor.SetKey = function(self, key, seed)
		self.key = key;
		self.seed = seed;
		self.wrappers = {};
		self.removed = 0;
		self.current = nil;
		self.loaded = false;
		self.error = nil;
		self:Select(nil);

		if ( key ~= nil ) then
			self:Reload();
		else
			self:UpdateState();
		end
	end;

	editor.Reload = function(self)
		if ( self.key == nil ) then
			return;
		end

		self.loading = true;
		self.error = nil;

		if ( not I.Query(self.tableName, self.key) ) then
			self.loading = false;
			self.error = "could not ask the server";
		end

		self:UpdateState();
	end;

	editor.Load = function(self)
		local data = I.GetSet(self.tableName, self.key);
		self.loading = false;

		if ( not data ) then
			self.error = "the server sent no rows";
			self:UpdateState();
			return;
		end

		local previous = self.current and self.current.orig;
		self.columns = data.columns or {};
		self.keyColumn = data.keyColumn;
		self.wrappers = {};
		self.removed = 0;
		self.current = nil;
		self.loaded = true;
		self.error = nil;

		local rows = data.rows or {};

		if ( spec.sort ) then
			table.sort(rows, spec.sort);
		end

		for _, row in ipairs(rows) do
			table.insert(self.wrappers, Wrap(row, true));
		end

		if ( #self.wrappers == 0 and self.seed ) then
			local wrapper = Wrap(self:NewValues(self.seed), false);
			table.insert(self.wrappers, wrapper);
		end

		self.seed = nil;

		-- Keeps the same row selected across a reload when it is still there.
		local selected = self.wrappers[1];

		if ( previous and not spec.single ) then
			for _, wrapper in ipairs(self.wrappers) do
				if ( wrapper.orig and SameRow(self.columns, wrapper.orig, previous) ) then
					selected = wrapper;
					break;
				end
			end
		end

		self:Select(selected);

		if ( spec.onLoaded ) then
			spec.onLoaded(self);
		end
	end;

	-- A row with every column, 0 or empty, the key column and the given values on top.
	editor.NewValues = function(self, values)
		local row = {};

		for _, column in ipairs(self.columns) do
			row[column.name] = column.type == "string" and "" or 0;
		end

		for column, value in pairs(spec.defaults or {}) do
			row[column] = value;
		end

		if ( self.keyColumn and not spec.whole and self.key ~= nil ) then
			row[self.keyColumn] = self.key;
		end

		for column, value in pairs(values or {}) do
			row[column] = value;
		end

		return row;
	end;

	editor.AddRow = function(self, values)
		if ( not self.loaded ) then
			return;
		end

		local seed = values;

		if ( not seed and spec.newValues ) then
			seed = spec.newValues(self);
		end

		local wrapper = Wrap(self:NewValues(seed), false);
		table.insert(self.wrappers, wrapper);

		if ( self.filter ) then
			self.filter:SetText("");
		end

		self:Select(wrapper);

		if ( self.list ) then
			local items = self.list.items;

			for index, item in ipairs(items) do
				if ( item.wrapper == wrapper ) then
					self.list.offset = max(0, index - self.list.visibleCount + 1);
					break;
				end
			end

			NoggitListBox_Refresh(self.list);
		end

		return wrapper;
	end;

	editor.DuplicateRow = function(self)
		if ( self.current ) then
			local values = I.Copy(self.current.values);

			if ( spec.duplicate ) then
				spec.duplicate(self, values);
			end

			self:AddRow(values);
		end
	end;

	editor.DeleteRow = function(self)
		local wrapper = self.current;

		if ( not wrapper ) then
			return;
		end

		for index, other in ipairs(self.wrappers) do
			if ( other == wrapper ) then
				table.remove(self.wrappers, index);

				if ( wrapper.orig ) then
					self.removed = self.removed + 1;
				end

				local nextOne = self.wrappers[index] or self.wrappers[index - 1];
				self:Select(not spec.single and nextOne or nil);
				return;
			end
		end
	end;

	editor.Revert = function(self)
		local data = I.GetSet(self.tableName, self.key);

		if ( data ) then
			self:Load();
		else
			self:Reload();
		end
	end;

	editor.Validate = function(self)
		for _, columns in ipairs(spec.unique or {}) do
			local seen = {};

			for _, wrapper in ipairs(self.wrappers) do
				local parts = {};

				for _, column in ipairs(columns) do
					table.insert(parts, tostring(wrapper.values[column]));
				end

				local id = table.concat(parts, ":");

				if ( seen[id] ) then
					return format("Two rows share %s %s", table.concat(columns, " and "), id);
				end

				seen[id] = true;
			end
		end

		if ( spec.validate ) then
			return spec.validate(self);
		end
	end;

	editor.Save = function(self)
		if ( not self:HasChanges() or self.saving ) then
			return false;
		end

		if ( #self.wrappers == 0 and (spec.whole or spec.keepRows) ) then
			GameEditor_Notify("The server does not take an empty list here, keep at least one row", "error");
			return false;
		end

		local problem = self:Validate();

		if ( problem ) then
			GameEditor_Notify(problem, "error");
			return false;
		end

		local rows = {};

		for _, wrapper in ipairs(self.wrappers) do
			table.insert(rows, wrapper.values);
		end

		if ( not I.Save(self.tableName, self.key, self.columns, rows) ) then
			return false;
		end

		self.saving = true;
		self:UpdateState();
		return true;
	end;

	editor.ConfirmDiscard = function(self, action)
		if ( not self:HasChanges() ) then
			action();
			return;
		end

		NoggitMessageBox_Show("Unsaved changes", format("%s has unsaved changes. Drop them?", spec.title or spec.table), {
			{ text = "Drop changes", onClick = action },
			{ text = "Cancel" },
		});
	end;

	I.On("ROWS", function(tableName, key, ok, message)
		if ( tableName ~= string.lower(editor.tableName) or editor.key == nil or key ~= editor.key ) then
			return;
		end

		if ( ok ) then
			editor:Load();
		else
			editor.loading = false;
			editor.error = (message and message ~= "") and message or "the server could not read the rows";
			editor:UpdateState();
		end
	end);

	I.On("RESULT", function(tableName, key, ok)
		if ( tableName ~= string.lower(editor.tableName) or editor.key == nil or key ~= editor.key ) then
			return;
		end

		editor.saving = false;

		if ( ok ) then
			if ( spec.onSaved ) then
				spec.onSaved(editor);
			end

			editor:Reload();
		else
			editor:UpdateState();
		end
	end);

	I.On("NAMES", function()
		if ( editor:IsVisible() and editor.current ) then
			editor.grid:Refresh();
		end
	end);

	editor:Select(nil);
	return editor;
end
