// DyLua: SimpleGraphic
// (c) David Gowor, 2014
//
// Module: UI API
//

#include "ui_local.h"

#include <zlib.h>

/* OnFrame()
** OnChar("<char>")
** OnKeyDown("<keyName>")
** OnKeyUp("<keyName>")
** canExit = CanExit()
** OnExit()
** OnSubCall("<name>", ...)
** OnSubError(ssID, "<errorMsg>")
** OnSubFinished(ssID, ...)
**
** SetCallback("<name>"[, func])
** func = GetCallback("<name>")
** SetMainObject(object)
**
** imgHandle = NewImageHandle()
** imgHandle:Load("<fileName>"[, "flag1"[, "flag2"...]])  flag:{"ASYNC"|"CLAMP"|"MIPMAP"}
** imgHandle:Unload()
** isValid = imgHandle:IsValid()
** isLoading = imgHandle:IsLoading()
** imgHandle:SetLoadingPriority(pri)
** width, height = imgHandle:ImageSize()
**
** RenderInit()
** width, height = GetScreenSize()
** SetClearColor(red, green, blue[, alpha])
** SetDrawLayer({layer|nil}[, subLayer)
** GetDrawLayer()
** SetViewport([x, y, width, height])
** SetDrawColor(red, green, blue[, alpha]) / SetDrawColor("<escapeStr>")
** DrawImage({imgHandle|nil}, left, top, width, height[, tcLeft, tcTop, tcRight, tcBottom])
** DrawImageQuad({imgHandle|nil}, x1, y1, x2, y2, x3, y3, x4, y4[, s1, t1, s2, t2, s3, t3, s4, t4])
** DrawString(left, top, align{"LEFT"|"CENTER"|"RIGHT"|"CENTER_X"|"RIGHT_X"}, height, font{"FIXED"|"VAR"|"VAR BOLD"}, "<text>")
** width = DrawStringWidth(height, font{"FIXED"|"VAR"|"VAR BOLD"}, "<text>")
** index = DrawStringCursorIndex(height, font{"FIXED"|"VAR"|"VAR BOLD"}, "<text>", cursorX, cursorY)
** str = StripEscapes("<string>")
** count = GetAsyncCount()
**
** searchHandle = NewFileSearch("<spec>"[, findDirectories])
** found = searchHandle:NextFile()
** fileName = searchHandle:GetFileName()
** fileSize = searchHandle:GetFileSize()
** modified, date, time = searchHande:GetFileModifiedTime() 
**
** SetWindowTitle("<title>")
** x, y = GetCursorPos()
** SetCursorPos(x, y)
** ShowCursor(doShow)
** down = IsKeyDown("<keyName>")
** Copy("<string>")
** string = Paste()
** compressed = Deflate(uncompressed)
** uncompressed = Inflate(compressed)
** msec = GetTime()
** path = GetScriptPath()
** path = GetRuntimePath()
** path = GetUserPath()
** SetWorkDir("<path>")
** path = GetWorkDir()
** ssID = LaunchSubScript("<scriptText>", "<funcList>", "<subList>"[, ...])
** AbortSubScript(ssID)
** isRunning = IsSubScriptRunning(ssID)
** ... = LoadModule("<modName>"[, ...])
** err, ... = PLoadModule("<modName>"[, ...])
** err, ... = PCall(func[, ...])
** ConPrintf("<format>"[, ...])
** ConPrintTable(table[, noRecurse])
** ConExecute("<cmd>")
** SpawnProcess("<cmdName>"[, "<args>"])
** OpenURL("<url>")
** SetProfiling(isEnabled)
** Restart()
** Exit(["<message>"])
*/

// Grab UI main pointer from the registry
static ui_main_c* GetUIPtr(lua_State* L)
{
	lua_rawgeti(L, LUA_REGISTRYINDEX, 0);
	ui_main_c* ui = (ui_main_c*)lua_touserdata(L, -1);
	lua_pop(L, 1);
	return ui;
}

// =========
// Callbacks
// =========

static int l_SetCallback(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetCallback(name[, func])");
	ui->LAssert(L, lua_isstring(L, 1), "SetCallback() argument 1: expected string, got %t", 1);
	lua_pushvalue(L, 1);
	if (n >= 2) {
		ui->LAssert(L, lua_isfunction(L, 2) || lua_isnil(L, 2), "SetCallback() argument 2: expected function or nil, got %t", 2);
		lua_pushvalue(L, 2);
	} else {
		lua_pushnil(L);
	}
	lua_settable(L, lua_upvalueindex(1));
	return 0;
}

static int l_GetCallback(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: GetCallback(name)");
	ui->LAssert(L, lua_isstring(L, 1), "GetCallback() argument 1: expected string, got %t", 1);
	lua_pushvalue(L, 1);
	lua_gettable(L, lua_upvalueindex(1));
	return 1;
}

static int l_SetMainObject(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	lua_pushstring(L, "MainObject");
	if (n >= 1) {
		ui->LAssert(L, lua_istable(L, 1) || lua_isnil(L, 1), "SetMainObject() argument 1: expected table or nil, got %t", 1);
		lua_pushvalue(L, 1);
	} else {
		lua_pushnil(L);
	}
	lua_settable(L, lua_upvalueindex(1));
	return 0;
}

// =============
// Image Handles
// =============

struct imgHandle_s {
	r_shaderHnd_c* hnd;
};

static int l_NewImageHandle(lua_State* L)
{
	imgHandle_s* imgHandle = (imgHandle_s*)lua_newuserdata(L, sizeof(imgHandle_s));
	imgHandle->hnd = NULL;
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	return 1;
}

static imgHandle_s* GetImgHandle(lua_State* L, ui_main_c* ui, const char* method, bool loaded)
{
	ui->LAssert(L, ui->IsUserData(L, 1, "uiimghandlemeta"), "imgHandle:%s() must be used on an image handle", method);
	imgHandle_s* imgHandle = (imgHandle_s*)lua_touserdata(L, 1);
	lua_remove(L, 1);
	if (loaded) {
		ui->LAssert(L, imgHandle->hnd != NULL, "imgHandle:%s(): image handle has no image loaded", method);
	}
	return imgHandle;
}

static int l_imgHandleGC(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "__gc", false);
	delete imgHandle->hnd;
	return 0;
}

static int l_imgHandleLoad(lua_State* L) 
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "Load", false);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: imgHandle:Load(fileName[, flag1[, flag2...]])");
	ui->LAssert(L, lua_isstring(L, 1), "imgHandle:Load() argument 1: expected string, got %t", 1);
	const char* fileName = lua_tostring(L, 1);
	char fullFileName[512];
	if (strchr(fileName, ':') || !ui->scriptWorkDir) {
		strcpy(fullFileName, fileName);
	} else {
		sprintf(fullFileName, "%s\\%s", ui->scriptWorkDir, fileName);
	}
	delete imgHandle->hnd;
	int flags = TF_NOMIPMAP;
	for (int f = 2; f <= n; f++) {
		if ( !lua_isstring(L, f) ) {
			continue;
		}
		const char* flag = lua_tostring(L, f);
		if ( !strcmp(flag, "ASYNC") ) {
			// Async texture loading removed
		} else if ( !strcmp(flag, "CLAMP") ) {
			flags|= TF_CLAMP;
		} else if ( !strcmp(flag, "MIPMAP") ) {
			flags&= ~TF_NOMIPMAP;
		} else if ( !strcmp(flag, "NEAREST") ) {
			flags|= TF_NEAREST;
		} else {
			ui->LAssert(L, 0, "imgHandle:Load(): unrecognised flag '%s'", flag);
		}
	}
	imgHandle->hnd = ui->renderer->RegisterShader(fullFileName, flags);
	return 0;
}

static int l_imgHandleUnload(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "Unload", false);
	delete imgHandle->hnd;
	imgHandle->hnd = NULL;
	return 0;
}

static int l_imgHandleIsValid(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "IsValid", false);
	lua_pushboolean(L, imgHandle->hnd != NULL);
	return 1;
}

static int l_imgHandleIsLoading(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "IsLoading", true);
	int width, height;
	ui->renderer->GetShaderImageSize(imgHandle->hnd, width, height);
	lua_pushboolean(L, width == 0);
	return 1;
}

static int l_imgHandleSetLoadingPriority(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "SetLoadingPriority", true);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: imgHandle:SetLoadingPriority(pri)");
	ui->LAssert(L, lua_isnumber(L, 1), "imgHandle:SetLoadingPriority() argument 1: expected number, got %t", 1);
	ui->renderer->SetShaderLoadingPriority(imgHandle->hnd, (int)lua_tointeger(L, 1));
	return 0;
}

static int l_imgHandleImageSize(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	imgHandle_s* imgHandle = GetImgHandle(L, ui, "ImageSize", true);
	int width, height;
	ui->renderer->GetShaderImageSize(imgHandle->hnd, width, height);
	lua_pushinteger(L, width);
	lua_pushinteger(L, height);
	return 2;
}

// =========
// Rendering
// =========

static int l_RenderInit(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->RenderInit();
	return 0;
}

static int l_GetScreenSize(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushinteger(L, ui->sys->video->vid.size[0]);
	lua_pushinteger(L, ui->sys->video->vid.size[1]);
	return 2;
}

static int l_SetClearColor(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 3, "Usage: SetClearColor(red, green, blue[, alpha])");
	col4_t color;
	for (int i = 1; i <= 3; i++) {
		ui->LAssert(L, lua_isnumber(L, i), "SetClearColor() argument %d: expected number, got %t", i, i);
		color[i-1] = (float)lua_tonumber(L, i);
	}
	if (n >= 4 && !lua_isnil(L, 4)) {
		ui->LAssert(L, lua_isnumber(L, 4), "SetClearColor() argument 4: expected number or nil, got %t", 4);
		color[3] = (float)lua_tonumber(L, 4);
	} else {
		color[3] = 1.0;
	}
	ui->renderer->SetClearColor(color);
	return 0;
}

static int l_SetDrawLayer(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "SetDrawLayer() called outside of OnFrame");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetDrawLayer({layer|nil}[, subLayer])");
	ui->LAssert(L, lua_isnumber(L, 1) || lua_isnil(L, 1), "SetDrawLayer() argument 1: expected number or nil, got %t", 1);
	if (n >= 2) {
		ui->LAssert(L, lua_isnumber(L, 2), "SetDrawLayer() argument 2: expected number, got %t", 2);
	}
	if (lua_isnil(L, 1)) {
		ui->LAssert(L, n >= 2, "SetDrawLayer(): must provide subLayer if layer is nil");
		ui->renderer->SetDrawSubLayer((int)lua_tointeger(L, 2));
	} else if (n >= 2) {
		ui->renderer->SetDrawLayer((int)lua_tointeger(L, 1), (int)lua_tointeger(L, 2));
	} else {
		ui->renderer->SetDrawLayer((int)lua_tointeger(L, 1));
	}
	return 0;
}

static int l_GetDrawLayer(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushinteger(L, ui->renderer->GetDrawLayer());
	return 1;
}

static int l_SetViewport(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "SetViewport() called outside of OnFrame");
	int n = lua_gettop(L);
	if (n) {
		ui->LAssert(L, n >= 4, "Usage: SetViewport([x, y, width, height])");
		for (int i = 1; i <= 4; i++) {
			ui->LAssert(L, lua_isnumber(L, i), "SetViewport() argument %d: expected number, got %t", i, i);
		}
		ui->renderer->SetViewport((int)lua_tointeger(L, 1), (int)lua_tointeger(L, 2), (int)lua_tointeger(L, 3), (int)lua_tointeger(L, 4));
	} else {
		ui->renderer->SetViewport();
	}
	return 0;
}

static int l_SetBlendMode(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "SetViewport() called outside of OnFrame");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetBlendMode(mode)");
	static const char* modeMap[6] = { "ALPHA", "PREALPHA", "ADDITIVE", NULL };
	ui->renderer->SetBlendMode(luaL_checkoption(L, 1, "ALPHA", modeMap));
	return 0;
}

static int l_SetDrawColor(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "SetDrawColor() called outside of OnFrame");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetDrawColor(red, green, blue[, alpha]) or SetDrawColor(escapeStr)");
	col4_t color;
	if (lua_type(L, 1) == LUA_TSTRING) {
		ui->LAssert(L, IsColorEscape(lua_tostring(L, 1)), "SetDrawColor() argument 1: invalid color escape sequence");
		ReadColorEscape(lua_tostring(L, 1), color);
		color[3] = 1.0;
	} else {
		ui->LAssert(L, n >= 3, "Usage: SetDrawColor(red, green, blue[, alpha]) or SetDrawColor(escapeStr)");
		for (int i = 1; i <= 3; i++) {
			ui->LAssert(L, lua_isnumber(L, i), "SetDrawColor() argument %d: expected number, got %t", i, i);
			color[i-1] = (float)lua_tonumber(L, i);
		}
		if (n >= 4 && !lua_isnil(L, 4)) {
			ui->LAssert(L, lua_isnumber(L, 4), "SetDrawColor() argument 4: expected number or nil, got %t", 4);
			color[3] = (float)lua_tonumber(L, 4);
		} else {
			color[3] = 1.0;
		}
	}
	ui->renderer->DrawColor(color);
	return 0;
}

static int l_DrawImage(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "DrawImage() called outside of OnFrame");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 5, "Usage: DrawImage({imgHandle|nil}, left, top, width, height[, tcLeft, tcTop, tcRight, tcBottom])");
	ui->LAssert(L, lua_isnil(L, 1) || ui->IsUserData(L, 1, "uiimghandlemeta"), "DrawImage() argument 1: expected image handle or nil, got %t", 1);
	r_shaderHnd_c* hnd = NULL;
	if ( !lua_isnil(L, 1) ) {
		imgHandle_s* imgHandle = (imgHandle_s*)lua_touserdata(L, 1);
		ui->LAssert(L, imgHandle->hnd != NULL, "DrawImage(): image handle has no image loaded");
		hnd = imgHandle->hnd;
	}
	float arg[8];
	if (n > 5) {
		ui->LAssert(L, n >= 9, "DrawImage(): incomplete set of texture coordinates provided");
		for (int i = 2; i <= 9; i++) {
			ui->LAssert(L, lua_isnumber(L, i), "DrawImage() argument %d: expected number, got %t", i, i);
			arg[i-2] = (float)lua_tonumber(L, i);
		}
		ui->renderer->DrawImage(hnd, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);
	} else {
		for (int i = 2; i <= 5; i++) {
			ui->LAssert(L, lua_isnumber(L, i), "DrawImage() argument %d: expected number, got %t", i, i);
			arg[i-2] = (float)lua_tonumber(L, i);
		}
		ui->renderer->DrawImage(hnd, arg[0], arg[1], arg[2], arg[3]);
	}
	return 0;
}

static int l_DrawImageQuad(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "DrawImageQuad() called outside of OnFrame");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 9, "Usage: DrawImageQuad({imgHandle|nil}, x1, y1, x2, y2, x3, y3, x4, y4[, s1, t1, s2, t2, s3, t3, s4, t4])");
	ui->LAssert(L, lua_isnil(L, 1) || ui->IsUserData(L, 1, "uiimghandlemeta"), "DrawImageQuad() argument 1: expected image handle or nil, got %t", 1);
	r_shaderHnd_c* hnd = NULL;
	if ( !lua_isnil(L, 1) ) {
		imgHandle_s* imgHandle = (imgHandle_s*)lua_touserdata(L, 1);
		ui->LAssert(L, imgHandle->hnd != NULL, "DrawImageQuad(): image handle has no image loaded");
		hnd = imgHandle->hnd;
	}
	float arg[16];
	if (n > 9) {
		ui->LAssert(L, n >= 17, "DrawImageQuad(): incomplete set of texture coordinates provided");
		for (int i = 2; i <= 17; i++) {
			ui->LAssert(L, lua_isnumber(L, i), "DrawImageQuad() argument %d: expected number, got %t", i, i);
			arg[i-2] = (float)lua_tonumber(L, i);
		}
		ui->renderer->DrawImageQuad(hnd, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7], arg[8], arg[9], arg[10], arg[11], arg[12], arg[13], arg[14], arg[15]);
	} else {
		for (int i = 2; i <= 9; i++) {
			ui->LAssert(L, lua_isnumber(L, i), "DrawImageQuad() argument %d: expected number, got %t", i, i);
			arg[i-2] = (float)lua_tonumber(L, i);
		}
		ui->renderer->DrawImageQuad(hnd, arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7]);
	}
	return 0;
}

static int l_DrawString(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	ui->LAssert(L, ui->renderEnable, "DrawString() called outside of OnFrame");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 6, "Usage: DrawString(left, top, align, height, font, text)");
	ui->LAssert(L, lua_isnumber(L, 1), "DrawString() argument 1: expected number, got %t", 1);
	ui->LAssert(L, lua_isnumber(L, 2), "DrawString() argument 2: expected number, got %t", 2);
	ui->LAssert(L, lua_isstring(L, 3) || lua_isnil(L, 3), "DrawString() argument 3: expected string or nil, got %t", 3);
	ui->LAssert(L, lua_isnumber(L, 4), "DrawString() argument 4: expected number, got %t", 4);
	ui->LAssert(L, lua_isstring(L, 5), "DrawString() argument 5: expected string, got %t", 5);
	ui->LAssert(L, lua_isstring(L, 6), "DrawString() argument 6: expected string, got %t", 6);
	static const char* alignMap[6] = { "LEFT", "CENTER", "RIGHT", "CENTER_X", "RIGHT_X", NULL };
	static const char* fontMap[4] = { "FIXED", "VAR", "VAR BOLD", NULL };
	ui->renderer->DrawString(
		(float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2), luaL_checkoption(L, 3, "LEFT", alignMap), 
		(int)lua_tointeger(L, 4), NULL, luaL_checkoption(L, 5, "FIXED", fontMap), lua_tostring(L, 6)
	);
	return 0;
}

static int l_DrawStringWidth(lua_State* L) 
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 3, "Usage: DrawStringWidth(height, font, text)");
	ui->LAssert(L, lua_isnumber(L, 1), "DrawStringWidth() argument 1: expected number, got %t", 1);
	ui->LAssert(L, lua_isstring(L, 2), "DrawStringWidth() argument 2: expected string, got %t", 2);
	ui->LAssert(L, lua_isstring(L, 3), "DrawStringWidth() argument 3: expected string, got %t", 3);
	static const char* fontMap[4] = { "FIXED", "VAR", "VAR BOLD", NULL };
	lua_pushinteger(L, ui->renderer->DrawStringWidth((int)lua_tointeger(L, 1), luaL_checkoption(L, 2, "FIXED", fontMap), lua_tostring(L, 3)));
	return 1;
}

static int l_DrawStringCursorIndex(lua_State* L) 
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 5, "Usage: DrawStringCursorIndex(height, font, text, cursorX, cursorY)");
	ui->LAssert(L, lua_isnumber(L, 1), "DrawStringCursorIndex() argument 1: expected number, got %t", 1);
	ui->LAssert(L, lua_isstring(L, 2), "DrawStringCursorIndex() argument 2: expected string, got %t", 2);
	ui->LAssert(L, lua_isstring(L, 3), "DrawStringCursorIndex() argument 3: expected string, got %t", 3);
	ui->LAssert(L, lua_isnumber(L, 4), "DrawStringCursorIndex() argument 4: expected number, got %t", 4);
	ui->LAssert(L, lua_isnumber(L, 5), "DrawStringCursorIndex() argument 5: expected number, got %t", 5);
	static const char* fontMap[4] = { "FIXED", "VAR", "VAR BOLD", NULL };
	lua_pushinteger(L, ui->renderer->DrawStringCursorIndex((int)lua_tointeger(L, 1), luaL_checkoption(L, 2, "FIXED", fontMap), lua_tostring(L, 3), (int)lua_tointeger(L, 4), (int)lua_tointeger(L, 5)) + 1);
	return 1;
}

static int l_StripEscapes(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: StripEscapes(string)");
	ui->LAssert(L, lua_isstring(L, 1), "StripEscapes() argument 1: expected string, got %t", 1);
	const char* str = lua_tostring(L, 1);
	char* strip = new char[strlen(str) + 1];
	char* p = strip;
	while (*str) {
		int esclen = IsColorEscape(str);
		if (esclen) {
			str+= esclen;
		} else {
			*(p++) = *(str++);
		}
	}
	*p = 0;
	lua_pushstring(L, strip);
	delete[] strip;
	return 1;
}

static int l_GetAsyncCount(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->LAssert(L, ui->renderer != NULL, "Renderer is not initialised");
	lua_pushinteger(L, ui->renderer->GetTexAsyncCount());
	return 1;
}

// ==============
// Search Handles
// ==============

struct searchHandle_s {
	find_c* find;
	bool	dirOnly;
};

static int l_NewFileSearch(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: NewFileSearch(spec[, findDirectories])");
	ui->LAssert(L, lua_isstring(L, 1), "NewFileSearch() argument 1: expected string, got %t", 1);
	find_c* find = new find_c();
	if ( !find->FindFirst(lua_tostring(L, 1)) ) {
		delete find;
		return 0;
	}
	bool dirOnly = lua_toboolean(L, 2) != 0;
	while (find->isDirectory != dirOnly || find->fileName == "." || find->fileName == "..") {
		if ( !find->FindNext() ) {
			delete find;
			return 0;
		}
	}
	searchHandle_s* searchHandle = (searchHandle_s*)lua_newuserdata(L, sizeof(searchHandle_s));
	searchHandle->find = find;
	searchHandle->dirOnly = dirOnly;
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_setmetatable(L, -2);
	return 1;
}

static searchHandle_s* GetSearchHandle(lua_State* L, ui_main_c* ui, const char* method, bool valid)
{
	ui->LAssert(L, ui->IsUserData(L, 1, "uisearchhandlemeta"), "searchHandle:%s() must be used on a search handle", method);
	searchHandle_s* searchHandle = (searchHandle_s*)lua_touserdata(L, 1);
	lua_remove(L, 1);
	if (valid) {
		ui->LAssert(L, searchHandle->find != NULL, "searchHandle:%s(): search handle is no longer valid (ran out of files to find)", method);
	}
	return searchHandle;
}

static int l_searchHandleGC(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	searchHandle_s* searchHandle = GetSearchHandle(L, ui, "__gc", false);
	delete searchHandle->find;
	return 0;
}

static int l_searchHandleNextFile(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	searchHandle_s* searchHandle = GetSearchHandle(L, ui, "NextFile", true);
	do {
		if ( !searchHandle->find->FindNext() ) {
			delete searchHandle->find;
			searchHandle->find = NULL;
			return 0;
		}
	} while (searchHandle->find->isDirectory != searchHandle->dirOnly || searchHandle->find->fileName == "." || searchHandle->find->fileName == "..");
	lua_pushboolean(L, 1);
	return 1;
}

static int l_searchHandleGetFileName(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	searchHandle_s* searchHandle = GetSearchHandle(L, ui, "GetFileName", true);
	lua_pushstring(L, searchHandle->find->fileName.c_str());
	return 1;
}

static int l_searchHandleGetFileSize(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	searchHandle_s* searchHandle = GetSearchHandle(L, ui, "GetFileSize", true);
	lua_pushinteger(L, (lua_Integer)searchHandle->find->fileSize);
	return 1;
}

static int l_searchHandleGetFileModifiedTime(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	searchHandle_s* searchHandle = GetSearchHandle(L, ui, "GetFileModifiedTime", true);
	lua_pushnumber(L, (double)searchHandle->find->modified);
	return 1;
}

// =================
// General Functions
// =================

static int l_SetWindowTitle(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetWindowTitle(title)");
	ui->LAssert(L, lua_isstring(L, 1), "SetWindowTitle() argument 1: expected string, got %t", 1);
	ui->sys->video->SetTitle(lua_tostring(L, 1));
	ui->sys->conWin->SetTitle(lua_tostring(L, 1));
	return 0;
}

static int l_GetCursorPos(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushinteger(L, ui->cursorX);
	lua_pushinteger(L, ui->cursorY);
	return 2;
}

static int l_SetCursorPos(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 2, "Usage: SetCursorPos(x, y)");
	ui->LAssert(L, lua_isnumber(L, 1), "SetCursorPos() argument 1: expected number, got %t", 1);
	ui->LAssert(L, lua_isnumber(L, 2), "SetCursorPos() argument 2: expected number, got %t", 2);
	ui->sys->video->SetRelativeCursor((int)lua_tointeger(L, 1), (int)lua_tointeger(L, 2));
	return 0;
}

static int l_ShowCursor(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: ShowCursor(doShow)");
	ui->sys->ShowCursor(lua_toboolean(L, 1));
	return 0;
}

static int l_IsKeyDown(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: IsKeyDown(keyName)");
	ui->LAssert(L, lua_isstring(L, 1), "IsKeyDown() argument 1: expected string, got %t", 1);
	size_t len;
	const char* kname = lua_tolstring(L, 1, &len);
	ui->LAssert(L, len >= 1, "IsKeyDown() argument 1: string is empty", 1);
	int key = ui->KeyForName(kname);
	ui->LAssert(L, key, "IsKeyDown(): unrecognised key name");
	lua_pushboolean(L, ui->sys->IsKeyDown(key));
	return 1;
}

static int l_Copy(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: Copy(string)");
	ui->LAssert(L, lua_isstring(L, 1), "Copy() argument 1: expected string, got %t", 1);
	ui->sys->ClipboardCopy(lua_tostring(L, 1));
	return 0;
}

static int l_Paste(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	char* data = ui->sys->ClipboardPaste();
	if (data) {
		lua_pushstring(L, data);
		FreeString(data);
		return 1;
	} else {
		return 0;
	}
}

static int l_Deflate(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: Deflate(string)");
	ui->LAssert(L, lua_isstring(L, 1), "Deflate() argument 1: expected string, got %t", 1);
	z_stream_s z;
	z.zalloc = NULL;
	z.zfree = NULL;
	deflateInit(&z, 9);
	size_t inLen;
	byte* in = (byte*)lua_tolstring(L, 1, &inLen);
	int outSz = deflateBound(&z, inLen);
	byte* out = new byte[outSz];
	z.next_in = in;
	z.avail_in = inLen;
	z.next_out = out;
	z.avail_out = outSz;
	int err = deflate(&z, Z_FINISH);
	deflateEnd(&z);
	if (err == Z_STREAM_END) {
		lua_pushlstring(L, (const char*)out, z.total_out);
		return 1;
	} else {
		lua_pushnil(L);
		lua_pushstring(L, zError(err));
		return 2;
	}
}

static int l_Inflate(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: Inflate(string)");
	ui->LAssert(L, lua_isstring(L, 1), "Inflate() argument 1: expected string, got %t", 1);
	size_t inLen;
	byte* in = (byte*)lua_tolstring(L, 1, &inLen);
	int outSz = inLen * 4;
	byte* out = new byte[outSz];
	z_stream_s z;
	z.next_in = in;
	z.avail_in = inLen;
	z.zalloc = NULL;
	z.zfree = NULL;
	z.next_out = out;
	z.avail_out = outSz;
	inflateInit(&z);
	int err;
	while ((err = inflate(&z, Z_NO_FLUSH)) == Z_OK) {
		if (z.avail_out == 0) {
			// Output buffer filled, embiggen it
			int newSz = outSz << 1;
			trealloc(out, newSz);
			z.next_out = out + outSz;
			z.avail_out = outSz;
			outSz = newSz;
		}
	}
	inflateEnd(&z);
	if (err == Z_STREAM_END) {
		lua_pushlstring(L, (const char*)out, z.total_out);
		return 1;
	} else {
		lua_pushnil(L);
		lua_pushstring(L, zError(err));
		return 2;
	}
}

static int l_GetTime(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushinteger(L, ui->sys->GetTime());
	return 1;
}

static int l_GetScriptPath(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushstring(L, ui->scriptPath);
	return 1;
}

static int l_GetRuntimePath(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushstring(L, ui->sys->basePath);
	return 1;
}

static int l_GetUserPath(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushstring(L, ui->sys->userPath);
	return 1;
}

static int l_MakeDir(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: MakeDir(path)");
	ui->LAssert(L, lua_isstring(L, 1), "MakeDir() argument 1: expected string, got %t", 1);
	if (_mkdir(lua_tostring(L, 1))) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	} else {
		lua_pushboolean(L, true);
		return 1;
	}
}

static int l_RemoveDir(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: l_RemoveDir(path)");
	ui->LAssert(L, lua_isstring(L, 1), "l_RemoveDir() argument 1: expected string, got %t", 1);
	if (_rmdir(lua_tostring(L, 1))) {
		lua_pushnil(L);
		lua_pushstring(L, strerror(errno));
		return 2;
	} else {
		lua_pushboolean(L, true);
		return 1;
	}
}

static int l_SetWorkDir(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetWorkDir(path)");
	ui->LAssert(L, lua_isstring(L, 1), "SetWorkDir() argument 1: expected string, got %t", 1);
	const char* newWorkDir = lua_tostring(L, 1);
	if ( !ui->sys->SetWorkDir(newWorkDir) ) {
		if (ui->scriptWorkDir) {
			FreeString(ui->scriptWorkDir);
		}
		ui->scriptWorkDir = AllocString(newWorkDir);
	}
	return 0;
}

static int l_GetWorkDir(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	lua_pushstring(L, ui->scriptWorkDir);
	return 1;
}

static int l_LaunchSubScript(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 3, "Usage: LaunchSubScript(scriptText, funcList, subList[, ...])");
	for (int i = 1; i <= 3; i++) {
		ui->LAssert(L, lua_isstring(L, i), "LaunchSubScript() argument %d: expected string, got %t", i, i);
	}
	for (int i = 4; i <= n; i++) {
		ui->LAssert(L, lua_isnil(L, i) || lua_isboolean(L, i) || lua_isnumber(L, i) || lua_isstring(L, i), 
			"LaunchSubScript() argument %d: only nil, boolean, number and string types can be passed to sub script", i);
	}
	dword slot = -1;
	for (dword i = 0; i < ui->subScriptSize; i++) {
		if ( !ui->subScriptList[i] ) {
			slot = i;
			break;
		}
	}
	if (slot == -1) {
		slot = ui->subScriptSize;
		ui->subScriptSize<<= 1;
		trealloc(ui->subScriptList, ui->subScriptSize);
		for (dword i = slot; i < ui->subScriptSize; i++) {
			ui->subScriptList[i] = NULL;
		}
	}
	ui->subScriptList[slot] = ui_ISubScript::GetHandle(ui, slot);
	if (ui->subScriptList[slot]->Start()) {
		lua_pushlightuserdata(L, (void*)slot);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

static int l_AbortSubScript(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: AbortSubScript(ssID)");
	ui->LAssert(L, lua_islightuserdata(L, 1), "AbortSubScript() argument 1: expected subscript ID, got %t", 1);
	dword slot = (dword)lua_touserdata(L, 1);
	ui->LAssert(L, slot < ui->subScriptSize && ui->subScriptList[slot], "AbortSubScript() argument 1: invalid subscript ID");
	ui->LAssert(L, ui->subScriptList[slot]->IsRunning(), "AbortSubScript(): subscript isn't running");
	ui_ISubScript::FreeHandle(ui->subScriptList[slot]);
	ui->subScriptList[slot] = NULL;
	return 0;
}

static int l_IsSubScriptRunning(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: IsSubScriptRunning(ssID)");
	ui->LAssert(L, lua_islightuserdata(L, 1), "IsSubScriptRunning() argument 1: expected subscript ID, got %t", 1);
	dword slot = (dword)lua_touserdata(L, 1);
	ui->LAssert(L, slot < ui->subScriptSize && ui->subScriptList[slot], "IsSubScriptRunning() argument 1: invalid subscript ID");
	lua_pushboolean(L, ui->subScriptList[slot]->IsRunning());
	return 1;
}

static int l_LoadModule(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: LoadModule(name[, ...])");
	ui->LAssert(L, lua_isstring(L, 1), "LoadModule() argument 1: expected string, got %t", 1);
	const char* modName = lua_tostring(L, 1);
	char fileName[1024];
	strcpy(fileName, modName);
	if ( !strchr(fileName, '.') ) {
		strcat(fileName, ".lua");
	}
	ui->sys->SetWorkDir(ui->scriptPath);
	int err = luaL_loadfile(L, fileName);
	ui->sys->SetWorkDir(ui->scriptWorkDir);
	ui->LAssert(L, err == 0, "LoadModule() error loading '%s' (%d):\n%s", fileName, err, lua_tostring(L, -1));
	lua_replace(L, 1);	// Replace module name with module main chunk
	lua_call(L, n - 1, LUA_MULTRET);
	return lua_gettop(L);
}

static int l_PLoadModule(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: PLoadModule(name[, ...])");
	ui->LAssert(L, lua_isstring(L, 1), "PLoadModule() argument 1: expected string, got %t", 1);
	const char* modName = lua_tostring(L, 1);
	char* fileName = AllocStringLen(strlen(modName) + 4);
	strcpy(fileName, modName);
	if ( !strchr(fileName, '.') ) {
		strcat(fileName, ".lua");
	}
	ui->sys->SetWorkDir(ui->scriptPath);
	int err = luaL_loadfile(L, fileName);
	ui->sys->SetWorkDir(ui->scriptWorkDir);
	if (err) {
		return 1;
	}
	FreeString(fileName);
	lua_replace(L, 1);	// Replace module name with module main chunk
	lua_getfield(L, LUA_REGISTRYINDEX, "traceback");
	lua_insert(L, 1); // Insert traceback function at start of stack
	err = lua_pcall(L, n - 1, LUA_MULTRET, 1);
	if (err) {
		return 1;
	}
	lua_pushnil(L);
	lua_replace(L, 1); // Replace traceback function with nil
	return lua_gettop(L);
}

static int l_PCall(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: PCall(func[, ...])");
	ui->LAssert(L, lua_isfunction(L, 1), "PCall() argument 1: expected function, got %t", 1);
	lua_getfield(L, LUA_REGISTRYINDEX, "traceback");
	lua_insert(L, 1); // Insert traceback function at start of stack
	int err = lua_pcall(L, n - 1, LUA_MULTRET, 1);
	if (err) {
		return 1;
	}
	lua_pushnil(L);
	lua_replace(L, 1); // Replace traceback function with nil
	return lua_gettop(L);
}

static int l_ConPrintf(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: ConPrintf(fmt[, ...])");
	ui->LAssert(L, lua_isstring(L, 1), "ConPrintf() argument 1: expected string, got %t", 1);
	lua_pushvalue(L, lua_upvalueindex(1));	// string.format
	lua_insert(L, 1);
	lua_call(L, n, 1);
	ui->LAssert(L, lua_isstring(L, 1), "ConPrintf() error: string.format returned non-string");
	ui->sys->con->Printf("%s\n", lua_tostring(L, 1));
	return 0;
}

static void printTableItter(lua_State* L, IConsole* con, int index, int level, bool recurse)
{
	lua_checkstack(L, 5);
	lua_pushnil(L);
	while (lua_next(L, index)) {
		for (int t = 0; t < level; t++) con->Print("  ");
		// Print key
		if (lua_type(L, -2) == LUA_TSTRING) {
			con->Printf("[\"%s^7\"] = ", lua_tostring(L, -2));
		} else {
			lua_pushvalue(L, 2);	// Push tostring function
			lua_pushvalue(L, -3);	// Push key
			lua_call(L, 1, 1);		// Call tostring
			con->Printf("%s = ", lua_tostring(L, -1));
			lua_pop(L, 1);			// Pop result of tostring
		}
		// Print value
		if (lua_type(L, -1) == LUA_TTABLE) {
			bool expand = recurse;
			if (expand) {
				lua_pushvalue(L, -1);	// Push value
				lua_gettable(L, 3);		// Index printed tables list
				expand = lua_toboolean(L, -1) == 0;
				lua_pop(L, 1);			// Pop result of indexing
			}
			if (expand) {
				lua_pushvalue(L, -1);	// Push value
				lua_pushboolean(L, 1);
				lua_settable(L, 3);		// Add to printed tables list
				con->Printf("table: %08x {\n", lua_topointer(L, -1));
				printTableItter(L, con, lua_gettop(L), level + 1, true);
				for (int t = 0; t < level; t++) con->Print("  ");
				con->Print("}\n");
			} else {
				con->Printf("table: %08x { ... }\n", lua_topointer(L, -1));
			}
		} else if (lua_type(L, -1) == LUA_TSTRING) {
			con->Printf("\"%s\"\n", lua_tostring(L, -1));
		} else {
			lua_pushvalue(L, 2);	// Push tostring function
			lua_pushvalue(L, -2);	// Push value
			lua_call(L, 1, 1);		// Call tostring
			con->Printf("%s\n", lua_tostring(L, -1));
			lua_pop(L, 1);			// Pop result of tostring
		}
		lua_pop(L, 1);	// Pop value
	}
}

static int l_ConPrintTable(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: ConPrintTable(tbl[, noRecurse])");
	ui->LAssert(L, lua_istable(L, 1), "ConPrintTable() argument 1: expected table, got %t", 1);
	bool recurse = lua_toboolean(L, 2) == 0;
	lua_settop(L, 1);
	lua_getglobal(L, "tostring");
	lua_newtable(L);		// Printed tables list
	lua_pushvalue(L, 1);	// Push root table
	lua_pushboolean(L, 1);
	lua_settable(L, 3);		// Add root table to printed tables list
	printTableItter(L, ui->sys->con, 1, 0, recurse);
	return 0;
}

static int l_ConExecute(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: ConExecute(cmd)");
	ui->LAssert(L, lua_isstring(L, 1), "ConExecute() argument 1: expected string, got %t", 1);
	ui->sys->con->Execute(lua_tostring(L,1));
	return 0;
}

static int l_ConClear(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->sys->con->Clear();
	return 0;
}

static int l_print(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	lua_getglobal(L, "tostring");
	for (int i = 1; i <= n; i++) {
		lua_pushvalue(L, -1);	// Push tostring function
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);		// Call tostring
		const char* s = lua_tostring(L, -1);
		ui->LAssert(L, s != NULL, "print() error: tostring returned non-string");
		if (i > 1) ui->sys->con->Print(" ");
		ui->sys->con->Print(s);
		lua_pop(L, 1);			// Pop result of tostring
	}
	ui->sys->con->Print("\n");
	return 0;
}

static int l_SpawnProcess(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SpawnProcess(cmdName[, args])");
	ui->LAssert(L, lua_isstring(L, 1), "SpawnProcess() argument 1: expected string, got %t", 1);
	ui->sys->SpawnProcess(lua_tostring(L, 1), lua_tostring(L, 2));
	return 0;
}

static int l_OpenURL(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: OpenURL(url)");
	ui->LAssert(L, lua_isstring(L, 1), "OpenURL() argument 1: expected string, got %t", 1);
	ui->sys->OpenURL(lua_tostring(L, 1));
	return 0;
}

static int l_SetProfiling(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	ui->LAssert(L, n >= 1, "Usage: SetProfiling(isEnabled)");
	ui->debug->SetProfiling(lua_toboolean(L, 1) == 1);
	return 0;
}

static int l_Restart(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	ui->restartFlag = true;
	return 0;
}

static int l_Exit(lua_State* L)
{
	ui_main_c* ui = GetUIPtr(L);
	int n = lua_gettop(L);
	const char* msg = NULL;
	if (n >= 1 && !lua_isnil(L, 1)) {
		ui->LAssert(L, lua_isstring(L, 1), "Exit() argument 1: expected string or nil, got %t", 1);
		msg = lua_tostring(L, 1);
	}
	ui->sys->Exit(msg);
	ui->didExit = true;
//	lua_pushstring(L, "dummy");
//	lua_error(L);
	return 0;
}

// ==============================
// Library and API Initialisation
// ==============================

#define ADDFUNC(n) lua_pushcclosure(L, l_##n, 0);lua_setglobal(L, #n);
#define ADDFUNCCL(n, u) lua_pushcclosure(L, l_##n, u);lua_setglobal(L, #n);

int ui_main_c::InitAPI(lua_State* L)
{
	luaL_openlibs(L);

	// Callbacks
	lua_newtable(L);		// Callbacks table
	lua_pushvalue(L, -1);	// Push callbacks table
	ADDFUNCCL(SetCallback, 1);
	lua_pushvalue(L, -1);	// Push callbacks table
	ADDFUNCCL(GetCallback, 1);
	lua_pushvalue(L, -1);	// Push callbacks table
	ADDFUNCCL(SetMainObject, 1);
	lua_setfield(L, LUA_REGISTRYINDEX, "uicallbacks");

	// Image handles
	lua_newtable(L);		// Image handle metatable
	lua_pushvalue(L, -1);	// Push image handle metatable
	ADDFUNCCL(NewImageHandle, 1);
	lua_pushvalue(L, -1);	// Push image handle metatable
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, l_imgHandleGC);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, l_imgHandleLoad);
	lua_setfield(L, -2, "Load");
	lua_pushcfunction(L, l_imgHandleUnload);
	lua_setfield(L, -2, "Unload");
	lua_pushcfunction(L, l_imgHandleIsValid);
	lua_setfield(L, -2, "IsValid");
	lua_pushcfunction(L, l_imgHandleIsLoading);
	lua_setfield(L, -2, "IsLoading");
	lua_pushcfunction(L, l_imgHandleSetLoadingPriority);
	lua_setfield(L, -2, "SetLoadingPriority");
	lua_pushcfunction(L, l_imgHandleImageSize);
	lua_setfield(L, -2, "ImageSize");
	lua_setfield(L, LUA_REGISTRYINDEX, "uiimghandlemeta");

	// Rendering
	ADDFUNC(RenderInit);
	ADDFUNC(GetScreenSize);
	ADDFUNC(SetClearColor);
	ADDFUNC(SetDrawLayer);
	ADDFUNC(GetDrawLayer);
	ADDFUNC(SetViewport);
	ADDFUNC(SetBlendMode);
	ADDFUNC(SetDrawColor);
	ADDFUNC(DrawImage);
	ADDFUNC(DrawImageQuad);
	ADDFUNC(DrawString);
	ADDFUNC(DrawStringWidth);
	ADDFUNC(DrawStringCursorIndex);
	ADDFUNC(StripEscapes);
	ADDFUNC(GetAsyncCount);
	ADDFUNC(RenderInit);

	// Search handles
	lua_newtable(L);	// Search handle metatable
	lua_pushvalue(L, -1);	// Push search handle metatable
	ADDFUNCCL(NewFileSearch, 1);
	lua_pushvalue(L, -1);	// Push search handle metatable
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, l_searchHandleGC);
	lua_setfield(L, -2, "__gc");
	lua_pushcfunction(L, l_searchHandleNextFile);
	lua_setfield(L, -2, "NextFile");
	lua_pushcfunction(L, l_searchHandleGetFileName);
	lua_setfield(L, -2, "GetFileName");
	lua_pushcfunction(L, l_searchHandleGetFileSize);
	lua_setfield(L, -2, "GetFileSize");
	lua_pushcfunction(L, l_searchHandleGetFileModifiedTime);
	lua_setfield(L, -2, "GetFileModifiedTime");
	lua_setfield(L, LUA_REGISTRYINDEX, "uisearchhandlemeta");

	// General function
	ADDFUNC(SetWindowTitle);
	ADDFUNC(GetCursorPos);
	ADDFUNC(SetCursorPos);
	ADDFUNC(ShowCursor);
	ADDFUNC(IsKeyDown);
	ADDFUNC(Copy);
	ADDFUNC(Paste);
	ADDFUNC(Deflate);
	ADDFUNC(Inflate);
	ADDFUNC(GetTime);
	ADDFUNC(GetScriptPath);
	ADDFUNC(GetRuntimePath);
	ADDFUNC(GetUserPath);
	ADDFUNC(MakeDir);
	ADDFUNC(RemoveDir);
	ADDFUNC(SetWorkDir);
	ADDFUNC(GetWorkDir);
	ADDFUNC(LaunchSubScript);
	ADDFUNC(AbortSubScript);
	ADDFUNC(IsSubScriptRunning);
	ADDFUNC(LoadModule);
	ADDFUNC(PLoadModule);
	ADDFUNC(PCall);
	lua_getglobal(L, "string");
	lua_getfield(L, -1, "format");
	ADDFUNCCL(ConPrintf, 1);
	lua_pop(L, 1);		// Pop 'string' table
	ADDFUNC(ConPrintTable);
	ADDFUNC(ConExecute);
	ADDFUNC(ConClear);
	ADDFUNC(print);
	ADDFUNC(SpawnProcess);
	ADDFUNC(OpenURL);
	ADDFUNC(SetProfiling);
	ADDFUNC(Restart);
	ADDFUNC(Exit);
	lua_getglobal(L, "os");
	lua_pushcfunction(L, l_Exit);
	lua_setfield(L, -2, "exit");
	lua_pop(L, 1);		// Pop 'os' table

	return 0;
}

