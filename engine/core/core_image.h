// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Core Image Header
//

// =======
// Classes
// =======

#include <functional>
#include <optional>

#include <gli/texture2d_array.hpp>

// Image types
enum imageType_s {
	IMGTYPE_NONE = 0x00,
	IMGTYPE_GRAY = 0x11,
	IMGTYPE_RGB  = 0x23,
	IMGTYPE_RGBA = 0x34,
	IMGTYPE_RGB_DXT1 = 0x63,
	IMGTYPE_RGBA_BC1 = 0x74,
	IMGTYPE_RGBA_DXT1 = IMGTYPE_RGBA_BC1,
	IMGTYPE_RGBA_BC2 = 0x84,
	IMGTYPE_RGBA_DXT3 = IMGTYPE_RGBA_BC2,
	IMGTYPE_RGBA_BC3 = 0x94,
	IMGTYPE_RGBA_DXT5 = IMGTYPE_RGBA_BC3,
	//IMGTYPE_R_BC4 = 0xA4,
	//IMGTYPE_RG_BC5 = 0xB4,
	//IMGTYPE_RGBF_BC6H = 0xC4,
	IMGTYPE_RGBA_BC7 = 0xD4,
};

constexpr imageType_s g_imageTypeFromComp[]{
	IMGTYPE_NONE,
	IMGTYPE_GRAY,
	IMGTYPE_NONE,
	IMGTYPE_RGB,
	IMGTYPE_RGBA,
};

// Image
class image_c {
public:
	// This `tex` member supersedes the past raw data and metadata fields in order to hold
	// both unblocked and blocked texture formats with array layers and mip levels.
	gli::texture2d_array tex{};

	explicit image_c(IConsole* conHnd = NULL);
	virtual ~image_c() = default;

	IConsole* con;

	using size_callback_t = std::function<void(int, int)>;

	virtual bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {});
	virtual bool Save(const char* fileName);

	bool CopyRaw(int type, dword width, dword height, const byte* dat);
	void Free();

	static image_c* LoaderForFile(IConsole* conHnd, const char* fileName);
};

// Targa Image
class targa_c : public image_c {
public:
	bool rle;
	targa_c(IConsole* conHnd) : image_c(conHnd) { rle = true; }
	bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {}) override;
	bool Save(const char* fileName) override;
};

// JPEG Image
class jpeg_c : public image_c {
public:
	int quality;
	jpeg_c(IConsole* conHnd) : image_c(conHnd) { quality = 80; }
	bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {}) override;
	bool Save(const char* fileName) override;
};

// PNG Image
class png_c : public image_c {
public:
	png_c(IConsole* conHnd) : image_c(conHnd) { }
	bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {}) override;
	bool Save(const char* fileName) override;
};

// JPEG XL Image
class jpeg_xl_c : public image_c {
public:
	jpeg_xl_c(IConsole* conHnd) : image_c(conHnd) {}
	bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {}) override;
	bool Save(const char* fileName) override;
};

// GIF Image
class gif_c : public image_c {
public:
	gif_c(IConsole* conHnd) : image_c(conHnd) { }
	bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {}) override;
	bool Save(const char* fileName) override;
};

// DDS Image
class dds_c : public image_c {
public:
	dds_c(IConsole* conHnd) : image_c(conHnd) {}
	bool Load(const char* fileName, std::optional<size_callback_t> sizeCallback = {}) override;
	bool Save(const char* fileName) override;
};
