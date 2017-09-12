// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Main Header
//

// =============
// Configuration
// =============

#define R_MAXSHADERS 65536

// =======
// Classes
// =======

// Render viewport
struct r_viewport_s {
	int	x;
	int	y;
	int	width;
	int height;
};

// Render layer
class r_layer_c {
public:
	int		numCmd;
	int		cmdSize;
	struct r_layerCmd_s** cmdList;

	int		layer;
	int		subLayer;

	r_layer_c(r_renderer_c* renderer, int i_layer, int i_subLayer);
	~r_layer_c();

	void	SetViewport(r_viewport_s* viewport);
	void	Bind(r_tex_c* tex);
	void	Color(col4_t col);
	void	Quad(double s0, double t0, double x0, double y0, double s1, double t1, double x1, double y1, double s2, double t2, double x2, double y2, double s3, double t3, double x3, double y3);
	void	Render();

private:
	r_renderer_c* renderer;

	struct r_layerCmd_s* NewCommand();
};

// Renderer Main Class
class r_renderer_c: public r_IRenderer, public conCmdHandler_c {
public:
	// Interface
	void	Init();
	void	Shutdown();

	void	BeginFrame();
	void	EndFrame();
	
	r_shaderHnd_c* RegisterShader(char* shname, int flags);
	r_shaderHnd_c* RegisterShaderFromData(int width, int height, int type, byte* dat, int flags);
	void	GetShaderImageSize(r_shaderHnd_c* hnd, int &width, int &height);
	void	SetShaderLoadingPriority(r_shaderHnd_c* hnd, int pri);
	void	PurgeShaders();
	int		GetTexAsyncCount();

	void	SetClearColor(const col4_t col);
	void	SetDrawLayer(int layer, int subLayer = 0);
	void	SetDrawSubLayer(int subLayer);
	void	SetViewport(int x = 0, int y = 0, int width = 0, int height = 0);
	void	DrawColor(const col4_t col = NULL);
	void	DrawColor(dword col);
	void	DrawImage(r_shaderHnd_c* hnd, float x, float y, float w, float h, float s1 = 0.0f, float t1 = 0.0f, float s2 = 1.0f, float t2 = 1.0f);
	void	DrawImageQuad(r_shaderHnd_c* hnd, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float s0 = 0, float t0 = 0, float s1 = 1, float t1 = 0, float s2 = 1, float t2 = 1, float s3 = 0, float t3 = 1);
	void	DrawString(float x, float y, int align, int height, const col4_t col, int font, const char* str);
	void	DrawStringFormat(float x, float y, int align, int height, const col4_t col, int font, const char* fmt, ...);
	int		DrawStringWidth(int height, int font, const char* str);
	int		DrawStringCursorIndex(int height, int font, const char* str, int curX, int curY);

	// Encapsulated
	r_renderer_c(sys_IMain* sysHnd);

	sys_IMain* sys;

	sys_IOpenGL* openGL;

	r_ITexManager* texMan;	// Texture manager interface

	const char*	st_vendor;	// Vendor string
	const char*	st_renderer;// Renderer string
	const char*	st_ver;		// Version string
	const char*	st_ext;		// Exntension string

	bool	texNonPOT;		// Non power-of-2 textures supported?
	dword	texMaxDim;		// Maximum texture dimension

	PFNGLCOMPRESSEDTEXIMAGE2DARBPROC	glCompressedTexImage2DARB;
	
	conVar_c*	r_compress;
	conVar_c*	r_screenshotFormat;
	conVar_c*	r_layerDebug;

	r_shaderHnd_c* whiteImage;	// White image

	r_font_c* fonts[F_NUMFONTS]; // Font objects

	col4_t	drawColor;		// Current draw color

	r_viewport_s curViewport; // Current viewport

	int		numShader;
	class r_shader_c* shaderList[R_MAXSHADERS];

	int		numLayer;
	int		layerListSize;
	r_layer_c** layerList;
	r_layer_c* curLayer;

	int		layerCmdBinCount;
	int		layerCmdBinSize;
	struct r_layerCmd_s** layerCmdBin;

	int		takeScreenshot;
	void	DoScreenshot(image_c* i, char* ext);

	void	C_Screenshot(IConsole* conHnd, args_c &args);
};
