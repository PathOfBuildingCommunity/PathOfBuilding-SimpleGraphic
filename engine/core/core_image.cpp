// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Core Image
//

#include "common.h"

#include "core_image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <vector>

// =======
// Classes
// =======

#define BLP2_MAGIC	0x32504C42	// "BLP2"

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
	if (dat) delete dat;
	comp = inType & 0xF;
	type = inType;
	width = inWidth;
	height = inHeight;
	dat = new byte[width * height * comp];
	memcpy(dat, inDat, width * height * comp);
}

void image_c::Free()
{
	delete dat;
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

bool image_c::ImageInfo(const char* fileName, imageInfo_s* info)
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
	// Attempt to detect image file type from first 4 bytes of file
	byte dat[4];
	if (in.Read(dat, 4)) {
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
	} else if (*(dword*)dat == BLP2_MAGIC) {
		// B L P 2
		return new blp_c(conHnd);
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
		 2, 24, IMGTYPE_BGR,
		 2, 32, IMGTYPE_BGRA
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
					delete dat;
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

	return false;
}

bool targa_c::Save(const char* fileName)
{
	// Find a suitable image type
	int ittable[3][3] = {
		 1, IMGTYPE_GRAY, 3,
		 3, IMGTYPE_BGR, 2,
		 4, IMGTYPE_BGRA, 2
	};
	int it_m;
	for (it_m = 0; it_m < 3; it_m++) {
		if (ittable[it_m][0] == comp && ittable[it_m][1] == type) break;
	}
	if (it_m == 3) {
		// Image type not supported
		return true;
	}

	// Open the file
	fileOutputStream_c out;
	if (out.FileOpen(fileName, true)) {
		return true;
	}

	// Write header
	tgaHeader_s hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.width = width;
	hdr.height = height;
	hdr.depth = comp << 3;
	hdr.imgType = ittable[it_m][2] + rle * 8;
	out.TWrite(hdr);

	// Write image
	dword rowSize = width * comp;
	if (rle) {
		byte* packet = new byte[comp * 128 + 4];
		dword mask = 0xFFFFFFFF >> ((4 - comp) << 3);
		for (int y = height - 1; y >= 0; y--) {
			byte* p = dat + y * rowSize;
			byte hdr = 255;
			dword lastPix;
			memOutputStream_c line(512);
			for (dword x = 0; x < width; x++, p+= comp) {
				dword pix = *(dword*)p & mask;
				if (hdr == 255) {
					// Start new packet
					*(dword*)packet = pix;
					hdr = 0;
				} else if (hdr & 0x80) {
					// Run-length packet, check for continuance
					if (pix == lastPix) {
						hdr++;
						if (hdr == 255) {
							// Max length, write it
							line.TWrite(hdr);
							line.Write(packet, comp);
						}
					} else {
						line.TWrite(hdr);
						line.Write(packet, comp);
						*(dword*)packet = pix;
						hdr = 0;
					}
				} else if (hdr) {
					// Raw packet, check if a run-length packet could be created with the last pixel
					if (pix == lastPix) {
						hdr--;
						line.TWrite(hdr);
						line.Write(packet, comp * (hdr + 1));
						*(dword*)packet = pix;
						hdr = 129;
					} else if (hdr == 127) {
						// Packet is already full, write it
						line.TWrite(hdr);
						line.Write(packet, comp * (hdr + 1));
						*(dword*)packet = pix;
						hdr = 0;
					} else {
						hdr++;
						*(dword*)(packet + comp * hdr) = pix;
					}
				} else {
					// New packet, check if this could become a run-length packet
					if (pix == lastPix) {
						hdr = 129;
					} else {
						hdr = 1;
						*(dword*)(packet + comp) = pix;
					}
				}
				lastPix = pix;
			}
			if (hdr < 255) {
				// Leftover packet, write it
				line.TWrite(hdr);
				line.Write(packet, hdr & 0x80? comp : comp * (hdr + 1));
			}
			line.MemOutput(&out);
		}
		delete[] packet;
	} else {
		// Raw
		for (int y = height - 1; y >= 0; y--) {
			out.Write(dat + y * rowSize, rowSize);
		}
	}

	return false;
}

bool targa_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	// Read header
	tgaHeader_s hdr;
	if (in.TRead(hdr) || hdr.colorMapType) {
		return true;
	}
	info->width = hdr.width;
	info->height = hdr.height;
	if ((hdr.imgType & 7) == 3 && hdr.depth == 8) {
		info->alpha = false;
		info->comp = 1;
	} else if ((hdr.imgType & 7) == 2 && hdr.depth == 24) {
		info->alpha = false;
		info->comp = 3;
	} else if ((hdr.imgType & 7) == 2 && hdr.depth == 32) {
		info->alpha = true;
		info->comp = 4;
	} else {
		return true;
	}
	return false;
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
	if (!stbi_info_from_memory(fileData.data(), fileData.size(), &x, &y, &in_comp)) {
		return true;
	}
	if (in_comp != 1 && in_comp != 3) {
		con->Warning("JPEG '%s': unsupported component count '%d'", fileName, comp);
		return true;
	}
	stbi_uc* data = stbi_load_from_memory(fileData.data(), fileData.size(), &x, &y, &in_comp, in_comp);
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

// JPEG Image Info

bool jpeg_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}
	
	std::vector<byte> fileData(in.GetLen());
	if (in.Read(fileData.data(), fileData.size())) {
		return true;
	}
	int x, y, comp;
	if (stbi_info_from_memory(fileData.data(), fileData.size(), &x, &y, &comp)) {
		return true;
	}

	info->width = x;
	info->height = y;
	info->comp = comp;
	info->alpha = false;

	return (comp != 1 && comp != 3);
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
	if (!stbi_info_from_memory(fileData.data(), fileData.size(), &x, &y, &in_comp)) {
		return true;
	}
	width = x;
	height = y;
	comp = (in_comp == 1 || in_comp == 3) ? 3 : 4;
	type = comp == 3 ? IMGTYPE_RGB : IMGTYPE_RGBA;
	stbi_uc* data = stbi_load_from_memory(fileData.data(), fileData.size(), &x, &y, &in_comp, comp);
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

// PNG Image Info

bool png_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	// Open file and check signature
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	std::vector<byte> fileData(in.GetLen());
	if (in.Read(fileData.data(), fileData.size())) {
		return true;
	}
	int x, y, comp;
	if (stbi_info_from_memory(fileData.data(), fileData.size(), &x, &y, &comp)) {
		return true;
	}

	info->width = x;
	info->height = y;
	info->comp = comp <= 3 ? 3 : comp;
	info->alpha = comp == 4;
	return false;
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
		stbi_uc* data = stbi_load_from_memory(fileData.data(), fileData.size(), &x, &y, &in_comp, 4);
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

bool gif_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	return true;
}

// =========
// BLP Image
// =========

enum blpType_e {
	BLP_JPEG,
	BLP_PALETTE,
	BLP_DXTC
};	

#pragma pack(push,1)
struct blpHeader_s {
	dword	id;
	dword	unknown1;
	byte	type;
	byte	alphaDepth;
	byte	alphaType;
	byte	hasMipmaps;
	dword	width;
	dword	height;
};
struct blpMipmapHeader_s {
	dword	offset[16];
	dword	size[16];
};
#pragma pack(pop)

bool blp_c::Load(const char* fileName)
{
	Free();

	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	// Read header
	blpHeader_s hdr;
	if (in.TRead(hdr)) {
		return true;
	}
	if (hdr.id != BLP2_MAGIC) {
		con->Warning("'%s' is not a BLP2 file", fileName);
		return true;
	}
	if (hdr.type != BLP_DXTC || !hdr.hasMipmaps) {
		// To hell with compatability
		con->Warning("'%s': unsupported image type (type: %d hasMipmaps: %d)", fileName, hdr.type, hdr.hasMipmaps);
		return true;
	}
	if (hdr.alphaDepth == 0) {
		comp = 3;
		type = IMGTYPE_RGB_DXT1;
	} else if (hdr.alphaDepth == 1) {
		comp = 4;
		type = IMGTYPE_RGBA_DXT1;
	} else if (hdr.alphaDepth == 8) {
		comp = 4;
		if (hdr.alphaType == 7) {
			type = IMGTYPE_RGBA_DXT5;
		} else {
			type = IMGTYPE_RGBA_DXT3;
		}
	} else {
		con->Warning("'%s': unsupported alpha type (alpha depth: %d alpha type %d)", fileName, hdr.alphaDepth, hdr.alphaType);
		return true;
	}
	width = hdr.width;
	height = hdr.height;

	// Read the mipmap header
	blpMipmapHeader_s mip;
	in.TRead(mip);

	// Read image
	int isize = 0;
	int numMip = 0;
	for (int c = 0; c < 16; c++) {
		if (mip.size[c] == 0) {
			numMip = c;
			break;
		}
		isize+= mip.size[c];
	}
	dat = new byte[isize + numMip*sizeof(dword)];
	byte* datPtr = dat;
	for (int c = 0; c < numMip; c++) {
		memcpy(datPtr, mip.size + c, sizeof(dword));
		datPtr+= sizeof(dword);
		in.Seek(mip.offset[c], SEEK_SET);
		in.Read(datPtr, mip.size[c]);
		datPtr+= mip.size[c];
	}

	return false;
}

bool blp_c::Save(const char* fileName)
{
	// No.
	return true;
}

bool blp_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	// Read header
	blpHeader_s hdr;
	if (in.TRead(hdr)) {
		return true;
	}
	if (hdr.id != BLP2_MAGIC) {
		return true;
	}
	if (hdr.type != BLP_DXTC) {
		// To hell with compatability
		return true;
	}
	if (hdr.alphaDepth == 0) {
		info->alpha = false;
		info->comp = 3;
	} else if (hdr.alphaDepth == 1 || hdr.alphaDepth == 8) {
		info->alpha = true;
		info->comp = 4;
	} else {
		return true;
	}
	info->width = hdr.width;
	info->height = hdr.height;
	return false;
}
