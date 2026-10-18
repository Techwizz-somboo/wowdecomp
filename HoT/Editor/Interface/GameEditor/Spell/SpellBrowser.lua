-- Spell browser column: search by name or id, recent spells, new and copy.

local SE = GameEditorSpell;

local ROW_HEIGHT = 32;
local SEARCH_DELAY = 0.25;
local SEARCH_LIMIT = 300;

local browser;
local contextItem;

local function ItemFor(id, name)
	return { id = id, name = name };
end

local function SelectTab(index)
	for _, tab in ipairs(browser.tabs) do
		tab.selected = tab.index == index;
		NoggitTab_Refresh(tab);
	end
end

local function ShowRecent()
	local items = {};

	for _, id in ipairs(SE.recent) do
		table.insert(items, ItemFor(id));
	end

	browser.mode = "recent";
	browser.items = items;
	NoggitListBox_SetItems(browser.list, items);
	browser.countText:SetText(#items > 0 and format("%d recent", #items) or "No recent spells yet");
	SelectTab(2);
	SE.SyncBrowserSelection();
end

local function RunSearch(fromTab)
	local text = strtrim(browser.search:GetText() or "");

	if ( text == "" ) then
		if ( fromTab ) then
			browser.mode = "search";
			browser.items = {};
			NoggitListBox_SetItems(browser.list, {});
			browser.countText:SetText("Type a name or an id to search");
			browser.search:SetFocus();
		else
			ShowRecent();
		end

		return;
	end

	local items = {};
	local seen = {};
	local number = tonumber(text);

	if ( number and SE.RowExists(SE.TABLE, number) ) then
		table.insert(items, ItemFor(number));
		seen[number] = true;
	end

	for _, row in ipairs(SE.Find(SE.TABLE, text, SEARCH_LIMIT)) do
		if ( not seen[row.id] ) then
			seen[row.id] = true;
			table.insert(items, ItemFor(row.id, row.name));
		end
	end

	browser.mode = "search";
	browser.items = items;
	NoggitListBox_SetItems(browser.list, items);

	if ( #items == 0 ) then
		browser.countText:SetText("No spell matches");
	elseif ( #items >= SEARCH_LIMIT ) then
		browser.countText:SetText(format("First %d matches", #items));
	else
		browser.countText:SetText(format("%d match%s", #items, #items == 1 and "" or "es"));
	end

	SelectTab(1);
	SE.SyncBrowserSelection();
end

function SE.SyncBrowserSelection()
	if ( not browser ) then
		return;
	end

	local id = SE.CurrentId();
	browser.list.selected = nil;

	for index, item in ipairs(browser.list.items) do
		if ( item.id == id ) then
			browser.list.selected = index;
			break;
		end
	end

	NoggitListBox_Refresh(browser.list);
end

function SE.FocusSearch()
	if ( browser ) then
		browser.search:SetFocus();
		browser.search:HighlightText();
	end
end

local function RefreshRow(row, item, selected)
	local doc = SE.FindDoc(item.id);
	local name, rank, icon;

	if ( doc ) then
		name = doc.values.SpellName0;
		rank = doc.values.SpellRank0;
		icon = SE.IconPath(doc.values.SpellIconID);
	else
		local infoName, infoRank, infoIcon = GetSpellInfo(item.id);
		name = item.name or infoName;
		rank = infoRank;
		icon = infoIcon;

		if ( not name or not icon ) then
			local values = SE.GetRow(SE.TABLE, item.id);

			if ( values ) then
				name = name or values.SpellName0;
				rank = rank or values.SpellRank0;
				icon = icon or SE.IconPath(values.SpellIconID);
			end
		end
	end

	row.icon:SetTexture(icon or "Interface\\Icons\\INV_Misc_QuestionMark");
	row.name:SetText((name and name ~= "") and name or "|cff8f8f8f(no name)|r");
	row.detail:SetText(format("%d%s", item.id, (rank and rank ~= "") and ("  -  "..rank) or ""));

	if ( doc and SE.DocHasUnsaved(doc) ) then
		row.dirty:Show();
	else
		row.dirty:Hide();
	end

	-- Spells open in a tab are white, the active one is also selected.
	NoggitText_SetColor(row.name, (selected or doc) and NOGGIT_COLOR_WHITE or NOGGIT_COLOR_TEXT);
end

local function BuildRow(row)
	row.icon = row:CreateTexture(nil, "ARTWORK");
	row.icon:SetWidth(26);
	row.icon:SetHeight(26);
	row.icon:SetPoint("LEFT", row, "LEFT", 4, 0);
	row.icon:SetTexCoord(0.07, 0.93, 0.07, 0.93);
	row.name = SE.Label(row, "");
	row.name:SetPoint("TOPLEFT", row, "TOPLEFT", 36, -3);
	row.name:SetPoint("RIGHT", row, "RIGHT", -14, 0);
	row.detail = SE.Label(row, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	row.detail:SetPoint("BOTTOMLEFT", row, "BOTTOMLEFT", 36, 4);
	row.detail:SetPoint("RIGHT", row, "RIGHT", -14, 0);
	row.dirty = SE.Label(row, "*", "NoggitFontBold", NOGGIT_COLOR_GOLD);
	row.dirty:SetPoint("RIGHT", row, "RIGHT", -4, 0);
	row.dirty:Hide();
end

local CONTEXT_MENU = {
	{ text = "Open in a tab", onClick = function() if ( contextItem ) then SE.Open(contextItem.id); end end },
	{ text = "Close its tab", onClick = function()
		local doc = contextItem and SE.FindDoc(contextItem.id);

		if ( doc ) then
			SE.CloseDoc(doc);
		end
	end },
	{ text = "Copy to a new id", onClick = function()
		if ( contextItem ) then
			local values = SE.GetRow(SE.TABLE, contextItem.id);

			if ( values ) then
				SE.NewSpell(values);
			end
		end
	end },
	{ text = "Preview on target", onClick = function()
		if ( contextItem and SpellPreview_Cast ) then
			SpellPreview_Cast(contextItem.id, SE.PreviewTargetGuid("auto"));
		end
	end },
	{ separator = true },
	{ text = "Remove from recent", onClick = function() if ( contextItem ) then SE.RemoveRecent(contextItem.id); end end },
};

-- Opens a spell picker with search, used by Spell > Open by id.
function SE.PickSpell(title, onPick)
	GameEditor_PickValue(title or "Open spell", nil, SE.CurrentId(), function(value)
		onPick(tonumber(value));
	end, GameEditor_RefSearchers["spell"]);
end

function SE.CreateBrowser(parent)
	browser = CreateFrame("Frame", "GameEditorSpellBrowser", parent);

	local title = SE.Label(browser, "Spells", "NoggitFontBold", NOGGIT_COLOR_WHITE);
	title:SetPoint("TOPLEFT", browser, "TOPLEFT", 2, -6);

	browser.search = SE.SearchBox(browser, 200, "Search name or id  (Ctrl+F)", function()
		browser.pending = SEARCH_DELAY;
	end);
	browser.search:SetPoint("TOPLEFT", browser, "TOPLEFT", 0, -26);
	browser.search:SetPoint("TOPRIGHT", browser, "TOPRIGHT", 0, -26);
	browser.search:SetScript("OnEnterPressed", function(self)
		self:ClearFocus();
		browser.pending = nil;
		RunSearch();

		local first = browser.list.items[1];

		if ( first and (#browser.list.items == 1 or tonumber(self:GetText()) == first.id) ) then
			SE.Open(first.id);
		end
	end);

	local tabsHolder = CreateFrame("Frame", NoggitUI_NewName("GameEditorSpellBrowserTabs"), browser);
	tabsHolder:SetHeight(24);
	tabsHolder:SetPoint("TOPLEFT", browser.search, "BOTTOMLEFT", 0, -6);
	tabsHolder:SetPoint("TOPRIGHT", browser.search, "BOTTOMRIGHT", 0, -6);
	browser.tabs = NoggitTabs_Create(tabsHolder, { "Results", "Recent" }, 2, function(index)
		if ( index == 2 ) then
			browser.search:SetQuietText("");
			browser.search.hint:Show();
			ShowRecent();
		else
			RunSearch(true);
		end
	end);
	browser.tabs[1]:SetPoint("LEFT", tabsHolder, "LEFT", 0, 0);

	browser.countText = SE.Label(browser, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	browser.countText:SetPoint("TOPLEFT", tabsHolder, "BOTTOMLEFT", 2, -4);

	browser.list = NoggitListBox_Create(browser, ROW_HEIGHT, BuildRow, RefreshRow);
	browser.list:SetPoint("TOPLEFT", tabsHolder, "BOTTOMLEFT", 0, -20);
	browser.list:SetPoint("BOTTOMRIGHT", browser, "BOTTOMRIGHT", 0, 60);
	browser.list.onSelect = function(item)
		if ( GetMouseButtonClicked and GetMouseButtonClicked() == "RightButton" ) then
			return;
		end

		SE.Open(item.id);
	end;
	browser.list.onContext = function(item)
		contextItem = item;
		SE.SyncBrowserSelection();
		NoggitContextMenu_Show("GameEditorSpellBrowserContext", CONTEXT_MENU);
	end;

	local newButton = SE.Button(browser, "New", 70, function() SE.NewSpell(); end, "New empty spell with the next free id (Ctrl+N)");
	newButton:SetPoint("BOTTOMLEFT", browser, "BOTTOMLEFT", 0, 32);
	local copyButton = SE.Button(browser, "Copy", 70, function() SE.CopySpell(); end, "Copy the open spell, unsaved edits included, to the next free id (Ctrl+D)");
	copyButton:SetPoint("LEFT", newButton, "RIGHT", 4, 0);
	local openButton = SE.Button(browser, "Open id", 70, function()
		SE.PickSpell("Open spell", function(id) SE.Open(id); end);
	end, "Open a spell by id or name (Ctrl+O)");
	openButton:SetPoint("LEFT", copyButton, "RIGHT", 4, 0);

	browser.idText = SE.Label(browser, "", "NoggitFont10", NOGGIT_COLOR_DISABLED);
	browser.idText:SetPoint("BOTTOMLEFT", browser, "BOTTOMLEFT", 2, 10);
	browser.idText:SetPoint("RIGHT", browser, "RIGHT", -2, 0);

	browser:SetScript("OnUpdate", function(self, elapsed)
		if ( self.pending ) then
			self.pending = self.pending - elapsed;

			if ( self.pending <= 0 ) then
				self.pending = nil;
				RunSearch();
			end
		end
	end);
	browser:SetScript("OnShow", function()
		browser.idText:SetText(format("Highest spell id %d", SE.MaxId(SE.TABLE)));
	end);

	SE.On("RECENT_CHANGED", function()
		if ( browser.mode == "recent" ) then
			ShowRecent();
		end
	end);
	SE.On("SPELL_LOADED", SE.SyncBrowserSelection);
	SE.On("REFRESH", function()
		NoggitListBox_Refresh(browser.list);
	end);

	ShowRecent();
	return browser;
end
