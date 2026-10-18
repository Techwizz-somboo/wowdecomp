-- Graphics profiler window, data comes from HoT/Profiler/Profiler.cpp through the GraphicsProfiler_* functions.

local FONT = "Interface\\GameEditor\\Fonts\\segoeui.ttf"
local FONT_BOLD = "Interface\\GameEditor\\Fonts\\segoeuisb.ttf"
local FALLBACK_FONT = "Fonts\\ARIALN.TTF"

local WIDTH = 820
local HEIGHT = 600
local ROW_HEIGHT = 15
local REFRESH = 0.25
local HISTORY = 240

local COLOR_GOOD = { 0.45, 0.85, 0.45 }
local COLOR_WARN = { 1.0, 0.8, 0.3 }
local COLOR_BAD = { 1.0, 0.4, 0.35 }
local COLOR_DIM = { 0.6, 0.6, 0.65 }
local COLOR_TEXT = { 0.92, 0.92, 0.95 }
local COLOR_HEAD = { 0.55, 0.75, 1.0 }
local COLOR_CPU = { 0.35, 0.65, 1.0 }
local COLOR_GPU = { 1.0, 0.55, 0.2 }

local summary = {}
local zones = {}
local counters = {}
local topModels = {}
local topShaders = {}
local topOpcodes = {}
local hitchRows = {}
local history = {}
local historyGpu = {}

local frame
local pages = {}
local tabs = {}
local currentPage = "overview"
local elapsed = 0
local paused = false
local hitchBack = 1

local function SetFontSafe(fs, bold, size)
    fs:SetFont(bold and FONT_BOLD or FONT, size or 11, "")

    if not fs:GetFont() then
        fs:SetFont(FALLBACK_FONT, (size or 11) + 1, "")
    end
end

local function NewText(parent, size, bold, justify)
    local fs = parent:CreateFontString(nil, "OVERLAY")
    SetFontSafe(fs, bold, size)
    fs:SetJustifyH(justify or "LEFT")
    fs:SetTextColor(COLOR_TEXT[1], COLOR_TEXT[2], COLOR_TEXT[3])
    return fs
end

local function SetColor(fs, color)
    fs:SetTextColor(color[1], color[2], color[3])
end

local function NewRect(parent, layer, r, g, b, a)
    local tex = parent:CreateTexture(nil, layer or "ARTWORK")
    tex:SetTexture(r, g, b, a or 1)
    return tex
end

local function Severity(value, warn, bad)
    if value >= bad then
        return COLOR_BAD
    elseif value >= warn then
        return COLOR_WARN
    end

    return COLOR_GOOD
end

local function Ms(value)
    if not value then
        return "-"
    end

    if value >= 100 then
        return string.format("%.0f", value)
    elseif value >= 10 then
        return string.format("%.1f", value)
    end

    return string.format("%.2f", value)
end

local function Num(value)
    if not value then
        return "-"
    end

    if value >= 1000000 then
        return string.format("%.2fM", value / 1000000)
    elseif value >= 10000 then
        return string.format("%.1fk", value / 1000)
    elseif value >= 100 or value == math.floor(value) then
        return string.format("%.0f", value)
    end

    return string.format("%.1f", value)
end

-- A table widget: fixed columns of font strings, rows made on demand, mouse wheel scrolls.

local function NewTable(parent, columns, top, bottom)
    local t = CreateFrame("Frame", nil, parent)
    t:SetPoint("TOPLEFT", parent, "TOPLEFT", 10, top)
    t:SetPoint("BOTTOMRIGHT", parent, "BOTTOMRIGHT", -10, bottom or 10)
    t:EnableMouseWheel(true)
    t.columns = columns
    t.rows = {}
    t.offset = 0
    t.data = {}
    t.count = 0

    local header = CreateFrame("Frame", nil, t)
    header:SetPoint("TOPLEFT", t, "TOPLEFT", 0, 0)
    header:SetPoint("TOPRIGHT", t, "TOPRIGHT", 0, 0)
    header:SetHeight(ROW_HEIGHT + 2)
    local x = 0

    for _, column in ipairs(columns) do
        local fs = NewText(header, 11, true, column.justify or "RIGHT")
        fs:SetPoint("TOPLEFT", header, "TOPLEFT", x + 2, 0)
        fs:SetWidth(column.width - 4)
        fs:SetHeight(ROW_HEIGHT)
        fs:SetText(column.title)
        SetColor(fs, COLOR_HEAD)
        x = x + column.width
    end

    local line = NewRect(header, "ARTWORK", 1, 1, 1, 0.15)
    line:SetPoint("BOTTOMLEFT", header, "BOTTOMLEFT", 0, 0)
    line:SetPoint("BOTTOMRIGHT", header, "BOTTOMRIGHT", 0, 0)
    line:SetHeight(1)

    t:SetScript("OnMouseWheel", function(self, delta)
        local visible = self:VisibleRows()
        local maxOffset = math.max(0, self.count - visible)
        self.offset = math.min(maxOffset, math.max(0, self.offset - delta * 3))
        self:Render()
    end)

    function t:VisibleRows()
        return math.max(1, math.floor((self:GetHeight() - ROW_HEIGHT - 4) / ROW_HEIGHT))
    end

    function t:Row(i)
        local row = self.rows[i]

        if row then
            return row
        end

        row = CreateFrame("Frame", nil, self)
        row:SetHeight(ROW_HEIGHT)
        row:SetPoint("TOPLEFT", self, "TOPLEFT", 0, -(ROW_HEIGHT + 4) - (i - 1) * ROW_HEIGHT)
        row:SetPoint("TOPRIGHT", self, "TOPRIGHT", 0, -(ROW_HEIGHT + 4) - (i - 1) * ROW_HEIGHT)
        row.bar = NewRect(row, "BACKGROUND", 0.3, 0.5, 0.9, 0.25)
        row.bar:SetPoint("TOPLEFT", row, "TOPLEFT", 0, -1)
        row.bar:SetPoint("BOTTOMLEFT", row, "BOTTOMLEFT", 0, 1)
        row.bar:SetWidth(1)
        row.cells = {}
        local cx = 0

        for c, column in ipairs(self.columns) do
            local fs = NewText(row, 11, false, column.justify or "RIGHT")
            fs:SetPoint("TOPLEFT", row, "TOPLEFT", cx + 2, 0)
            fs:SetWidth(column.width - 4)
            fs:SetHeight(ROW_HEIGHT)
            row.cells[c] = fs
            cx = cx + column.width
        end

        self.rows[i] = row
        return row
    end

    -- fill(row, item) sets the cells, data is an array of items.
    function t:SetData(data, count, fill)
        self.data = data
        self.count = count
        self.fill = fill
        self:Render()
    end

    function t:Render()
        local visible = self:VisibleRows()
        self.offset = math.min(self.offset, math.max(0, self.count - visible))

        for i = 1, visible do
            local item = self.data[self.offset + i]
            local row = self:Row(i)

            if item and self.offset + i <= self.count then
                row:Show()
                row.bar:Hide()

                for _, fs in ipairs(row.cells) do
                    SetColor(fs, COLOR_TEXT)
                end

                self.fill(row, item)
            else
                row:Hide()
            end
        end

        for i = visible + 1, #self.rows do
            self.rows[i]:Hide()
        end
    end

    return t
end

local function SetBar(row, fraction, color)
    fraction = math.max(0, math.min(1, fraction or 0))

    if fraction <= 0 then
        row.bar:Hide()
        return
    end

    local width = row:GetWidth()
    row.bar:SetWidth(math.max(1, width * fraction))
    row.bar:SetTexture(color[1], color[2], color[3], 0.22)
    row.bar:Show()
end

-- Frame time graph: one bar per frame, GPU time drawn as a thin bar on top.

local function NewGraph(parent, top, height)
    local g = CreateFrame("Frame", nil, parent)
    g:SetPoint("TOPLEFT", parent, "TOPLEFT", 10, top)
    g:SetPoint("TOPRIGHT", parent, "TOPRIGHT", -10, top)
    g:SetHeight(height)

    local bg = NewRect(g, "BACKGROUND", 0, 0, 0, 0.35)
    bg:SetAllPoints(g)

    g.bars = {}
    g.gpu = {}
    g.lines = {}

    for _, ms in ipairs({ 16.7, 33.3 }) do
        local line = NewRect(g, "BORDER", 1, 1, 1, 0.18)
        line:SetHeight(1)
        local label = NewText(g, 9, false, "LEFT")
        label:SetText(ms == 16.7 and "60 fps" or "30 fps")
        SetColor(label, COLOR_DIM)
        g.lines[#g.lines + 1] = { ms = ms, line = line, label = label }
    end

    g.scaleLabel = NewText(g, 9, false, "RIGHT")
    g.scaleLabel:SetPoint("TOPRIGHT", g, "TOPRIGHT", -3, -2)
    SetColor(g.scaleLabel, COLOR_DIM)

    function g:Update()
        local width = self:GetWidth()
        local h = self:GetHeight()
        local barWidth = width / HISTORY
        local peak = 33.3

        for i = 1, HISTORY do
            local v = history[i] or 0

            if v > peak then
                peak = v
            end
        end

        peak = math.min(peak * 1.1, 250)
        self.scaleLabel:SetText(string.format("%.0f ms", peak))

        for _, entry in ipairs(self.lines) do
            local y = entry.ms / peak * h
            entry.line:ClearAllPoints()
            entry.line:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", 0, y)
            entry.line:SetPoint("BOTTOMRIGHT", self, "BOTTOMRIGHT", 0, y)
            entry.label:ClearAllPoints()
            entry.label:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", 3, y + 1)
        end

        for i = 1, HISTORY do
            local bar = self.bars[i]

            if not bar then
                bar = NewRect(self, "ARTWORK", 1, 1, 1, 1)
                bar:SetWidth(math.max(1, barWidth - 0.5))
                self.bars[i] = bar
                local gpu = NewRect(self, "OVERLAY", COLOR_GPU[1], COLOR_GPU[2], COLOR_GPU[3], 0.9)
                gpu:SetWidth(math.max(1, barWidth - 0.5))
                gpu:SetHeight(2)
                self.gpu[i] = gpu
            end

            local v = history[i] or 0
            local barHeight = math.max(1, math.min(h, v / peak * h))
            bar:ClearAllPoints()
            bar:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", (i - 1) * barWidth, 0)
            bar:SetHeight(barHeight)
            local c = Severity(v, 20, 40)

            if v >= summary.hitchMs and summary.hitchMs > 0 then
                c = COLOR_BAD
            elseif c == COLOR_GOOD then
                c = COLOR_CPU
            end

            bar:SetTexture(c[1], c[2], c[3], 0.85)

            local gv = historyGpu[i] or 0
            local gpu = self.gpu[i]

            if gv > 0 then
                gpu:ClearAllPoints()
                gpu:SetPoint("BOTTOMLEFT", self, "BOTTOMLEFT", (i - 1) * barWidth, math.min(h - 2, gv / peak * h))
                gpu:Show()
            else
                gpu:Hide()
            end
        end
    end

    return g
end

-- Zone lookups.

-- Rows come back in zone id order, ids follow the Zone enum in Profiler.h.
local function ZoneById(id)
    return id and zones[id + 1]
end

local ZONE = {
    Frame = 0, Wait = 1, Poll = 4, AsyncFile = 5, Net = 8, Idle = 11, Paint = 16, UiUpdate = 17, UiBuild = 18, UiDraw = 19,
    WorldUpdate = 20, WorldText = 25, WorldRender = 26, SceneCull = 27, ObjectAnimate = 28, M2Animate = 29, LuaGc = 44,
    Present = 46, FpsCap = 49, Swap = 50, LuaHandlers = 51, TexUpload = 54, ModelCreate = 57, LoadWait = 58,
}

local function ZoneMs(key)
    local z = ZoneById(ZONE[key])
    return z and z.ms or 0
end

local function ZoneGpu(key)
    local z = ZoneById(ZONE[key])
    return z and z.gpu or 0
end

local function CounterAvg(name)
    for i = 1, #counters do
        if counters[i].name == name then
            return counters[i].avg, counters[i].peak
        end
    end

    return 0, 0
end

-- Overview page.

local function BoundBy()
    local cpu = summary.cpuMs or 0
    local gpu = summary.gpuMs or 0
    local frameMs = summary.frameMs or 0

    if (summary.capMs or 0) > frameMs * 0.25 or (summary.waitMs or 0) > frameMs * 0.35 then
        return "Frame limiter", COLOR_GOOD, "The frame waits on the FPS cap or idle sleep, there is headroom."
    end

    if summary.gpuTiming and summary.gpuFrames > 0 then
        if gpu > cpu * 1.15 or (summary.swapMs or 0) > frameMs * 0.3 then
            return "GPU", COLOR_BAD, "The GPU takes longer than the CPU, see the GPU tab for the passes that cost the most."
        end

        return "CPU", COLOR_WARN, "The CPU takes longer than the GPU, see the CPU tab for the slowest zones."
    end

    if (summary.swapMs or 0) > frameMs * 0.3 then
        return "GPU (from swap wait)", COLOR_BAD, "Present blocks for a long time, the GPU is behind."
    end

    return "CPU", COLOR_WARN, "See the CPU tab for the slowest zones."
end

local function BuildAdvice()
    local advice = {}
    local frameMs = summary.frameMs or 0
    local draws = CounterAvg("Draw calls")
    local texUploads, texUploadPeak = CounterAvg("Texture uploads")
    local uploadKB = CounterAvg("Texture upload KB")
    local m2Drawn = CounterAvg("M2 models drawn")
    local particles = CounterAvg("Particle emitters")
    local loads = CounterAvg("Blocking load waits")

    if draws > 3000 then
        advice[#advice + 1] = { COLOR_BAD, string.format("%.0f draw calls per frame, D3D9 submission gets expensive past 2000 to 3000. Check which zones issue them on the CPU tab.", draws) }
    elseif draws > 1500 then
        advice[#advice + 1] = { COLOR_WARN, string.format("%.0f draw calls per frame.", draws) }
    end

    if ZoneMs("UiUpdate") + ZoneMs("UiBuild") > frameMs * 0.25 and frameMs > 0 then
        advice[#advice + 1] = { COLOR_WARN, string.format("UI update and batch build take %.1f ms, addons or frame OnUpdate scripts are heavy.", ZoneMs("UiUpdate") + ZoneMs("UiBuild")) }
    end

    if ZoneMs("LuaHandlers") > 4 then
        advice[#advice + 1] = { COLOR_WARN, string.format("Lua script handlers run %.1f ms per frame.", ZoneMs("LuaHandlers")) }
    end

    if ZoneMs("ObjectAnimate") + ZoneMs("M2Animate") > frameMs * 0.25 and frameMs > 0 then
        advice[#advice + 1] = { COLOR_WARN, string.format("Model animation takes %.1f ms for %.0f animated models, crowds and particle heavy models add up.", ZoneMs("ObjectAnimate") + ZoneMs("M2Animate"), CounterAvg("M2 models animated")) }
    end

    if texUploadPeak > 20 or uploadKB > 2048 then
        advice[#advice + 1] = { COLOR_WARN, string.format("Texture uploads peak at %.0f in one frame (%.0f KB per frame on average), streaming textures causes hitches.", texUploadPeak, uploadKB) }
    end

    if loads > 0.05 then
        advice[#advice + 1] = { COLOR_BAD, "The main thread blocks waiting on file loads, see Blocking load wait on the CPU tab." }
    end

    if ZoneMs("ModelCreate") > 1 then
        advice[#advice + 1] = { COLOR_WARN, string.format("M2 creation opens files on the main thread, %.1f ms per frame.", ZoneMs("ModelCreate")) }
    end

    if ZoneMs("Net") > 3 then
        advice[#advice + 1] = { COLOR_WARN, string.format("Network handlers take %.1f ms, see the opcode list on the Resources tab.", ZoneMs("Net")) }
    end

    if particles > 400 then
        advice[#advice + 1] = { COLOR_WARN, string.format("%.0f particle emitters animate per frame.", particles) }
    end

    if m2Drawn > 1500 then
        advice[#advice + 1] = { COLOR_WARN, string.format("%.0f M2 models are drawn per frame.", m2Drawn) }
    end

    if summary.availableTexMB and summary.availableTexMB >= 0 and summary.availableTexMB < 256 then
        advice[#advice + 1] = { COLOR_BAD, string.format("Only %.0f MB of texture memory is left.", summary.availableTexMB) }
    end

    if #advice == 0 then
        advice[1] = { COLOR_GOOD, "Nothing stands out right now." }
    end

    return advice
end

local function CreateOverview(page)
    local stats = {}
    local labels = {
        { "fps", "FPS" }, { "frameMs", "Frame ms" }, { "low1Fps", "1% low fps" }, { "frameP99", "p99 ms" }, { "frameMax", "Worst ms" },
        { "cpuMs", "CPU busy ms" }, { "gpuMs", "GPU ms" }, { "draws", "Draw calls" }, { "hitches", "Hitches" },
    }

    for i, entry in ipairs(labels) do
        local x = 10 + ((i - 1) % 9) * 88
        local value = NewText(page, 18, true, "LEFT")
        value:SetPoint("TOPLEFT", page, "TOPLEFT", x, -8)
        value:SetWidth(86)
        local label = NewText(page, 10, false, "LEFT")
        label:SetPoint("TOPLEFT", value, "BOTTOMLEFT", 0, -1)
        label:SetText(entry[2])
        SetColor(label, COLOR_DIM)
        stats[entry[1]] = value
    end

    page.stats = stats

    page.bound = NewText(page, 12, true, "LEFT")
    page.bound:SetPoint("TOPLEFT", page, "TOPLEFT", 10, -50)
    page.boundHint = NewText(page, 11, false, "LEFT")
    page.boundHint:SetPoint("LEFT", page.bound, "RIGHT", 10, 0)
    SetColor(page.boundHint, COLOR_DIM)

    page.graph = NewGraph(page, -70, 110)

    page.legend = NewText(page, 10, false, "LEFT")
    page.legend:SetPoint("TOPLEFT", page.graph, "BOTTOMLEFT", 0, -2)
    page.legend:SetText("|cff59a6ffbars: frame time|r   |cffff8c33line: GPU time|r   |cffff6659red: hitch|r")

    page.breakdown = NewTable(page, {
        { title = "Where the frame goes", width = 260, justify = "LEFT" },
        { title = "ms", width = 60 },
        { title = "% frame", width = 70 },
        { title = "draws", width = 70 },
    }, -200, 130)
    page.breakdown:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", -340, 130)

    page.top = NewTable(page, {
        { title = "Hottest zones (self time)", width = 230, justify = "LEFT" },
        { title = "ms", width = 60 },
    }, -200, 130)
    page.top:ClearAllPoints()
    page.top:SetPoint("TOPLEFT", page, "TOPLEFT", 480, -200)
    page.top:SetPoint("BOTTOMRIGHT", page, "BOTTOMRIGHT", -10, 130)

    local adviceTitle = NewText(page, 11, true, "LEFT")
    adviceTitle:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 10, 112)
    adviceTitle:SetText("What to look at")
    SetColor(adviceTitle, COLOR_HEAD)

    page.advice = {}

    for i = 1, 6 do
        local fs = NewText(page, 11, false, "LEFT")
        fs:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 14, 112 - i * 16)
        fs:SetWidth(WIDTH - 40)
        page.advice[i] = fs
    end

    page.breakdownData = {}
    page.topData = {}
end

local BREAKDOWN = {
    { "Wait", "Sleep and frame limiter" },
    { "Poll", "Poll (net, sound, file loads)" },
    { "Idle", "Idle (movement, game UI, spells)" },
    { "UiUpdate", "UI update (OnUpdate, layout)" },
    { "UiBuild", "UI batch build" },
    { "UiDrawOnly", "UI draw (without world)" },
    { "WorldUpdate", "World update" },
    { "WorldRender", "World render" },
    { "LuaGc", "Lua GC" },
    { "Present", "Present (flush, cap, swap)" },
}

local function UpdateOverview(page)
    local s = page.stats
    s.fps:SetText(string.format("%.1f", summary.fps or 0))
    SetColor(s.fps, (summary.fps or 0) >= 55 and COLOR_GOOD or (summary.fps or 0) >= 30 and COLOR_WARN or COLOR_BAD)
    s.frameMs:SetText(Ms(summary.frameMs))
    s.low1Fps:SetText(string.format("%.0f", summary.low1Fps or 0))
    SetColor(s.low1Fps, (summary.low1Fps or 0) >= 45 and COLOR_GOOD or (summary.low1Fps or 0) >= 25 and COLOR_WARN or COLOR_BAD)
    s.frameP99:SetText(Ms(summary.frameP99))
    s.frameMax:SetText(Ms(summary.frameMax))
    SetColor(s.frameMax, Severity(summary.frameMax or 0, 33, summary.hitchMs > 0 and summary.hitchMs or 50))
    s.cpuMs:SetText(Ms(summary.cpuMs))
    s.gpuMs:SetText(summary.gpuTiming and Ms(summary.gpuMs) or "n/a")
    s.draws:SetText(Num(summary.draws))
    SetColor(s.draws, Severity(summary.draws or 0, 1500, 3000))
    s.hitches:SetText(string.format("%d", summary.hitches or 0))
    SetColor(s.hitches, (summary.hitches or 0) > 0 and COLOR_BAD or COLOR_GOOD)

    local bound, color, hint = BoundBy()
    page.bound:SetText("Bound by: " .. bound)
    SetColor(page.bound, color)
    page.boundHint:SetText(hint)

    page.graph:Update()

    local frameMs = summary.frameMs or 0
    local data = page.breakdownData

    for i, entry in ipairs(BREAKDOWN) do
        local item = data[i] or {}
        data[i] = item
        item.name = entry[2]

        if entry[1] == "UiDrawOnly" then
            local z = ZoneById(ZONE.UiDraw)
            local wu = ZoneById(ZONE.WorldUpdate)
            local wr = ZoneById(ZONE.WorldRender)
            local wt = ZoneById(ZONE.WorldText)
            item.ms = math.max(0, (z and z.ms or 0) - (wu and wu.ms or 0) - (wr and wr.ms or 0) - (wt and wt.ms or 0))
            item.draws = math.max(0, (z and z.draws or 0))
        else
            local z = ZoneById(ZONE[entry[1]])
            item.ms = z and z.ms or 0
            item.draws = z and z.draws or 0

            if z and (entry[1] == "WorldRender" or entry[1] == "Present") then
                item.draws = 0

                for j = 1, #zones do
                    local other = zones[j]
                    local parent = other.parent
                    local guard = 0

                    while parent and parent ~= 0 and guard < 16 do
                        if parent == z.id then
                            item.draws = item.draws + other.draws
                            break
                        end

                        local p = ZoneById(parent)
                        parent = p and p.parent
                        guard = guard + 1
                    end
                end

                item.draws = item.draws + z.draws
            end
        end

        item.fraction = frameMs > 0 and item.ms / frameMs or 0
    end

    page.breakdown:SetData(data, #BREAKDOWN, function(row, item)
        row.cells[1]:SetText(item.name)
        row.cells[2]:SetText(Ms(item.ms))
        row.cells[3]:SetText(string.format("%.0f%%", item.fraction * 100))
        row.cells[4]:SetText(item.draws > 0 and Num(item.draws) or "")
        SetBar(row, item.fraction, COLOR_CPU)
    end)

    local top = page.topData
    local n = 0

    for i = 1, #zones do
        local z = zones[i]

        if z.id ~= ZONE.Frame and z.id ~= ZONE.Wait and z.id ~= ZONE.FpsCap and z.self > 0.02 then
            n = n + 1
            top[n] = top[n] or {}
            top[n].name = z.name
            top[n].ms = z.self
        end
    end

    for i = n + 1, #top do
        top[i] = nil
    end

    table.sort(top, function(a, b) return a.ms > b.ms end)

    page.top:SetData(top, math.min(n, 20), function(row, item)
        row.cells[1]:SetText(item.name)
        row.cells[2]:SetText(Ms(item.ms))
        SetBar(row, frameMs > 0 and item.ms / frameMs or 0, COLOR_WARN)
    end)

    local advice = BuildAdvice()

    for i, fs in ipairs(page.advice) do
        local entry = advice[i]

        if entry then
            fs:SetText("- " .. entry[2])
            SetColor(fs, entry[1])
            fs:Show()
        else
            fs:Hide()
        end
    end
end

-- CPU page: the zone tree, then the hot spots that run inside many zones.

local function CreateCpu(page)
    page.table = NewTable(page, {
        { title = "Zone", width = 300, justify = "LEFT" },
        { title = "ms", width = 62 },
        { title = "self", width = 62 },
        { title = "peak", width = 62 },
        { title = "% frame", width = 62 },
        { title = "calls", width = 62 },
        { title = "draws", width = 70 },
        { title = "prims", width = 90 },
    }, -8, 28)

    page.hint = NewText(page, 10, false, "LEFT")
    page.hint:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 10, 10)
    page.hint:SetText("Per frame averages. self excludes child zones. Hot spots (marked *) run inside other zones and are already part of their totals. Wheel scrolls.")
    SetColor(page.hint, COLOR_DIM)
    page.data = {}
end

local function UpdateCpu(page)
    local data = page.data
    local n = 0
    local frameMs = summary.frameMs or 0

    for pass = 1, 2 do
        for i = 1, #zones do
            local z = zones[i]

            if (pass == 1 and not z.hotspot) or (pass == 2 and z.hotspot) then
                n = n + 1
                data[n] = z
            end
        end
    end

    for i = n + 1, #data do
        data[i] = nil
    end

    page.table:SetData(data, n, function(row, z)
        local indent = string.rep("    ", z.depth)
        row.cells[1]:SetText(z.hotspot and ("* " .. z.name) or (indent .. z.name))
        row.cells[2]:SetText(Ms(z.ms))
        row.cells[3]:SetText(Ms(z.self))
        row.cells[4]:SetText(Ms(z.peak))
        row.cells[5]:SetText(frameMs > 0 and string.format("%.1f", z.ms / frameMs * 100) or "")
        row.cells[6]:SetText(Num(z.calls))
        row.cells[7]:SetText(z.draws > 0 and Num(z.draws) or "")
        row.cells[8]:SetText(z.prims > 0 and Num(z.prims) or "")

        if z.ms < 0.005 then
            for _, fs in ipairs(row.cells) do
                SetColor(fs, COLOR_DIM)
            end
        else
            SetColor(row.cells[3], Severity(z.self, 2, 5))
            SetColor(row.cells[4], Severity(z.peak, 8, 20))
        end

        if z.hotspot then
            SetColor(row.cells[1], COLOR_WARN)
        end

        SetBar(row, frameMs > 0 and z.ms / frameMs or 0, z.hotspot and COLOR_WARN or COLOR_CPU)
    end)
end

-- GPU page.

local function CreateGpu(page)
    page.header = NewText(page, 12, true, "LEFT")
    page.header:SetPoint("TOPLEFT", page, "TOPLEFT", 10, -8)
    page.table = NewTable(page, {
        { title = "Pass", width = 300, justify = "LEFT" },
        { title = "GPU ms", width = 70 },
        { title = "GPU peak", width = 70 },
        { title = "% GPU", width = 60 },
        { title = "CPU ms", width = 70 },
        { title = "draws", width = 70 },
        { title = "prims", width = 90 },
    }, -30, 28)
    page.hint = NewText(page, 10, false, "LEFT")
    page.hint:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 10, 10)
    page.hint:SetText("D3D9 timestamp queries around each pass, read back a few frames later. UI draw includes the world, render passes nest under World render.")
    SetColor(page.hint, COLOR_DIM)
    page.data = {}
end

local function UpdateGpu(page)
    if not summary.gpuTiming then
        page.header:SetText("GPU timing is off (needs D3D9 or D3D9Ex with timestamp queries, CVar profilerGpuTiming 1).")
        SetColor(page.header, COLOR_WARN)
        page.table:SetData(page.data, 0, function() end)
        return
    end

    page.header:SetText(string.format("GPU frame %.2f ms, CPU busy %.2f ms, %d frames timed, %d dropped", summary.gpuMs or 0, summary.cpuMs or 0, summary.gpuFrames or 0, summary.gpuDropped or 0))
    SetColor(page.header, COLOR_TEXT)

    local data = page.data
    local n = 0

    for i = 1, #zones do
        local z = zones[i]

        if z.gpuZone or z.id == ZONE.Frame then
            n = n + 1
            data[n] = z
        end
    end

    for i = n + 1, #data do
        data[i] = nil
    end

    local gpuFrame = summary.gpuMs or 0

    page.table:SetData(data, n, function(row, z)
        local depth = 0

        if z.id == ZONE.Frame then
            row.cells[1]:SetText("GPU frame")
        else
            depth = z.parent == ZONE.WorldRender and 2 or 1
            row.cells[1]:SetText(string.rep("    ", depth) .. z.name)
        end

        row.cells[2]:SetText(Ms(z.gpu))
        row.cells[3]:SetText(Ms(z.gpuPeak))
        row.cells[4]:SetText(gpuFrame > 0 and string.format("%.1f", z.gpu / gpuFrame * 100) or "")
        row.cells[5]:SetText(Ms(z.ms))
        row.cells[6]:SetText(z.draws > 0 and Num(z.draws) or "")
        row.cells[7]:SetText(z.prims > 0 and Num(z.prims) or "")
        SetColor(row.cells[2], Severity(z.gpu, 2, 5))
        SetBar(row, gpuFrame > 0 and z.gpu / gpuFrame or 0, COLOR_GPU)
    end)
end

-- Scene page, every counter grouped.

local function CreateScene(page)
    page.table = NewTable(page, {
        { title = "Group", width = 110, justify = "LEFT" },
        { title = "Counter (per frame)", width = 300, justify = "LEFT" },
        { title = "avg", width = 100 },
        { title = "peak", width = 100 },
    }, -8, 28)
    page.hint = NewText(page, 10, false, "LEFT")
    page.hint:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 10, 10)
    page.hint:SetText("Gx counters come from the device layer, scene counters from the world and M2 code. Peak is the highest single frame in the window.")
    SetColor(page.hint, COLOR_DIM)
end

local function UpdateScene(page)
    page.table:SetData(counters, #counters, function(row, c)
        row.cells[1]:SetText(c.group)
        SetColor(row.cells[1], COLOR_HEAD)
        row.cells[2]:SetText(c.name)
        row.cells[3]:SetText(Num(c.avg))
        row.cells[4]:SetText(Num(c.peak))

        if c.avg == 0 and c.peak == 0 then
            for _, fs in ipairs(row.cells) do
                SetColor(fs, COLOR_DIM)
            end
        end
    end)
end

-- Resources page: memory, then the model, shader and opcode top lists.

local function CreateResources(page)
    page.memory = {}

    for i = 1, 3 do
        local fs = NewText(page, 11, false, "LEFT")
        fs:SetPoint("TOPLEFT", page, "TOPLEFT", 10, -8 - (i - 1) * 16)
        fs:SetWidth(WIDTH - 40)
        page.memory[i] = fs
    end

    page.kind = "models"
    page.buttons = {}
    local kinds = { { "models", "M2 models" }, { "shaders", "Pixel shaders" }, { "opcodes", "Network opcodes" } }

    for i, entry in ipairs(kinds) do
        local b = CreateFrame("Button", nil, page)
        b:SetSize(120, 18)
        b:SetPoint("TOPLEFT", page, "TOPLEFT", 10 + (i - 1) * 126, -60)
        b.bg = NewRect(b, "BACKGROUND", 1, 1, 1, 0.08)
        b.bg:SetAllPoints(b)
        b.text = NewText(b, 11, true, "CENTER")
        b.text:SetAllPoints(b)
        b.text:SetText(entry[2])
        b:SetScript("OnClick", function()
            page.kind = entry[1]
            page.table.offset = 0
            GraphicsProfiler_Refresh(true)
        end)
        b.kind = entry[1]
        page.buttons[i] = b
    end

    page.table = NewTable(page, {
        { title = "Name", width = 430, justify = "LEFT" },
        { title = "a", width = 80 },
        { title = "b", width = 80 },
        { title = "c", width = 80 },
        { title = "d", width = 100 },
    }, -86, 28)
    page.hint = NewText(page, 10, false, "LEFT")
    page.hint:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 10, 10)
    SetColor(page.hint, COLOR_DIM)
end

local function SetHeaders(t, titles)
    local header = t:GetChildren()
    local regions = { header:GetRegions() }
    local n = 0

    for _, region in ipairs(regions) do
        if region:GetObjectType() == "FontString" then
            n = n + 1

            if titles[n] then
                region:SetText(titles[n])
            end
        end
    end
end

local function UpdateResources(page)
    local luaKB = collectgarbage("count")
    page.memory[1]:SetText(string.format("Textures %d using %.1f MB, render targets %d using %.1f MB, %s", summary.textures or 0, summary.textureMB or 0, summary.renderTargets or 0, summary.renderTargetMB or 0,
        (summary.availableTexMB or -1) >= 0 and string.format("driver reports %.0f MB of texture memory free", summary.availableTexMB) or "free texture memory unknown"))
    page.memory[2]:SetText(string.format("Process working set %.0f MB, private %.0f MB, Lua heap %.1f MB%s", summary.workingSetMB or 0, summary.privateMB or 0, luaKB / 1024,
        (summary.texCacheMB or -1) >= 0 and string.format(", D3D9Ex texture cache %.0f of %.0f MB", summary.texCacheMB, summary.texCacheLimitMB) or ""))
    page.memory[3]:SetText(string.format("Renderer %s", summary.api or "?"))
    SetColor(page.memory[3], COLOR_DIM)

    for _, b in ipairs(page.buttons) do
        if b.kind == page.kind then
            b.bg:SetTexture(0.3, 0.5, 0.9, 0.45)
        else
            b.bg:SetTexture(1, 1, 1, 0.08)
        end
    end

    if page.kind == "models" then
        local n = GraphicsProfiler_GetTop(topModels, "models")
        SetHeaders(page.table, { "M2 file", "CPU ms", "elements", "draws", "prims" })
        page.hint:SetText("CPU time spent submitting each M2 file's draw elements per frame (GPU cost is on the GPU tab by pass).")
        page.table:SetData(topModels, n, function(row, e)
            row.cells[1]:SetText(e.name)
            row.cells[2]:SetText(Ms(e.ms))
            row.cells[3]:SetText(Num(e.count))
            row.cells[4]:SetText(Num(e.draws))
            row.cells[5]:SetText(Num(e.prims))
            SetColor(row.cells[2], Severity(e.ms, 0.5, 2))
        end)
    elseif page.kind == "shaders" then
        local n = GraphicsProfiler_GetTop(topShaders, "shaders")
        SetHeaders(page.table, { "Pixel shader (vertex shader)", "", "", "draws", "prims" })
        page.hint:SetText("Draw calls per pixel shader per frame, a shader with many draws is a batching candidate.")
        page.table:SetData(topShaders, n, function(row, e)
            row.cells[1]:SetText(e.name)
            row.cells[2]:SetText("")
            row.cells[3]:SetText("")
            row.cells[4]:SetText(Num(e.draws))
            row.cells[5]:SetText(Num(e.prims))
        end)
    else
        local n = GraphicsProfiler_GetTop(topOpcodes, "opcodes")
        SetHeaders(page.table, { "Opcode", "ms / frame", "messages", "KB", "" })
        page.hint:SetText("Server message handlers by time, messages and KB are totals for the refresh window.")
        page.table:SetData(topOpcodes, n, function(row, e)
            row.cells[1]:SetText(e.name)
            row.cells[2]:SetText(Ms(e.ms))
            row.cells[3]:SetText(Num(e.count))
            row.cells[4]:SetText(string.format("%.1f", e.kb))
            row.cells[5]:SetText("")
            SetColor(row.cells[2], Severity(e.ms, 0.5, 2))
        end)
    end
end

-- Hitches page.

local function CreateHitches(page)
    page.header = NewText(page, 12, true, "LEFT")
    page.header:SetPoint("TOPLEFT", page, "TOPLEFT", 10, -8)
    page.sub = NewText(page, 11, false, "LEFT")
    page.sub:SetPoint("TOPLEFT", page.header, "BOTTOMLEFT", 0, -3)
    SetColor(page.sub, COLOR_DIM)

    local function Button(text, x, onClick)
        local b = CreateFrame("Button", nil, page)
        b:SetSize(70, 18)
        b:SetPoint("TOPRIGHT", page, "TOPRIGHT", x, -8)
        local bg = NewRect(b, "BACKGROUND", 1, 1, 1, 0.08)
        bg:SetAllPoints(b)
        local fs = NewText(b, 11, true, "CENTER")
        fs:SetAllPoints(b)
        fs:SetText(text)
        b:SetScript("OnClick", onClick)
        return b
    end

    Button("Older", -170, function() hitchBack = hitchBack + 1 GraphicsProfiler_Refresh(true) end)
    Button("Newer", -90, function() hitchBack = math.max(1, hitchBack - 1) GraphicsProfiler_Refresh(true) end)
    Button("Clear", -10, function() GraphicsProfiler_ClearHitches() hitchBack = 1 GraphicsProfiler_Refresh(true) end)

    page.table = NewTable(page, {
        { title = "Zone (sorted by self time)", width = 360, justify = "LEFT" },
        { title = "self ms", width = 80 },
        { title = "total ms", width = 80 },
        { title = "draws", width = 80 },
    }, -46, 28)
    page.hint = NewText(page, 10, false, "LEFT")
    page.hint:SetPoint("BOTTOMLEFT", page, "BOTTOMLEFT", 10, 10)
    page.hint:SetText("Frames over profilerHitchMs are captured whole. The last 8 are kept, Dump writes all of them to Logs.")
    SetColor(page.hint, COLOR_DIM)
end

local function UpdateHitches(page)
    local n, frameMs, clock, total, uploads, creates, waits = GraphicsProfiler_GetHitch(hitchRows, hitchBack)

    if not n or n == 0 then
        page.header:SetText(string.format("No hitches over %.0f ms captured yet.", summary.hitchMs or 0))
        SetColor(page.header, COLOR_GOOD)
        page.sub:SetText("")
        page.table:SetData(hitchRows, 0, function() end)
        return
    end

    hitchBack = math.min(hitchBack, total)
    page.header:SetText(string.format("Hitch %d of %d at %s: %.1f ms", hitchBack, total, clock, frameMs))
    SetColor(page.header, COLOR_BAD)
    page.sub:SetText(string.format("texture uploads %d, M2 creates %d, blocking load waits %d", uploads, creates, waits))

    page.table:SetData(hitchRows, n, function(row, z)
        row.cells[1]:SetText(z.hotspot and ("* " .. z.name) or z.name)
        row.cells[2]:SetText(Ms(z.self))
        row.cells[3]:SetText(Ms(z.ms))
        row.cells[4]:SetText(z.draws > 0 and Num(z.draws) or "")
        SetColor(row.cells[2], Severity(z.self, 3, 10))
        SetBar(row, frameMs > 0 and z.self / frameMs or 0, COLOR_BAD)
    end)
end

-- Window.

local PAGES = {
    { "overview", "Overview", CreateOverview, UpdateOverview },
    { "cpu", "CPU", CreateCpu, UpdateCpu },
    { "gpu", "GPU", CreateGpu, UpdateGpu },
    { "scene", "Scene", CreateScene, UpdateScene },
    { "resources", "Resources", CreateResources, UpdateResources },
    { "hitches", "Hitches", CreateHitches, UpdateHitches },
}

local function ShowPage(key)
    currentPage = key

    for _, entry in ipairs(PAGES) do
        local page = pages[entry[1]]

        if entry[1] == key then
            page:Show()
        else
            page:Hide()
        end
    end

    for _, tab in ipairs(tabs) do
        if tab.key == key then
            tab.bg:SetTexture(0.3, 0.5, 0.9, 0.5)
        else
            tab.bg:SetTexture(1, 1, 1, 0.06)
        end
    end

    GraphicsProfiler_Refresh(true)
end

function GraphicsProfiler_Refresh(force)
    if not frame or not frame:IsShown() then
        return
    end

    if paused and not force then
        return
    end

    GraphicsProfiler_GetSummary(summary)
    GraphicsProfiler_GetZones(zones)
    GraphicsProfiler_GetCounters(counters)
    GraphicsProfiler_GetHistory(history, false)
    GraphicsProfiler_GetHistory(historyGpu, true)

    frame.status:SetText(string.format("%s  |  %.2f s window, %d frames%s", summary.api or "?", summary.seconds or 0, summary.frames or 0, paused and "  |  PAUSED" or ""))

    for _, entry in ipairs(PAGES) do
        if entry[1] == currentPage then
            entry[4](pages[entry[1]])
        end
    end
end

local function HeaderButton(text, width, x, onClick)
    local b = CreateFrame("Button", nil, frame)
    b:SetSize(width, 18)
    b:SetPoint("TOPRIGHT", frame, "TOPRIGHT", x, -6)
    b.bg = NewRect(b, "BACKGROUND", 1, 1, 1, 0.08)
    b.bg:SetAllPoints(b)
    b.text = NewText(b, 11, true, "CENTER")
    b.text:SetAllPoints(b)
    b.text:SetText(text)
    b:SetScript("OnClick", onClick)
    b:SetScript("OnEnter", function(self) self.bg:SetTexture(1, 1, 1, 0.18) end)
    b:SetScript("OnLeave", function(self) self.bg:SetTexture(1, 1, 1, 0.08) end)
    return b
end

local function Print(text)
    if DEFAULT_CHAT_FRAME then
        DEFAULT_CHAT_FRAME:AddMessage("|cff59a6ffGraphics profiler:|r " .. text)
    end
end

local function CreateWindow()
    frame = CreateFrame("Frame", "GraphicsProfilerFrame", nil)
    frame:SetFrameStrata("FULLSCREEN_DIALOG")
    frame:SetToplevel(true)
    frame:SetSize(WIDTH, HEIGHT)
    frame:SetPoint("TOPLEFT", nil, "TOPLEFT", 20, -20)
    frame:SetClampedToScreen(true)
    frame:EnableMouse(true)
    frame:SetMovable(true)
    frame:RegisterForDrag("LeftButton")
    frame:SetScript("OnDragStart", frame.StartMoving)
    frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
    frame:Hide()

    local bg = NewRect(frame, "BACKGROUND", 0.06, 0.06, 0.08, 0.92)
    bg:SetAllPoints(frame)
    local border = NewRect(frame, "BORDER", 0.3, 0.5, 0.9, 0.6)
    border:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, 0)
    border:SetPoint("TOPRIGHT", frame, "TOPRIGHT", 0, 0)
    border:SetHeight(2)

    local title = NewText(frame, 13, true, "LEFT")
    title:SetPoint("TOPLEFT", frame, "TOPLEFT", 10, -8)
    title:SetText("Graphics Profiler")

    frame.status = NewText(frame, 10, false, "LEFT")
    frame.status:SetPoint("LEFT", title, "RIGHT", 12, 0)
    SetColor(frame.status, COLOR_DIM)

    HeaderButton("Close", 60, -8, function() GraphicsProfiler_Toggle() end)
    HeaderButton("Dump to log", 90, -74, function()
        local path = GraphicsProfiler_Dump()
        Print(path and ("written to " .. path) or "could not write the report")
    end)
    frame.pause = HeaderButton("Pause", 60, -170, function(self)
        paused = not paused
        self.text:SetText(paused and "Resume" or "Pause")
        GraphicsProfiler_Refresh(true)
    end)

    for i, entry in ipairs(PAGES) do
        local tab = CreateFrame("Button", nil, frame)
        tab:SetSize(100, 20)
        tab:SetPoint("TOPLEFT", frame, "TOPLEFT", 10 + (i - 1) * 104, -30)
        tab.bg = NewRect(tab, "BACKGROUND", 1, 1, 1, 0.06)
        tab.bg:SetAllPoints(tab)
        tab.text = NewText(tab, 11, true, "CENTER")
        tab.text:SetAllPoints(tab)
        tab.text:SetText(entry[2])
        tab.key = entry[1]
        tab:SetScript("OnClick", function() ShowPage(entry[1]) end)
        tabs[i] = tab

        local page = CreateFrame("Frame", nil, frame)
        page:SetPoint("TOPLEFT", frame, "TOPLEFT", 0, -54)
        page:SetPoint("BOTTOMRIGHT", frame, "BOTTOMRIGHT", 0, 0)
        page:Hide()
        pages[entry[1]] = page
        entry[3](page)
    end

    frame:SetScript("OnUpdate", function(self, dt)
        elapsed = elapsed + dt

        if elapsed >= REFRESH then
            elapsed = 0
            GraphicsProfiler_Refresh(false)
        end
    end)

    ShowPage(currentPage)
end

function GraphicsProfiler_SetShown(shown)
    if not frame then
        CreateWindow()
    end

    if shown then
        frame:Show()
        GraphicsProfiler_Refresh(true)
    else
        frame:Hide()
    end
end

if SlashCmdList then
    SLASH_GRAPHICSPROFILER1 = "/gfxprof"
    SlashCmdList["GRAPHICSPROFILER"] = function(msg)
        msg = string.lower(msg or "")

        if msg == "dump" then
            local path = GraphicsProfiler_Dump()
            Print(path and ("written to " .. path) or "could not write the report")
        else
            GraphicsProfiler_Toggle()
        end
    end
end

CreateWindow()
