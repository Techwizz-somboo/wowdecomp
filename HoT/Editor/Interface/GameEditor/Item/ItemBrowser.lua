-- Item browser on the left of the main area, the generator view next to it and the new item window.

GameEditorItem = GameEditorItem or {};
local I = GameEditorItem;
local Common = GameEditorObject;

local LEFT_OFFSET = 56;
local BROWSER_WIDTH = 320;

local root, browser, generator, searchBox, list, status;
local results;
local truncated = false;
local searching = false;

I.view = "items";

-- Opening ----------------------------------------------------------------------------------------------------

function I.OpenItem(entry)
	entry = tonumber(entry);

	if ( not entry or entry <= 0 ) then
		return;
	end

	if ( entry == I.entry ) then
		I.Reload();
		return;
	end

	I.Editor.ConfirmDiscard(function()
		I.Open(entry);

		if ( GameEditor_GetWorkspace() ~= "item" ) then
			GameEditor_SetWorkspace("item", "select");
		elseif ( I.view ~= "items" ) then
			GameEditor_SelectTool("select");
		end
	end);

	I.RefreshBrowser(true);
end

-- Browser ----------------------------------------------------------------------------------------------------

local function Item(entry, tag)
	local meta = I.meta[entry];
	local name, _, quality, itemLevel = GetItemInfo(entry);

	return {
		value = entry,
		text = name or I.names[entry] or "",
		quality = quality or (meta and meta.quality) or 1,
		level = itemLevel or (meta and meta.itemLevel) or 0,
		tag = entry == I.entry and "open" or tag,
	};
end

local function Items()
	local text = strtrim(searchBox and searchBox:GetText() or "");
	local items = {};

	if ( text == "" or not results ) then
		for _, entry in ipairs(I.recent) do
			table.insert(items, Item(entry, "recent"));
		end

		return items;
	end

	for _, row in ipairs(results) do
		table.insert(items, Item(row.entry));
	end

	return items;
end

function I.RefreshBrowser(keepSelection)
	if ( not list ) then
		return;
	end

	local items = Items();
	NoggitListBox_SetItems(list, items, keepSelection);
	list.selected = nil;

	for index, item in ipairs(items) do
		if ( item.value == I.entry ) then
			list.selected = index;
			break;
		end
	end

	NoggitListBox_Refresh(list);

	local text = strtrim(searchBox:GetText() or "");

	if ( searching ) then
		status:SetText("Searching the server...");
	elseif ( text == "" or not results ) then
		status:SetText(#items > 0 and "Recently opened items" or "Type a name or an entry and press Enter");
	else
		status:SetText(format("%d found%s", #items, truncated and ", showing the first ones" or ""));
	end
end

local function RunSearch()
	local text = strtrim(searchBox:GetText() or "");

	if ( text == "" ) then
		results = nil;
		I.RefreshBrowser();
		return;
	end

	searching = I.Search(text, function(rows, isTruncated)
		searching = false;

		if ( not rows ) then
			I.RefreshBrowser(true);
			return;
		end

		results = rows;
		truncated = isTruncated and true or false;
		I.RefreshBrowser();
	end);

	if ( not searching ) then
		GameEditor_Notify("Item search needs TemplateSearch_Query in this client build", "error");
	end

	I.RefreshBrowser();
end

function I.FocusSearch()
	if ( I.view ~= "items" ) then
		GameEditor_SelectTool("select");
	end

	if ( searchBox ) then
		searchBox:SetFocus();
		searchBox:HighlightText();
	end
end

local function BuildBrowser(parent)
	local frame = CreateFrame("Frame", "GameEditorItemBrowser", parent, "NoggitBoxTemplate");
	NoggitBox_Set(frame, NOGGIT_COLOR_BASE, NOGGIT_COLOR_MID);
	frame:SetPoint("TOPLEFT", parent, "TOPLEFT", LEFT_OFFSET, -6);
	frame:SetPoint("BOTTOMLEFT", parent, "BOTTOMLEFT", LEFT_OFFSET, 6);
	frame:SetWidth(BROWSER_WIDTH);
	frame:EnableMouse(true);
	frame:EnableMouseWheel(true);

	local title = Common.Label(frame, "Items", "NoggitFont16", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", frame, "TOPLEFT", 10, -8);

	local new = Common.Button(frame, "New item", 80, function() I.ShowNewDialog(); end, "Create an item with a new entry (Ctrl+N)");
	new:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -8, -6);

	local searchButton = Common.Button(frame, "Search", 64, RunSearch, "Search the item names on the server, a number opens that entry");
	searchButton:SetPoint("TOPRIGHT", frame, "TOPRIGHT", -8, -34);

	searchBox = NoggitLineEdit_Create(frame, 200);
	searchBox:SetHeight(22);
	searchBox:SetPoint("TOPLEFT", frame, "TOPLEFT", 8, -34);
	searchBox:SetPoint("RIGHT", searchButton, "LEFT", -4, 0);
	searchBox:SetAutoFocus(false);

	local hint = Common.Label(searchBox, "Name or entry", nil, NOGGIT_COLOR_DISABLED);
	hint:SetPoint("LEFT", searchBox, "LEFT", 6, 0);

	searchBox:SetScript("OnEditFocusGained", function() hint:Hide(); end);
	searchBox:SetScript("OnEditFocusLost", function(self)
		if ( self:GetText() == "" ) then
			hint:Show();
		end
	end);
	searchBox:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		local number = tonumber(strtrim(self:GetText() or ""));

		if ( number ) then
			I.OpenItem(number);
		end

		RunSearch();
	end);
	searchBox:SetScript("OnEscapePressed", function(self)
		self:SetText("");
		self:ClearFocus();
	end);
	searchBox:SetScript("OnTextChanged", function(self)
		if ( strtrim(self:GetText() or "") == "" ) then
			results = nil;
			I.RefreshBrowser();
		end
	end);

	status = Common.Label(frame, "", nil, NOGGIT_COLOR_DISABLED);
	status:SetPoint("BOTTOMLEFT", frame, "BOTTOMLEFT", 10, 8);
	status:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", -10, 8);

	list = NoggitListBox_Create(frame, 20, function(row)
		row.entry = NoggitUI_FontString(row, "");
		row.entry:SetPoint("LEFT", row, "LEFT", 4, 0);
		row.entry:SetWidth(52);
		NoggitText_SetColor(row.entry, NOGGIT_COLOR_DISABLED);
		row.level = NoggitUI_FontString(row, "", nil, "RIGHT");
		row.level:SetPoint("RIGHT", row, "RIGHT", -4, 0);
		row.level:SetWidth(46);
		NoggitText_SetColor(row.level, NOGGIT_COLOR_DISABLED);
		row.name = NoggitUI_FontString(row, "");
		row.name:SetPoint("LEFT", row, "LEFT", 58, 0);
		row.name:SetPoint("RIGHT", row.level, "LEFT", -4, 0);
	end, function(row, item)
		row.entry:SetText(tostring(item.value));

		if ( item.text ~= "" ) then
			row.name:SetText((item.tag == "open" and "> " or "")..I.Colored(item.text, item.quality));
		else
			row.name:SetText("|cff7f7f7fname unknown|r");
		end

		if ( (tonumber(item.level) or 0) > 0 ) then
			row.level:SetText(format("[%d]", item.level));
		else
			row.level:SetText(item.tag == "recent" and "recent" or "");
		end
	end);
	list:SetPoint("TOPLEFT", searchBox, "BOTTOMLEFT", 0, -6);
	list:SetPoint("RIGHT", frame, "RIGHT", -8, 0);
	list:SetPoint("BOTTOM", status, "TOP", 0, 6);
	list.onSelect = function(item)
		I.OpenItem(item.value);
	end;

	I.RefreshBrowser();
	return frame;
end

-- Switches the main area between the item list and the generator tables.
function I.ShowView(view)
	I.view = view;

	if ( not root ) then
		return;
	end

	if ( view == "generator" ) then
		browser:Hide();

		if ( not generator ) then
			generator = I.BuildGenerator(root, LEFT_OFFSET);
		end

		generator:Show();
	else
		if ( generator ) then
			generator:Hide();
		end

		browser:Show();
		I.RefreshBrowser(true);
	end
end

function I.BuildMain(parent)
	root = CreateFrame("Frame", "GameEditorItemMain", parent);
	root:SetAllPoints(parent);

	browser = BuildBrowser(root);
	I.ShowView(I.view);
	return root;
end

I.On("OPENED", function() I.RefreshBrowser(true); end);
I.On("TEMPLATE", function() I.RefreshBrowser(true); end);
I.On("ITEM_INFO", function() I.RefreshBrowser(true); end);

-- New item window ---------------------------------------------------------------------------------------------

local newWindow;

local function SetNewStatus(text, color)
	newWindow.status:SetText(text or "");
	NoggitText_SetColor(newWindow.status, color or NOGGIT_COLOR_DISABLED);
end

local function Create(entry)
	local copy = newWindow.copy and I.Editor.Snapshot() or nil;
	local copiedFrom = I.entry;

	local function Run()
		GameEditor_HideWindow(newWindow);
		I.Open(entry, copy);

		if ( I.view ~= "items" ) then
			GameEditor_SelectTool("select");
		end

		GameEditor_Notify(copy and format("Item %d copied to %d, Save writes it", copiedFrom or 0, entry) or format("New item %d, Save writes it", entry), "info");
	end

	if ( copy ) then
		Run();
	else
		I.Editor.ConfirmDiscard(Run);
	end
end

local function CheckAndCreate()
	local entry = tonumber(strtrim(newWindow.idBox:GetText() or ""));

	if ( not entry or entry <= 0 or entry > 4294967295 ) then
		SetNewStatus("Type an item entry first", NOGGIT_COLOR_GOLD);
		return;
	end

	if ( newWindow.copy and not I.Editor.Snapshot() ) then
		SetNewStatus("No item is open to copy", NOGGIT_COLOR_GOLD);
		return;
	end

	SetNewStatus("Checking the entry on the server...");

	I.FindFreeEntry(entry, function(free)
		if ( free == entry ) then
			Create(entry);
		elseif ( free ) then
			newWindow.idBox:SetText(tostring(free));
			SetNewStatus(format("Item %d already exists, %d is free. Press Create to use it.", entry, free), NOGGIT_COLOR_GOLD);
		else
			SetNewStatus(format("Item %d already exists and no free entry was found close to it", entry), NOGGIT_COLOR_GOLD);
		end
	end);
end

local function FindFree()
	local start = tonumber(strtrim(newWindow.idBox:GetText() or "")) or (I.maxSeen + 1);
	SetNewStatus("Looking for a free entry...");

	I.FindFreeEntry(start, function(free)
		if ( free ) then
			newWindow.idBox:SetText(tostring(free));
			SetNewStatus(format("%d is free", free));
		else
			SetNewStatus("No free entry found close to it, type another one", NOGGIT_COLOR_GOLD);
		end
	end);
end

local function CreateNewWindow()
	newWindow = GameEditor_CreateWindow("GameEditorItemNewWindow", "New item", 380, 196);
	local content = newWindow.content;

	local label = Common.Label(content, "Item entry");
	label:SetPoint("TOPLEFT", content, "TOPLEFT", 2, -4);

	newWindow.idBox = NoggitLineEdit_Create(content, 120);
	newWindow.idBox:SetHeight(22);
	newWindow.idBox:SetPoint("TOPLEFT", content, "TOPLEFT", 80, 0);
	newWindow.idBox:SetAutoFocus(false);
	newWindow.idBox:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		CheckAndCreate();
	end);
	newWindow.idBox:SetScript("OnEscapePressed", function(self) self:ClearFocus(); end);

	local find = Common.Button(content, "Find free", 76, FindFree, "Walk up from the typed entry until the server has no item with it");
	find:SetPoint("LEFT", newWindow.idBox, "RIGHT", 6, 0);

	local group = {};
	newWindow.empty = NoggitCheckBox_Create(content, "Start from an empty item", true, function() newWindow.copy = false; end, group);
	newWindow.empty:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -32);
	newWindow.copyBox = NoggitCheckBox_Create(content, "Copy the open item", false, function() newWindow.copy = true; end, group);
	newWindow.copyBox:SetPoint("TOPLEFT", content, "TOPLEFT", 0, -56);

	local note = Common.Paragraph(content, "A copy takes every item_template value of the open item, not its translations.");
	note:SetPoint("TOPLEFT", content, "TOPLEFT", 2, -82);
	note:SetPoint("TOPRIGHT", content, "TOPRIGHT", -2, -82);

	newWindow.status = Common.Label(content, "", nil, NOGGIT_COLOR_DISABLED);
	newWindow.status:SetPoint("BOTTOMLEFT", content, "BOTTOMLEFT", 2, 30);
	newWindow.status:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", -2, 30);

	local cancel = Common.Button(content, "Cancel", 70, function() GameEditor_HideWindow(newWindow); end);
	cancel:SetPoint("BOTTOMRIGHT", content, "BOTTOMRIGHT", 0, 0);
	local create = Common.Button(content, "Create", 70, CheckAndCreate, "Open the new item in the editor, nothing is written until you save");
	create:SetPoint("RIGHT", cancel, "LEFT", -4, 0);
end

-- I.ShowNewDialog(copy) asks for the entry of a new item, copy preselects copying the open item.
function I.ShowNewDialog(copy)
	if ( not newWindow ) then
		CreateNewWindow();
	end

	local canCopy = I.Editor.Snapshot() ~= nil;
	newWindow.copy = copy and canCopy or false;
	Common.SetEnabled(newWindow.copyBox, canCopy);
	NoggitCheckBox_SetChecked(newWindow.empty, not newWindow.copy);
	NoggitCheckBox_SetChecked(newWindow.copyBox, newWindow.copy);
	newWindow:SetTitle(newWindow.copy and format("Copy item %d", I.entry) or "New item");

	newWindow.idBox:SetText(I.maxSeen > 0 and tostring(I.maxSeen + 1) or "");
	SetNewStatus(I.maxSeen > 0 and "One above the highest entry seen so far, Create checks it" or "Type an entry, Find free walks up to an unused one");
	GameEditor_ShowWindow(newWindow);
	newWindow.idBox:SetFocus();
	newWindow.idBox:HighlightText();
end
