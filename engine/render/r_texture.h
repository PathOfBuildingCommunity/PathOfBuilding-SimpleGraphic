// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Texture Header
//

// =======
// Classes
// =======

#include <atomic>
#include <string>

// Texture
class r_tex_c {
public:
	int		error;
	enum Status
	{
		INIT,
		IN_QUEUE,
		PROCESSING,
		DONE,
	};
	std::atomic<Status> status;
	std::atomic<int> loadPri;
	dword	texId;
	int		flags;
	std::string fileName;
	dword	fileWidth;
	dword	fileHeight;

	r_tex_c(class r_ITexManager* manager, std::string_view fileName, int flags);
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
	void	Init(class r_ITexManager* manager, std::string_view fileName, int flags);
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
	virtual bool	GetImageInfo(std::string_view fileName, imageInfo_s* info) = 0;
};
