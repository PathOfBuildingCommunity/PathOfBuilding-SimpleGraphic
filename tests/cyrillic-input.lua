-- Cyrillic Notes spec clause 2: Windows driver sends characters and named keys.
local utf8 = require("lua-utf8")
local expected = { "A", utf8.char(0x401), utf8.char(0x44f), utf8.char(0x451), utf8.char(0x100), utf8.char(0x20ac), utf8.char(0x10400) }
local report = assert(io.open("input-result.txt", "w"))
local count, failures = 0, 0
local function check(actual, value)
	count = count + 1
	failures = failures + (actual == value and 0 or 1)
	report:write(string.format("%s: expected %s, got %s\n", actual == value and "PASS" or "FAIL", value, actual))
	report:flush()
end
SetCallback("OnInit", function()
	RenderInit()
	SetWindowTitle("Cyrillic input regression")
end)
SetCallback("OnFrame", function() end)
SetCallback("OnChar", function(value)
	check(value, expected[count + 1])
end)
SetCallback("OnKeyDown", function(value)
	check(value, "LEFT")
	report:write(string.format("%d tests, %d failures\n", count, failures))
	report:close()
	Exit()
end)
