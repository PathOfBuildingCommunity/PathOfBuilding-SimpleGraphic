// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Core Image Header
//

// =======
// Classes
// =======

// Image types
enum imageType_s {
	IMGTYPE_NONE = 0x00,
	IMGTYPE_GRAY = 0x11,
	IMGTYPE_RGB  = 0x23,
	IMGTYPE_RGBA = 0x34,
	IMGTYPE_BGR  = 0x43,
	IMGTYPE_BGRA = 0x54,
	IMGTYPE_RGB_DXT1 = 0x63,
	IMGTYPE_RGBA_DXT1 = 0x74,
	IMGTYPE_RGBA_DXT3 = 0x84,
	IMGTYPE_RGBA_DXT5 = 0x94
};

// Image info
struct imageInfo_s {
	dword	width;
	dword	height;
	bool	alpha;
	int		comp;
};

// Image
class image_c {
public:
	byte*	dat = nullptr;
	dword	width = 0;
	dword	height = 0;
	int		comp = 0;
	int		type = 0;

	image_c(IConsole* conHnd = NULL);
	~image_c();

	IConsole* con;

	virtual bool Load(char* fileName);
	virtual bool Save(char* fileName);
	virtual bool ImageInfo(char* fileName, imageInfo_s* info);

	void CopyRaw(int type, dword width, dword height, const byte* dat);
	void Free();

	static image_c* LoaderForFile(IConsole* conHnd, char* fileName);
};

// Targa Image
class targa_c: public image_c {
public:
	bool	rle;
	targa_c(IConsole* conHnd): image_c(conHnd) { rle = true; }
	bool	Load(char* fileName);
	bool	Save(char* fileName);
	bool	ImageInfo(char* fileName, imageInfo_s* info);
};

// JPEG Image
class jpeg_c: public image_c {
public:
	int		quality;
	jpeg_c(IConsole* conHnd): image_c(conHnd) { quality = 80; }
	bool	Load(char* fileName);
	bool	Save(char* fileName);
	bool	ImageInfo(char* fileName, imageInfo_s* info);
};

// TIFF Image
class tiff_c: public image_c {
public:
	tiff_c(IConsole* conHnd): image_c(conHnd) { }
	bool	Load(char* fileName);
	bool	Save(char* fileName);
	bool	ImageInfo(char* fileName, imageInfo_s* info);
};

// PNG Image
class png_c: public image_c {
public:
	png_c(IConsole* conHnd): image_c(conHnd) { }
	bool	Load(char* fileName);
	bool	Save(char* fileName);
	bool	ImageInfo(char* fileName, imageInfo_s* info);
};

// GIF Image
class gif_c: public image_c {
public:
	gif_c(IConsole* conHnd): image_c(conHnd) { }
	bool	Load(char* fileName);
	bool	Save(char* fileName);
	bool	ImageInfo(char* fileName, imageInfo_s* info);
};

// BLP Image
class blp_c: public image_c {
public:
	blp_c(IConsole* conHnd): image_c(conHnd) { }
	bool	Load(char* fileName);
	bool	Save(char* fileName);
	bool	ImageInfo(char* fileName, imageInfo_s* info);
};
