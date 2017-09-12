// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Texture Header
//

// =======
// Classes
// =======

// Texture
class r_tex_c {
public:
	int		error;
	volatile int loading;
	volatile int loadPri;
	dword	texId;
	int		flags;
	char*	fileName;
	dword	fileWidth;
	dword	fileHeight;

	r_tex_c(class r_ITexManager* manager, char* fileName, int flags);
	r_tex_c(class r_ITexManager* manager, image_c* img, int flags);
	~r_tex_c();

	void	Bind();
	void	Unbind();
	void	Enable();
	void	Disable();
	void	StartLoad();
	void	AbortLoad();
	void	ForceLoad();
	void	LoadFile();

	static int GLTypeForImgType(int type);
private:
	class t_manager_c* manager;
	class r_renderer_c* renderer;
	void	Init(class r_ITexManager* manager, char* fileName, int flags);
	void	Upload(image_c *image, int flags);
};

// ==========
// Interfaces
// ==========

// Texture Manager
class r_ITexManager {
public:
	static r_ITexManager* GetHandle(r_renderer_c* renderer);
	static void FreeHandle(r_ITexManager* hnd);

	virtual int		GetAsyncCount() = 0;
	virtual bool	GetImageInfo(char* fileName, imageInfo_s* info) = 0;
};
