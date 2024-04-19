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

	virtual bool Load(std::filesystem::path const& fileName);
	virtual bool Save(std::filesystem::path const& fileName);
	virtual bool ImageInfo(std::filesystem::path const& fileName, imageInfo_s* info);

	void CopyRaw(int type, dword width, dword height, const byte* dat);
	void Free();

	static image_c* LoaderForFile(IConsole* conHnd, char const* fileName) = delete;
	static image_c* LoaderForFile(IConsole* conHnd, std::filesystem::path const& fileName);

private:
	// Force compile error on narrow strings to favour `std::filesystem::path`.
	// These are unfortunately necessary as the path constructor is eager to
	// interpret narrow strings as the ACP codepage. Prefer using
	// `std::filesystem::u8path` (C++17) or `std::u8string` (since C++20) in
	// the calls to these functions.
	virtual bool Load(char const* fileName) { return true; }
	virtual bool Load(std::string const& fileName) { return true; }
	virtual bool Load(std::string_view fileName) { return true; }
	virtual bool Save(char const* fileName) { return true; }
	virtual bool Save(std::string const& fileName) { return true; }
	virtual bool Save(std::string_view fileName) { return true; }
	virtual bool ImageInfo(char const* fileName, imageInfo_s* info) { return true; }
	virtual bool ImageInfo(std::string const& fileName, imageInfo_s* info) { return true; }
	virtual bool ImageInfo(std::string_view fileName, imageInfo_s* info) { return true; }
};

// Targa Image
class targa_c: public image_c {
public:
	bool	rle;
	targa_c(IConsole* conHnd): image_c(conHnd) { rle = true; }
	bool	Load(std::filesystem::path const& fileName) override;
	bool	Save(std::filesystem::path const& fileName) override;
	bool	ImageInfo(std::filesystem::path const& fileName, imageInfo_s* info) override;
};

// JPEG Image
class jpeg_c: public image_c {
public:
	int		quality;
	jpeg_c(IConsole* conHnd): image_c(conHnd) { quality = 80; }
	bool	Load(std::filesystem::path const& fileName) override;
	bool	Save(std::filesystem::path const& fileName) override;
	bool	ImageInfo(std::filesystem::path const& fileName, imageInfo_s* info) override;
};

// PNG Image
class png_c: public image_c {
public:
	png_c(IConsole* conHnd): image_c(conHnd) { }
	bool	Load(std::filesystem::path const& fileName) override;
	bool	Save(std::filesystem::path const& fileName) override;
	bool	ImageInfo(std::filesystem::path const& fileName, imageInfo_s* info) override;
};
