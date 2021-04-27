// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Global Header
//

// =======
// Classes
// =======

// Font alignment
enum r_fontAlign_e {
	F_LEFT,
	F_CENTRE,
	F_RIGHT,
	F_CENTRE_X,
	F_RIGHT_X
};

// Fonts
enum r_fonts_e {
	F_FIXED,	// Monospaced: Bitsteam Vera Sans Mono
	F_VAR,		// Normal: Liberation Sans
	F_VAR_BOLD,	// Normal: Liberation Sans Bold
	F_NUMFONTS
};

// Texture flags
enum r_texFlag_e {	
	TF_CLAMP	= 0x01,	// Clamp texture
	TF_NOMIPMAP	= 0x02,	// No mipmaps
	TF_NEAREST	= 0x04,	// Use nearest-pixel magnification instead of linear
};

// Blend modes
enum r_blendMode_e {
	RB_ALPHA,
	RB_PRE_ALPHA,
	RB_ADDITIVE
};

// Shader handle
class r_shaderHnd_c {
	friend class r_renderer_c;
public:
	~r_shaderHnd_c();
private:
	r_shaderHnd_c(class r_shader_c* sh);
	r_shader_c* sh;
};

// ==========
// Interfaces
// ==========

// Renderer: r_main.cpp
class r_IRenderer {
public:
	static r_IRenderer* GetHandle(sys_IMain* sysHnd);
	static void FreeHandle(r_IRenderer* hnd);

	virtual void	Init() = 0;
	virtual void	Shutdown() = 0;

	virtual void	BeginFrame() = 0;
	virtual void	EndFrame() = 0;
	
	virtual r_shaderHnd_c* RegisterShader(const char* name, int flags) = 0;
	virtual r_shaderHnd_c* RegisterShaderFromData(int width, int height, int type, byte* dat, int flags) = 0;
	virtual void	GetShaderImageSize(r_shaderHnd_c* hnd, int &width, int &height) = 0;
	virtual void	SetShaderLoadingPriority(r_shaderHnd_c* hnd, int pri) = 0;
	virtual void	PurgeShaders() = 0;
	virtual int		GetTexAsyncCount() = 0;

	virtual void	SetClearColor(const col4_t col) = 0;
	virtual void	SetDrawLayer(int layer, int subLayer = 0) = 0;
	virtual void	SetDrawSubLayer(int subLayer) = 0;
	virtual int		GetDrawLayer() = 0;
	virtual void	SetViewport(int x = 0, int y = 0, int width = 0, int height = 0) = 0;
	virtual void	SetBlendMode(int mode) = 0;
	virtual void	DrawColor(const col4_t col = NULL) = 0;
	virtual void	DrawColor(dword col) = 0;
	virtual void	DrawImage(r_shaderHnd_c* hnd, float x, float y, float w, float h, float s1 = 0.0f, float t1 = 0.0f, float s2 = 1.0f, float t2 = 1.0f) = 0;
	virtual void	DrawImageQuad(r_shaderHnd_c* hnd, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 0, float s2 = 1, float t2 = 1, float s3 = 0, float t3 = 1) = 0;
	virtual void	DrawString(float x, float y, int align, int height, const col4_t col, int font, const char* str) = 0;
	virtual void	DrawStringFormat(float x, float y, int align, int height, const col4_t col, int font, const char* fmt, ...) = 0;
	virtual int		DrawStringWidth(int height, int font, const char* str) = 0;
	virtual int		DrawStringCursorIndex(int height, int font, const char* str, int curX, int curY) = 0;
};
