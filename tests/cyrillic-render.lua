-- Cyrillic Notes spec clause 3: exercise the actual renderer with generated glyphs.
local failures = 0
local count = 0
local report = assert(io.open("render-result.txt", "w"))
local function check(name, actual, expected)
	count = count + 1
	local success = actual == expected
	failures = failures + (success and 0 or 1)
	report:write(string.format("%s: %s (expected %s, got %s)\n", success and "PASS" or "FAIL", name, expected, actual))
	report:flush()
end
SetCallback("OnInit", function()
	RenderInit()
	SetDPIScaleOverridePercent(100)
	local utf8 = require("lua-utf8")
	local metadata = assert(io.open("SimpleGraphic/Fonts/Liberation Sans.tgf", "r"))
	local text = metadata:read("*a")
	metadata:close()
	local heights, widths, height, index = {}, nil, nil, 0
	for line in text:gmatch("[^\n]+") do
		local size = tonumber(line:match("^HEIGHT (%d+);"))
		if size then
			height, index, widths = size, 0, {}
			if size > 0 then heights[size] = widths end
		else
			local codepoint, width, left, right = line:match("^GLYPH_U32 (%d+) %s*%d+ %s*%d+ %s*(%d+) %s*(-?%d+) %s*(-?%d+)")
			if not codepoint then
				width, left, right = line:match("^GLYPH %s*%d+ %s*%d+ %s*(%d+) %s*(-?%d+) %s*(-?%d+)")
				if width then codepoint, index = index, index + 1 end
			end
			if codepoint then widths[tonumber(codepoint)] = tonumber(width) + tonumber(left) + tonumber(right) end
		end
	end
	for height, widths in pairs(heights) do
		local renderedHeight = height + height % 2
		local function width(codepoint) return math.ceil(widths[codepoint] * renderedHeight / height) end
		for _, codepoint in ipairs({ 0x401, 0x451 }) do
			check("measures Cyrillic " .. codepoint .. " at " .. height, DrawStringWidth(height, "VAR", utf8.char(codepoint)), width(codepoint))
		end
		for codepoint = 0x410, 0x44f do
			check("measures Cyrillic " .. codepoint .. " at " .. height, DrawStringWidth(height, "VAR", utf8.char(codepoint)), width(codepoint))
		end
		local english, expected = "Minion Damage Cluster", 0
		for index = 1, #english do expected = expected + width(english:byte(index)) end
		check("preserves ASCII at " .. height, DrawStringWidth(height, "VAR", english), expected)
		check("ignores color escapes at " .. height, DrawStringWidth(height, "VAR", "^xFFFF77" .. utf8.char(0x401) .. "^7"), width(0x401))
		check("finds UTF-8 byte offsets at " .. height, DrawStringCursorIndex(height, "VAR", utf8.char(0x401) .. "A", width(0x401) + 1, 0), 3)
	end
	check("retains placeholders for missing glyphs", DrawStringWidth(20, "VAR", utf8.char(0x4e00)), DrawStringWidth(16, "VAR", "[U+4E00]"))
	report:write(string.format("%d tests, %d failures\n", count, failures))
	report:close()
	Exit()
end)
