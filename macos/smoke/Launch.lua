#@ SimpleGraphic

local restartMarker = assert(os.getenv("SIMPLEGRAPHIC_SMOKE_RESTART_MARKER"))
local marker = io.open(restartMarker, "r")
local restarted = marker ~= nil
if marker then
	marker:close()
	os.remove(restartMarker)
end

local smoke = { frames = 0, restarted = restarted, subscriptPassed = false }
SetMainObject(smoke)
SetWindowTitle("SimpleGraphic macOS smoke")
ConExecute("set vid_mode 8")
ConExecute("set vid_resizable 3")

function smoke:Fail(message)
	local failureMarker = assert(io.open(restartMarker .. "-failed", "w"))
	failureMarker:write(message)
	failureMarker:close()
	ConPrintf("SMOKE FAILED: %s\n", message)
	Exit()
end

function smoke:RunChecks()
	RenderInit("DPI_AWARE")
	SetClearColor(0.04, 0.08, 0.12, 1)

	local payload = "SimpleGraphic macOS arm64 smoke"
	assert(Inflate(Deflate(payload)) == payload)
	assert(type(GetScriptPath()) == "string")
	assert(type(GetRuntimePath()) == "string")
	assert(type(GetUserPath()) == "string")
	assert(require("lcurl.safe"))
	assert(require("lua-utf8"))
	assert(require("lzip"))
	local socket = assert(require("socket.core"))
	local listener = assert(socket.tcp())
	assert(listener:bind("127.0.0.1", 0))
	listener:close()

	self.subscript = assert(LaunchSubScript([[
		assert(require("lcurl.safe"))
		assert(require("lua-utf8"))
		assert(require("lzip"))
		assert(require("socket.core"))
		return true
	]], "", ""))
	ConPrintf("SMOKE: main-state modules, compression, paths, and localhost socket passed.\n")
end

function smoke:OnInit()
	local passed, errorMessage = pcall(self.RunChecks, self)
	if not passed then
		self:Fail(errorMessage)
	end
end

function smoke:OnSubError(_, errorMessage)
	self:Fail("subscript error: " .. errorMessage)
end

function smoke:OnSubFinished(_, passed)
	if passed ~= true then
		self:Fail("subscript returned without success")
		return
	end
	self.subscriptPassed = true
	ConPrintf("SMOKE: subscript-state native modules passed.\n")
end

function smoke:OnFrame()
	local width, height = GetScreenSize()
	SetDrawColor(0.12, 0.55, 0.82, 1)
	DrawImage(nil, width / 4, height / 4, width / 2, height / 2)
	self.frames = self.frames + 1
	if self.frames >= 120 and self.subscriptPassed then
		ConPrintf("SMOKE: ANGLE frame loop passed at %dx%d scale %.2f.\n", width, height, GetScreenScale())
		if self.restarted then
			local successMarker = assert(io.open(restartMarker .. "-passed", "w"))
			successMarker:write("passed")
			successMarker:close()
			ConPrintf("SMOKE: restart loop passed; exiting cleanly.\n")
			Exit()
		else
			local markerFile = assert(io.open(restartMarker, "w"))
			markerFile:write("restart")
			markerFile:close()
			ConPrintf("SMOKE: restarting Lua host.\n")
			Restart()
		end
	end
end
