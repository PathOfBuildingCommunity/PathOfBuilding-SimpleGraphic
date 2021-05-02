// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Main
//

#include "r_local.h"

#include <fmt/chrono.h>

// =======
// Classes
// =======

enum r_takeScreenshot_e {
	R_SSNONE,
	R_SSTGA,
	R_SSJPEG,
	R_SSPNG
};

// ============
// Shader Class
// ============

class r_shader_c {
public:
	r_renderer_c* renderer;
	char*		name;
	dword		nameHash;
	int			refCount;
	r_tex_c*	tex;

	r_shader_c(r_renderer_c* renderer, const char* shname, int flags);
	r_shader_c(r_renderer_c* renderer, const char* shname, int flags, int width, int height, int type, byte* dat);
	~r_shader_c();
};

r_shader_c::r_shader_c(r_renderer_c* renderer, const char* shname, int flags)
	: renderer(renderer)
{
	name = AllocString(shname);
	nameHash = StringHash(shname, 0xFFFF);
	refCount = 0;
	tex = new r_tex_c(renderer->texMan, name, flags);
	if (tex->error) {
		renderer->sys->con->Warning("couldn't load texture '%s'", name);
	}
}

r_shader_c::r_shader_c(r_renderer_c* renderer, const char* shname, int flags, int width, int height, int type, byte* dat)
	: renderer(renderer)
{
	name = AllocString(shname);
	nameHash = StringHash(shname, 0xFFFF);
	refCount = 0;
	image_c img;
	img.width = width;
	img.height = height;
	img.type = type;
	img.comp = type & 0xF;
	img.dat = dat;
	tex = new r_tex_c(renderer->texMan, &img, flags);
	img.dat = NULL;
}

r_shader_c::~r_shader_c()
{
	FreeString(name);
	delete tex;
}

// ===================
// Shader Handle Class
// ===================

r_shaderHnd_c::r_shaderHnd_c(r_shader_c* sh)
	: sh(sh)
{
	sh->refCount++;
}

r_shaderHnd_c::~r_shaderHnd_c()
{
	sh->refCount--;
	if (sh->refCount == 0) {
		sh->tex->AbortLoad();
	}
}

// =================
// Layer queue class
// =================

struct r_layerCmd_s {
	enum {
		VIEWPORT,
		BLEND,
		BIND,
		COLOR,
		QUAD,
	} cmd;
	union {
		r_viewport_s viewport;
		int blendMode;
		r_tex_c* tex;
		col4_t col;
		struct {
			double s[4];
			double t[4];
			double x[4];
			double y[4];
		} quad;
	};
};

r_layer_c::r_layer_c(r_renderer_c* renderer, int layer, int subLayer)
	: renderer(renderer), layer(layer), subLayer(subLayer)
{
	numCmd = 0;
	cmdSize = 8;
	cmdList = new r_layerCmd_s*[cmdSize];
}

r_layer_c::~r_layer_c()
{
	delete cmdList;
}

r_layerCmd_s* r_layer_c::NewCommand()
{
	r_layerCmd_s* cmd;
	if (renderer->layerCmdBinCount) {
		cmd = renderer->layerCmdBin[--renderer->layerCmdBinCount];
	} else {
		cmd = new r_layerCmd_s;
	}
	if (numCmd == cmdSize) {
		cmdSize<<= 1;
		trealloc(cmdList, cmdSize);
	}
	cmdList[numCmd++] = cmd;
	return cmd;
}

void r_layer_c::SetViewport(r_viewport_s* viewport)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::VIEWPORT;
	cmd->viewport.x = viewport->x;
	cmd->viewport.y = viewport->y;
	cmd->viewport.width = viewport->width;
	cmd->viewport.height = viewport->height;
}

void r_layer_c::SetBlendMode(int mode)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::BLEND;
	cmd->blendMode = mode;
}

void r_layer_c::Bind(r_tex_c* tex)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::BIND;
	cmd->tex = tex;
}

void r_layer_c::Color(col4_t col)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::COLOR;
	Vector4Copy(col, cmd->col);
}

void r_layer_c::Quad(double s0, double t0, double x0, double y0, double s1, double t1, double x1, double y1, double s2, double t2, double x2, double y2, double s3, double t3, double x3, double y3)
{
	r_layerCmd_s* cmd = NewCommand();
	cmd->cmd = r_layerCmd_s::QUAD;
	cmd->quad.s[0] = s0; cmd->quad.s[1] = s1; cmd->quad.s[2] = s2; cmd->quad.s[3] = s3;
	cmd->quad.t[0] = t0; cmd->quad.t[1] = t1; cmd->quad.t[2] = t2; cmd->quad.t[3] = t3;
	cmd->quad.x[0] = x0; cmd->quad.x[1] = x1; cmd->quad.x[2] = x2; cmd->quad.x[3] = x3;
	cmd->quad.y[0] = y0; cmd->quad.y[1] = y1; cmd->quad.y[2] = y2; cmd->quad.y[3] = y3;
}

void r_layer_c::Render()
{
	r_viewport_s curViewPort = {-1, -1, -1, -1};
	int curBlendMode = -1;
	r_tex_c* curTex = NULL;
	for (int i = 0; i < numCmd; i++) {
		r_layerCmd_s* cmd = cmdList[i];
		switch (cmd->cmd) {
		case r_layerCmd_s::VIEWPORT:
			if (cmd->viewport.x != curViewPort.x || cmd->viewport.y != curViewPort.y || cmd->viewport.width != curViewPort.width || cmd->viewport.height != curViewPort.height) {
				auto& vid = renderer->sys->video->vid;
				float fbScaleX = vid.fbSize[0] / (float)vid.size[0];
				float fbScaleY = vid.fbSize[1] / (float)vid.size[1];
				float x = cmd->viewport.x * fbScaleX;
				float y = (vid.size[1] - cmd->viewport.y - cmd->viewport.height) * fbScaleY;
				float width = cmd->viewport.width * fbScaleX;
				float height = cmd->viewport.height * fbScaleY;
				glViewport((int)x, (int)y, (int)width, (int)height);
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				glOrtho(0, (float)cmd->viewport.width, (float)cmd->viewport.height, 0, -9999, 9999);
				glMatrixMode(GL_MODELVIEW);
				glLoadIdentity();
			}
			break;
		case r_layerCmd_s::BLEND:
			if (cmd->blendMode != curBlendMode) {
				switch (cmd->blendMode) {
				case RB_ALPHA:
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case RB_PRE_ALPHA:
					glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
					break;
				case RB_ADDITIVE:
					glBlendFunc(GL_ONE, GL_ONE);
					break;
				}
			}
			break;
		case r_layerCmd_s::BIND:
			if (cmd->tex != curTex) {
				cmd->tex->Bind();
				curTex = cmd->tex;
			}
			break;
		case r_layerCmd_s::QUAD:
			glBegin(GL_TRIANGLE_FAN);
			for (int v = 0; v < 4; v++) {
				glTexCoord2d(cmd->quad.s[v], cmd->quad.t[v]);
				glVertex2d(cmd->quad.x[v], cmd->quad.y[v]);
			}
			glEnd();
			break;
		case r_layerCmd_s::COLOR:
			glColor4fv(cmd->col);
			break;
		}
		if (renderer->layerCmdBinCount == renderer->layerCmdBinSize) {
			renderer->layerCmdBinSize<<= 1;
			trealloc(renderer->layerCmdBin, renderer->layerCmdBinSize);
		}
		renderer->layerCmdBin[renderer->layerCmdBinCount++] = cmd;
	}
	numCmd = 0;
}

// =====================
// r_IRenderer Interface
// =====================

r_IRenderer* r_IRenderer::GetHandle(sys_IMain* sysHnd)
{
	return new r_renderer_c(sysHnd);
}

void r_IRenderer::FreeHandle(r_IRenderer* hnd)
{
	delete (r_renderer_c*)hnd;
}

r_renderer_c::r_renderer_c(sys_IMain* sysHnd)
	: conCmdHandler_c(sysHnd->con), sys(sysHnd)
{
	r_compress = sys->con->Cvar_Add("r_compress", CV_ARCHIVE, "0");
	r_screenshotFormat = sys->con->Cvar_Add("r_screenshotFormat", CV_ARCHIVE, "jpg");
	r_layerDebug = sys->con->Cvar_Add("r_layerDebug", CV_ARCHIVE, "0");

	Cmd_Add("screenshot", 0, "[<format>]", this, &r_renderer_c::C_Screenshot);
}

// =============
// Init/Shutdown
// =============

void r_renderer_c::Init()
{
	sys->con->PrintFunc("Render Init");

	timer_c timer;
	timer.Start();

	// Initialise OpenGL
	openGL = sys_IOpenGL::GetHandle(sys);
	sys_glSet_s set;
	set.bColor = 32;
	set.bDepth = 24;
	set.bStencil = 0;
	set.vsync = true;
	if (openGL->Init(&set)) {
		sys->Error("OpenGL initialisation failed");
	}

	// Get strings
	st_vendor = (const char*)glGetString(GL_VENDOR);
	st_renderer = (const char*)glGetString(GL_RENDERER);
	st_ver = (const char*)glGetString(GL_VERSION);
	st_ext = (const char*)glGetString(GL_EXTENSIONS);

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (int*)&texMaxDim);
	sys->con->Printf("GL_MAX_TEXTURE_SIZE: %d\n", texMaxDim);

	// Set default state
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);

	// Load extensions
	sys->con->Printf("Loading OpenGL extensions...\n");

	if (strstr(st_ext, "GL_EXT_texture_compression_s3tc")) {
		sys->con->Printf("using GL_EXT_texture_compression_s3tc\n");
		glCompressedTexImage2DARB = (PFNGLCOMPRESSEDTEXIMAGE2DARBPROC)openGL->GetProc("glCompressedTexImage2DARB");
	} else {
		sys->con->Printf("GL_EXT_texture_compression_s3tc not supported\n");
		glCompressedTexImage2DARB = NULL;
	}

	if (strstr(st_ext, "GL_ARB_texture_non_power_of_two")) {
		sys->con->Printf("using GL_ARB_texture_non_power_of_two\n");
		texNonPOT = true;
	} else {
		sys->con->Printf("GL_ARB_texture_non_power_of_two not supported\n");
		texNonPOT = false;
	}

	// Initialise texture manager
	texMan = r_ITexManager::GetHandle(this);

	// Initialise shader array
	numShader = 0;
	memset(shaderList, 0, sizeof(shaderList));

	// Initialise layer array
	numLayer = 1;
	layerListSize = 16;
	layerList = new r_layer_c*[layerListSize];
	layerList[0] = new r_layer_c(this, 0, 0);

	// Initialise layer command bin
	layerCmdBinCount = 0;
	layerCmdBinSize = 1024;
	layerCmdBin = new r_layerCmd_s*[layerCmdBinSize];

	takeScreenshot = R_SSNONE;

	// Load render resources
	sys->con->Printf("Loading resources...\n");

	whiteImage = RegisterShader("@white", 0);

	fonts[F_FIXED] = new r_font_c(this, "Bitstream Vera Sans Mono");
	fonts[F_VAR] = new r_font_c(this, "Liberation Sans");
	fonts[F_VAR_BOLD] = new r_font_c(this, "Liberation Sans Bold");

	sys->con->Printf("Renderer initialised in %d msec.\n", timer.Get());
}

void r_renderer_c::Shutdown()
{
	sys->con->PrintFunc("Render Shutdown");

	sys->con->Printf("Unloading resources...\n");

	delete whiteImage;

	for (int f = 0; f < F_NUMFONTS; f++) {
		delete fonts[f];
	}

	for (int s = 0; s < numShader; s++) {
		delete shaderList[s];
	}

	for (int l = 0; l < numLayer; l++) {
		delete layerList[l];
	}
	delete layerList;
	for (int c = 0; c < layerCmdBinCount; c++) {
		delete layerCmdBin[c];
	}
	delete layerCmdBin;

	// Shutdown texture manager
	r_ITexManager::FreeHandle(texMan);

	// Shutdown OpenGL
	openGL->Shutdown();
	sys_IOpenGL::FreeHandle(openGL);

	sys->con->Printf("Renderer shutdown complete.\n");
}

// =================
// Render Management
// =================

void r_renderer_c::BeginFrame()
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	curLayer = layerList[0];

	SetViewport();
	SetBlendMode(RB_ALPHA);
	DrawColor();
}

static int layerCompFunc(const void* va, const void* vb)
{
	r_layer_c* a = *(r_layer_c**)va;
	r_layer_c* b = *(r_layer_c**)vb;
	if (a->layer < b->layer) {
		return -1;
	} else if (a->layer > b->layer) {
		return 1;
	} else if (a->subLayer < b->subLayer) {
		return -1;
	} else {
		return 1;
	}
}

void r_renderer_c::EndFrame()
{
	r_layer_c** layerSort = new r_layer_c*[numLayer];
	for (int l = 0; l < numLayer; l++) {
		layerSort[l] = layerList[l];
	}
	qsort(layerSort, numLayer, sizeof(r_layer_c*), layerCompFunc);
	if (r_layerDebug->intVal) {
		int totalCmd = 0;
		for (int l = 0; l < numLayer; l++) {
			totalCmd+= layerSort[l]->numCmd;
			char str[1024];
			sprintf(str, "%d (%4d,%4d) [%2d]", layerSort[l]->numCmd, layerSort[l]->layer, layerSort[l]->subLayer, l);
			float w = (float)DrawStringWidth(16, F_FIXED, str);
			DrawColor(0x7F000000);
			DrawImage(NULL, (float)sys->video->vid.size[0] - w, sys->video->vid.size[1] - (l + 2)*16.0f, w, 16);
			DrawStringFormat(0, sys->video->vid.size[1] - (l + 2)*16.0f, F_RIGHT, 16, colorWhite, F_FIXED, str);
		}
		char str[1024];
		sprintf(str, "%d", totalCmd);
		float w = (float)DrawStringWidth(16, F_FIXED, str);
		DrawColor(0xAF000000);
		DrawImage(NULL, (float)sys->video->vid.size[0] - w, sys->video->vid.size[1] - 16.0f, w, 16);
		DrawStringFormat(0, sys->video->vid.size[1] - 16.0f, F_RIGHT, 16, colorWhite, F_FIXED, str);
	}
	for (int l = 0; l < numLayer; l++) {
		layerSort[l]->Render();
	}
	delete[] layerSort;

	glFlush();

	// Swap output buffers
	openGL->Swap();

	// Take screenshot
	switch (takeScreenshot) {
	case R_SSTGA:
		{
			targa_c i(sys->con);
			i.type = IMGTYPE_BGR;
			DoScreenshot(&i, "tga");
		}
		break;
	case R_SSJPEG:
		{
			jpeg_c i(sys->con);
			i.type = IMGTYPE_RGB;
			DoScreenshot(&i, "jpg");
		}
		break;
	case R_SSPNG:
		{
			png_c i(sys->con);
			i.type = IMGTYPE_RGB;
			DoScreenshot(&i, "png");
		}
		break;
	}
	takeScreenshot = R_SSNONE;

	PurgeShaders();
}

// =================
// Shader Management
// =================

void r_renderer_c::PurgeShaders()
{
	// Delete released shaders
	for (int s = 0; s < numShader; s++) {
		if (shaderList[s] && shaderList[s]->refCount == 0 && shaderList[s]->tex->status == r_tex_c::DONE) {
			delete shaderList[s];
			shaderList[s] = NULL;
		}
	}
}

r_shaderHnd_c* r_renderer_c::RegisterShader(const char* shname, int flags)
{
	if (*shname == 0) {
		return NULL;
	}

	dword nameHash = StringHash(shname, 0xFFFF);
	int newId = -1;
	for (int s = 0; s < numShader; s++) {
		if ( !shaderList[s] ) {
			newId = s;
		} else if (shaderList[s]->nameHash == nameHash && _stricmp(shname, shaderList[s]->name) == 0 && shaderList[s]->tex->flags == flags) {
			// Shader already exists, return a new handle for it
			// Ensure texture is loaded as soon as possible
			shaderList[s]->tex->ForceLoad();
			return new r_shaderHnd_c(shaderList[s]);
		}
	}
	if (newId == -1) {
		if (numShader == R_MAXSHADERS) {
			sys->con->Warning("shader limit reached");
			return NULL;
		}
		newId = numShader++;
	}
	shaderList[newId] = new r_shader_c(this, shname, flags);
	return new r_shaderHnd_c(shaderList[newId]);
}

r_shaderHnd_c* r_renderer_c::RegisterShaderFromData(int width, int height, int type, byte* dat, int flags)
{
	int newId = -1;
	for (int s = 0; s < numShader; s++) {
		if ( !shaderList[s] ) {
			newId = s;
			break;
		}
	}
	if (newId == -1) {
		if (numShader == R_MAXSHADERS) {
			sys->con->Warning("shader limit reached");
			return NULL;
		}
		newId = numShader++;
	}
	char shname[32];
	sprintf(shname, "data:%d", newId);
	shaderList[newId] = new r_shader_c(this, shname, flags, width, height, type, dat);
	return new r_shaderHnd_c(shaderList[newId]);
}

void r_renderer_c::GetShaderImageSize(r_shaderHnd_c* hnd, int &width, int &height)
{
	if (hnd && hnd->sh->tex->status == r_tex_c::DONE) {
		width = hnd->sh->tex->fileWidth;
		height = hnd->sh->tex->fileHeight;
	} else {
		width = 0;
		height = 0;
	}
}

void r_renderer_c::SetShaderLoadingPriority(r_shaderHnd_c* hnd, int pri)
{
	if (hnd && hnd->sh->tex->status != r_tex_c::DONE) {
		hnd->sh->tex->loadPri = pri;
	}
}

int r_renderer_c::GetTexAsyncCount()
{
	return texMan->GetAsyncCount();
}

// ==========
// 2D Drawing
// ==========

void r_renderer_c::SetClearColor(const col4_t col)
{
	glClearColor(col[0], col[1], col[2], col[3]);
}

void r_renderer_c::SetDrawLayer(int layer, int subLayer)
{
	if (layer == curLayer->layer && subLayer == curLayer->subLayer) {
		return;
	}
	r_layer_c* newCurLayer = NULL;
	for (int l = 0; l < numLayer; l++) {
		if (layerList[l]->layer == layer && layerList[l]->subLayer == subLayer) {
			newCurLayer = layerList[l];
			break;
		}
	}
	if ( !newCurLayer ) {
		if (numLayer == layerListSize) {
			layerListSize<<= 1;
			trealloc(layerList, layerListSize); 
		}
		layerList[numLayer] = new r_layer_c(this, layer, subLayer);
		newCurLayer = layerList[numLayer];
		numLayer++;
	}
	curLayer = newCurLayer;
	curLayer->SetViewport(&curViewport);
	curLayer->SetBlendMode(curBlendMode);
}

void r_renderer_c::SetDrawSubLayer(int subLayer)
{
	SetDrawLayer(curLayer->layer, subLayer);
}

int r_renderer_c::GetDrawLayer()
{
	return curLayer->subLayer;
}

void r_renderer_c::SetViewport(int x, int y, int width, int height)
{
	if (height == 0) {
		width = sys->video->vid.size[0];
		height = sys->video->vid.size[1];
	}
	curViewport.x = x;
	curViewport.y = y;
	curViewport.width = width;
	curViewport.height = height;
	curLayer->SetViewport(&curViewport);
}

void r_renderer_c::SetBlendMode(int mode)
{
	curBlendMode = mode;
	curLayer->SetBlendMode(mode);
}

void r_renderer_c::DrawColor(const col4_t col)
{
	if (col) {
		Vector4Copy(col, drawColor);
	} else {
		drawColor[0] = 1.0f;
		drawColor[1] = 1.0f;
		drawColor[2] = 1.0f;
		drawColor[3] = 1.0f;
	}
}

void r_renderer_c::DrawColor(dword col)
{
	drawColor[0] = ((col >> 16) & 0xFF) / 255.0f;
	drawColor[1] = ((col >> 8) & 0xFF) / 255.0f;
	drawColor[2] = (col & 0xFF) / 255.0f;
	drawColor[3] = (col >> 24) / 255.0f;
}

void r_renderer_c::DrawImage(r_shaderHnd_c* hnd, float x, float y, float w, float h, float s1, float t1, float s2, float t2)
{
	DrawImageQuad(hnd, x, y, x + w, y, x + w, y + h, x, y + h, s1, t1, s2, t1, s2, t2, s1, t2);
}

void r_renderer_c::DrawImageQuad(r_shaderHnd_c* hnd, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float s0, float t0, float s1, float t1, float s2, float t2, float s3, float t3)
{
	if (hnd) {
		curLayer->Bind(hnd->sh->tex);
	} else {
		curLayer->Bind(whiteImage->sh->tex);
	}
	curLayer->Color(drawColor);
	curLayer->Quad(s0, t0, x0, y0, s1, t1, x1, y1, s2, t2, x2, y2, s3, t3, x3, y3);
}

void r_renderer_c::DrawString(float x, float y, int align, int height, const col4_t col, int font, const char* str)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}

	scp_t pos = {x, y};
	if (col) {
		col4_t tcol;
		Vector4Copy(col, tcol);
		fonts[font]->Draw(pos, align, height, tcol, str);
	} else {
		fonts[font]->Draw(pos, align, height, drawColor, str);
	}
}

void r_renderer_c::DrawStringFormat(float x, float y, int align, int height, const col4_t col, int font, const char* fmt, ...)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}

	va_list va;
	va_start(va, fmt);

	scp_t pos = {x, y};
	if (col) {
		col4_t tcol;
		Vector4Copy(col, tcol);
		fonts[font]->VDraw(pos, align, height, tcol, fmt, va);
	} else {
		fonts[font]->VDraw(pos, align, height, drawColor, fmt, va);
	}

	va_end(va);
}

int	r_renderer_c::DrawStringWidth(int height, int font, const char* str)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}
	return fonts[font]->StringWidth(height, str);
}

int r_renderer_c::DrawStringCursorIndex(int height, int font, const char* str, int curX, int curY)
{
	if (font < 0 || font >= F_NUMFONTS) {
		font = F_FIXED;
	}
	return fonts[font]->StringCursorIndex(height, str, curX, curY);
}

// ===========
// Screenshots
// ===========

void r_renderer_c::C_Screenshot(IConsole* conHnd, args_c &args)
{
	const char* fmtName = args.argc >= 2? args.argv[1] : r_screenshotFormat->strVal.c_str();
	takeScreenshot = R_SSNONE;
	if ( !_stricmp(fmtName, "tga") ) {
		takeScreenshot = R_SSTGA;
	} else if ( !_stricmp(fmtName, "jpg") || !_stricmp(fmtName, "jpeg") ) {
		takeScreenshot = R_SSJPEG;
	} else if ( !_stricmp(fmtName, "png") ) {
		takeScreenshot = R_SSPNG;
	} else {
		conHnd->Warning("Unknown screenshot format '%s', valid formats: jpg, tga, png", fmtName);
	}
}

void r_renderer_c::DoScreenshot(image_c* i, const char* ext)
{
	int xs = sys->video->vid.size[0];
	int ys = sys->video->vid.size[1];

	int size = xs * ys * 3;
	byte* sbuf = new byte[size];

	// Read the front buffer
	glReadBuffer(GL_FRONT);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, xs, ys, r_tex_c::GLTypeForImgType(i->type), GL_UNSIGNED_BYTE, sbuf);

	// Flip the image
	int	span = xs * 3;
	byte* ss = new byte[size];
	byte* p1 = sbuf; 
	byte* p2 = ss + size - span;
	for (int y = 0; y < ys; y++, p1+= span, p2-= span) {
		memcpy(p2, p1, span);
	}
	delete[] sbuf;

	// Set image info
	i->dat = ss;
	i->width = xs;
	i->height = ys;
	i->comp = 3;

	time_t curTime;
	time(&curTime);
	std::string ssname = fmt::format(CFG_DATAPATH "Screenshots/{:%m%d%y_%H%M%S}.{}",
	fmt::localtime(curTime), ext);
		// curTimeSt.tm_mon+1, curTimeSt.tm_mday, curTimeSt.tm_year%100,
		// curTimeSt.tm_hour, curTimeSt.tm_min, curTimeSt.tm_sec, ext);

	// Save image
	if (i->Save(ssname.c_str())) {
		sys->con->Print("Couldn't write screenshot!\n");
	} else {
		sys->con->Print(fmt::format("Wrote screenshot to {}\n", ssname).c_str());
	}
}
