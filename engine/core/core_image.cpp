// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Core Image
//

#include "common.h"

#include "core_image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <vector>

#include <jxl/decode.h>
#include <jxl/decode_cxx.h>

// =======
// Classes
// =======

// Generic client data structure, used by JPEG and PNG
struct clientData_s {
	IConsole* con;
	const char* fileName;
	ioStream_c* io;
};

// =========
// Raw Image
// =========

image_c::image_c(IConsole* conHnd)
	: con(conHnd)
{
	dat = NULL;
}

image_c::~image_c()
{
	Free();
}

void image_c::CopyRaw(int inType, dword inWidth, dword inHeight, const byte* inDat)
{
	if (dat) delete[] dat;
	comp = inType & 0xF;
	type = inType;
	width = inWidth;
	height = inHeight;
	dat = new byte[width * height * comp];
	memcpy(dat, inDat, width * height * comp);
}

void image_c::Free()
{
	delete[] dat;
	dat = NULL;
}

bool image_c::Load(const char* fileName) 
{
	return true; // o_O
}

bool image_c::Save(const char* fileName) 
{
	return true; // o_O
}

image_c* image_c::LoaderForFile(IConsole* conHnd, const char* fileName)
{
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		conHnd->Warning("'%s' doesn't exist or cannot be opened", fileName);
		return NULL;
	}
	// Attempt to detect image file type from first 12 bytes of file
	byte dat[12];
	if (in.Read(dat, 12)) {
		conHnd->Warning("'%s': cannot read image file (file is corrupt?)", fileName);
		return NULL;
	}
	if (dat[0] == 0xFF && dat[1] == 0xD8) {
		// JPEG Start Of Image marker
		return new jpeg_c(conHnd);
	} else if (*(dword*)dat == 0x474E5089) {
		// 0x89 P N G
		return new png_c(conHnd);
	} else if (*(dword*)dat == 0x38464947) {
		// G I F 8
		return new gif_c(conHnd);
	} else if (auto sig = JxlSignatureCheck(dat, 12); sig == JXL_SIG_CODESTREAM || sig == JXL_SIG_CONTAINER) {
		// JPEG XL
		return new jpeg_xl_c(conHnd);
	} else if ((dat[1] == 0 && (dat[2] == 2 || dat[2] == 3 || dat[2] == 10 || dat[2] == 11)) || (dat[1] == 1 && (dat[2] == 1 || dat[2] == 9))) {
		// Detect all valid image types (whether supported or not)
		return new targa_c(conHnd);
	}
	conHnd->Warning("'%s': unsupported image file format", fileName);
	return NULL;
}

// ===========
// Targa Image
// ===========

#pragma pack(push,1)
struct tgaHeader_s {
	byte	idLen;
	byte	colorMapType;
	byte	imgType;
	word	colorMapIndex;
	word	colorMapLen;
	byte	colorMapDepth;
	word	xOrigin, yOrigin;
	word	width, height;
	byte	depth;
	byte	descriptor;
};
#pragma pack(pop)

bool targa_c::Load(const char* fileName)
{
	Free();

	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	// Read header
	tgaHeader_s hdr;
	if (in.TRead(hdr)) {
		con->Warning("TGA '%s': couldn't read header", fileName);
		return true;
	}
	if (hdr.colorMapType) {
		con->Warning("TGA '%s': color mapped images not supported", fileName);
		return true;
	}
	in.Seek(hdr.idLen, SEEK_CUR);	

	// Try to match image type
	int ittable[3][3] = {
		 3,  8, IMGTYPE_GRAY,
		 2, 24, IMGTYPE_RGB,
		 2, 32, IMGTYPE_RGBA
	};
	int it_m;
	for (it_m = 0; it_m < 3; it_m++) {
		if (ittable[it_m][0] == (hdr.imgType & 7) && ittable[it_m][1] == hdr.depth) break;
	}
	if (it_m == 3) {
		con->Warning("TGA '%s': unsupported image type (it: %d pd: %d)", fileName, hdr.imgType, hdr.depth);
		return true;
	}

	// Read image
	width = hdr.width;
	height = hdr.height;
	comp = hdr.depth >> 3;
	type = ittable[it_m][2];
	int rowSize = width * comp;
	dat = new byte[height * rowSize];
	bool flipV = !(hdr.descriptor & 0x20);
	if (hdr.imgType & 8) {
		// Decode RLE image
		for (dword row = 0; row < height; row++) {
			int rowBase = (flipV? height - row - 1 : row) * rowSize;
			int x = 0;
			do {
				byte rlehdr;
				in.TRead(rlehdr);
				int rlen = ((rlehdr & 0x7F) + 1) * comp; 
				if (x + rlen > rowSize) {
					con->Warning("TGA '%s': invalid RLE coding (overlong row)", fileName);
					delete[] dat;
					return true;
				}
				if (rlehdr & 0x80) {
					byte rpk[4];
					in.Read(rpk, comp);
					for (int c = 0; c < rlen; c++, x++) dat[rowBase + x] = rpk[c % comp];
				} else {
					in.Read(dat + rowBase + x, rlen);
					x+= rlen;
				}
			} while (x < rowSize);
		}
	} else {
		// Raw image
		if (flipV) {
			for (int row = height - 1; row >= 0; row--) {
				in.Read(dat + row * rowSize, rowSize);
			}
		} else {
			in.Read(dat, height * rowSize);
		}
	}

	// Byteswap BGR(A) to RGB(A)
	if (comp == 3 || comp == 4) {
		uint8_t* p = dat;
		for (size_t i = 0; i < width * height; ++i, p += comp) {
			std::swap(p[0], p[2]);
		}
	}

	return false;
}

bool targa_c::Save(const char* fileName)
{
	if (type != IMGTYPE_RGB && type != IMGTYPE_RGBA) {
		return true;
	}

	// Open file
	fileOutputStream_c out;
	if (out.FileOpen(fileName, true)) {
		return true;
	}

	auto rc = stbi_write_tga_to_func([](void* ctx, void* data, int size) {
		auto out = (fileOutputStream_c*)ctx;
		out->Write(data, size);
		}, &out, width, height, comp, dat);

	return !rc;
}

// ==========
// JPEG Image
// ==========

bool jpeg_c::Load(const char* fileName)
{
	Free();

	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	std::vector<byte> fileData(in.GetLen());
	if (in.Read(fileData.data(), fileData.size())) {
		return true;
	}
	int x, y, in_comp;
	if (!stbi_info_from_memory(fileData.data(), (int)fileData.size(), &x, &y, &in_comp)) {
		return true;
	}
	if (in_comp != 1 && in_comp != 3) {
		con->Warning("JPEG '%s': unsupported component count '%d'", fileName, comp);
		return true;
	}
	stbi_uc* data = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &x, &y, &in_comp, in_comp);
	if (!data) {
		stbi_image_free(data);
		return true;
	}
	width = x;
	height = y;
	comp = in_comp;
	type = in_comp == 1 ? IMGTYPE_GRAY : IMGTYPE_RGB;
	const size_t byteSize = width * height * comp;
	dat = new byte[byteSize];
	std::copy_n(data, byteSize, dat);
	stbi_image_free(data);
	return false;
}

bool jpeg_c::Save(const char* fileName)
{
	// JPEG only supports RGB and grayscale images
	if (type != IMGTYPE_RGB && type != IMGTYPE_GRAY) {
		return true;
	}

	// Open the file
	fileOutputStream_c out;
	if (out.FileOpen(fileName, true)) {
		return true;
	}

	int rc = stbi_write_jpg_to_func([](void* ctx, void* data, int size) {
		auto out = (fileOutputStream_c*)ctx;
		out->Write(data, size);
	}, &out, width, height, comp, dat, quality);
	return !rc;
}

// =========
// PNG Image
// =========

bool png_c::Load(const char* fileName)
{
	Free();

	// Open file and check signature
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	std::vector<byte> fileData(in.GetLen());
	if (in.Read(fileData.data(), fileData.size())) {
		return true;
	}
	int x, y, in_comp;
	if (!stbi_info_from_memory(fileData.data(), (int)fileData.size(), &x, &y, &in_comp)) {
		return true;
	}
	width = x;
	height = y;
	comp = (in_comp == 1 || in_comp == 3) ? 3 : 4;
	type = comp == 3 ? IMGTYPE_RGB : IMGTYPE_RGBA;
	stbi_uc* data = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &x, &y, &in_comp, comp);
	if (!data) {
		stbi_image_free(data);
		return true;
	}
	const size_t byteSize = width * height * comp;
	dat = new byte[byteSize];
	std::copy_n(data, byteSize, dat);
	stbi_image_free(data);
	return false;
}

bool png_c::Save(const char* fileName)
{
	if (type != IMGTYPE_RGB && type != IMGTYPE_RGBA) {
		return true;
	}

	// Open file
	fileOutputStream_c out;
	if (out.FileOpen(fileName, true)) {
		return true;
	}

	auto rc = stbi_write_png_to_func([](void* ctx, void* data, int size) {
		auto out = (fileOutputStream_c*)ctx;
		out->Write(data, size);
	}, &out, width, height, comp, dat, width * comp);
	
	return !rc;
}

// =========
// GIF Image
// =========

bool gif_c::Load(const char* fileName)
{
	// Open file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	{
		std::vector<byte> fileData(in.GetLen());
		if (in.Read(fileData.data(), fileData.size())) {
			return true;
		}
		int x, y, in_comp;
		stbi_uc* data = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &x, &y, &in_comp, 4);
		if (!data || in_comp != 4) {
			stbi_image_free(data);
			return true;
		}
		width = x;
		height = y;
		comp = in_comp;
		type = IMGTYPE_RGBA;
		const size_t byteSize = width * height * comp;
		dat = new byte[byteSize];
		std::copy_n(data, byteSize, dat);
		stbi_image_free(data);
		return false;
	}
}

bool gif_c::Save(const char* fileName)
{
	// HELL no.
	return true;
}

// =========
// JPEG XL Image
// =========

bool jpeg_xl_c::Load(const char* fileName)
{
	// Open file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true))
		return true;

	std::vector<byte> fileData(in.GetLen());
	if (in.Read(fileData.data(), fileData.size()))
		return true;

	auto dec = JxlDecoderMake(nullptr);
	if (JXL_DEC_SUCCESS != JxlDecoderSubscribeEvents(dec.get(), JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE))
		return true;

	if (JXL_DEC_SUCCESS != JxlDecoderSetParallelRunner(dec.get(), nullptr, nullptr))
		return true;

	JxlBasicInfo info{};
	JxlPixelFormat format = { 4, JXL_TYPE_UINT8, JXL_NATIVE_ENDIAN, 0 };

	JxlDecoderSetInput(dec.get(), fileData.data(), fileData.size());
	JxlDecoderCloseInput(dec.get());

	while (true) {
		JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());

		switch (status) {
		case JXL_DEC_ERROR:
		case JXL_DEC_NEED_MORE_INPUT:
			return true;

		case JXL_DEC_BASIC_INFO: {
			if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info))
				return true;
			width = info.xsize;
			height = info.ysize;
			comp = info.num_color_channels + !!info.alpha_bits;
			if (comp == 1)
				type = IMGTYPE_GRAY;
			else if (comp == 3)
				type = IMGTYPE_RGB;
			else if (comp == 4)
				type = IMGTYPE_RGBA;
			else
				return true;

			format.num_channels = comp;
		} break;
		
		case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
			size_t bufferSize{};
			if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec.get(), &format, &bufferSize))
				return true;
			dat = new byte[width * height * comp];
			if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(dec.get(), &format, dat, bufferSize))
				return true;
		} break;

		case JXL_DEC_FULL_IMAGE: {
			// We don't care about animations, consider loading completed when a full image has obtained.
			return false;
		} break;

		case JXL_DEC_SUCCESS:
			return false;
		default:
			continue;
		}
	}
}
bool jpeg_xl_c::Save(const char* fileName)
{
	// Yeah, nah.
	return false;
}
