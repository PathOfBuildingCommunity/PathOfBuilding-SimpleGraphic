// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Core Image
//

#include "common.h"

#include "core_image.h"

#include <jpeglib.h>
#include <png.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

struct jpegError_s: public jpeg_error_mgr {
	jpegError_s()
	{
		jpeg_std_error(this);
		output_message = MSGOut;
		error_exit = FatalError;
	}
	static void MSGOut(j_common_ptr cinfo)
	{
		char buffer[JMSG_LENGTH_MAX];
		(*cinfo->err->format_message)(cinfo, buffer);
		clientData_s* cd = (clientData_s*)cinfo->client_data;
		cd->con->Warning("JPEG '%s': %s", cd->fileName, buffer);
	}
	static void FatalError(j_common_ptr cinfo)
	{
		MSGOut(cinfo);
		throw 1;
	}
};

// JPEG Reading

struct jpegRead_s: public jpeg_source_mgr {
	ioStream_c* in = nullptr;
	byte buffer[1024] = {};
	jpegRead_s(ioStream_c* in)
		: in(in)
	{
		init_source = Init;
		term_source = Term;
		fill_input_buffer = Fill;
		skip_input_data = SkipInput;
		resync_to_restart = jpeg_resync_to_restart;
	}
	static void Init(j_decompress_ptr jdecomp)
	{
		jpegRead_s* jr = (jpegRead_s*)jdecomp->src;
		jr->next_input_byte = jr->buffer;
		jr->bytes_in_buffer = 0;
	}
	static void Term(j_decompress_ptr jdecomp)
	{
	}
	static boolean Fill(j_decompress_ptr jdecomp)
	{
		static unsigned char dummyEOI[2] = {0xFF, JPEG_EOI};
		jpegRead_s* jr = (jpegRead_s*)jdecomp->src;
		size_t avail = jr->in->GetLen() - jr->in->GetPos();
		if (avail) {
			jr->next_input_byte = jr->buffer;
			jr->bytes_in_buffer = (std::min<size_t>)(avail, 1024);
			jr->in->Read(jr->buffer, jr->bytes_in_buffer);
		} else {
			jr->next_input_byte = dummyEOI;
			jr->bytes_in_buffer = 2;
		}
		return true;
	}
	static void SkipInput(j_decompress_ptr jdecomp, long count)
	{
		jpegRead_s* jr = (jpegRead_s*)jdecomp->src;
		while (count > 0) {
			if (jr->bytes_in_buffer == 0) {
				Fill(jdecomp);
			}
			long skip = (std::min<long>)(count, (long)jr->bytes_in_buffer);
			jr->next_input_byte+= skip;
			jr->bytes_in_buffer-= skip;
			count-= skip;
		}
	}
};

bool jpeg_c::Load(const char* fileName)
{
	Free();

	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	// Initialise decompressor
	jpegError_s jerror;
	jpeg_decompress_struct jdecomp;
	jdecomp.err = &jerror;
	jpeg_create_decompress(&jdecomp);
	clientData_s cd;
	cd.fileName = fileName;
	cd.con = con;
	jdecomp.client_data = &cd;

	// Initialise source manager
	jpegRead_s src(&in);
	jdecomp.src = &src;

	byte** rows = NULL;
	try {
		// Read header
		jpeg_read_header(&jdecomp, true);
		width = jdecomp.image_width;
		height = jdecomp.image_height;
		comp = jdecomp.num_components;
		if (comp != 1 && comp != 3) {
			con->Warning("JPEG '%s': unsupported component count '%d'", fileName, comp);
			jpeg_destroy_decompress(&jdecomp);
			return true;
		}
		type = comp == 1? IMGTYPE_GRAY : IMGTYPE_RGB;

		// Allocate image and generate row pointers
		dat = new byte[width * height * comp];
		rows = new byte*[height];
		for (dword r = 0; r < height; r++) {
			rows[r] = dat + r * width * comp;
		}

		// Decompress
		jpeg_start_decompress(&jdecomp);
		while (jdecomp.output_scanline < height) {
			jpeg_read_scanlines(&jdecomp, rows + jdecomp.output_scanline, height - jdecomp.output_scanline);
		}
		jpeg_finish_decompress(&jdecomp);
	} 
	catch (...) {
	}
	delete[] rows;

	jpeg_destroy_decompress(&jdecomp);
	return false;
}

// JPEG Writing

struct jpegWrite_s: public jpeg_destination_mgr {
	ioStream_c* out = nullptr;
	byte buffer[1024] = {};
	jpegWrite_s(ioStream_c* out)
		: out(out)
	{
		init_destination = Init;
		term_destination = Term;
		empty_output_buffer = Empty;
	}
	static void Init(j_compress_ptr jcomp)
	{
		jpegWrite_s* jw = (jpegWrite_s*)jcomp->dest;
		jw->next_output_byte = jw->buffer;
		jw->free_in_buffer = 1024;
	}
	static void Term(j_compress_ptr jcomp)
	{
		jpegWrite_s* jw = (jpegWrite_s*)jcomp->dest;
		jw->out->Write(jw->buffer, 1024 - jw->free_in_buffer);
	}
	static boolean Empty(j_compress_ptr jcomp)
	{
		jpegWrite_s* jw = (jpegWrite_s*)jcomp->dest;
		jw->out->Write(jw->buffer, 1024);
		jw->next_output_byte = jw->buffer;
		jw->free_in_buffer = 1024;
		return true;
	}
};

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

	// Initialise compressor
	jpegError_s jerror;
	jpeg_compress_struct jcomp;
	jcomp.err = &jerror;
	jpeg_create_compress(&jcomp);
	clientData_s cd;
	cd.fileName = fileName;
	cd.con = con;
	jcomp.client_data = &cd;

	// Initialise destination manager
	jpegWrite_s dst(&out);
	jcomp.dest = &dst;

	// Set image parameters
	jcomp.image_width = width;
	jcomp.image_height = height;
	if (type == IMGTYPE_GRAY) {
		jcomp.input_components = 1;
		jcomp.in_color_space = JCS_GRAYSCALE;
	} else {
		jcomp.input_components = 3;
		jcomp.in_color_space = JCS_RGB;
	}
	jpeg_set_defaults(&jcomp);
	jpeg_set_quality(&jcomp, quality, true);

	// Generate row pointers
	byte** rows = new byte*[height];
	for (dword r = 0; r < height; r++) {
		rows[r] = dat + r * width * comp;
	}

	try {
		// Compress
		jpeg_start_compress(&jcomp, true);		
		jpeg_write_scanlines(&jcomp, rows, height);
		jpeg_finish_compress(&jcomp);
	}
	catch (...) {
		dst.Term(&jcomp);
	}
	delete[] rows;

	jpeg_destroy_compress(&jcomp);
	return false;
}

// JPEG Image Info

bool jpeg_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	// Open the file
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}

	// Initialise decompressor
	jpegError_s jerror;
	jpeg_decompress_struct jdecomp;
	jdecomp.err = &jerror;
	jpeg_create_decompress(&jdecomp);
	clientData_s cd;
	cd.fileName = fileName;
	cd.con = con;
	jdecomp.client_data = &cd;

	// Initialise source manager
	jpegRead_s src(&in);
	jdecomp.src = &src;

	try {
		// Read header
		jpeg_read_header(&jdecomp, true);
	}
	catch (...) {
		jpeg_destroy_decompress(&jdecomp);
		return true;
	}
	info->width = jdecomp.image_width;
	info->height = jdecomp.image_height;
	info->alpha = false;
	info->comp = jdecomp.num_components;

	jpeg_destroy_decompress(&jdecomp);
	return (comp != 1 && comp != 3);
}

// =========
// PNG Image
// =========

static void IPNG_ErrorProc(png_structp png, const char* msg)
{
	clientData_s* cd = (clientData_s*)png_get_error_ptr(png);
	cd->con->Warning("PNG '%s': %s", cd->fileName, msg);
}

// PNG Reading

static void IPNG_ReadProc(png_structp png, png_bytep data, png_size_t len)
{
	ioStream_c* in = (ioStream_c*)png_get_io_ptr(png);
	if (in->Read(data, len)) {
		png_error(png, "Truncated file");
	}
}

bool png_c::Load(const char* fileName)
{
	Free();

	// Open file and check signature
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}
	byte sig[8];
	if (in.Read(sig, 8) || png_sig_cmp(sig, 0, 8)) {
		con->Warning("PNG '%s': invalid signature", fileName);
		return true;
	}

	// Initialise PNG reader
	clientData_s cd;
	cd.con = con;
	cd.fileName = fileName;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &cd, IPNG_ErrorProc, IPNG_ErrorProc);
	png_infop info = png_create_info_struct(png);
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &info, NULL);
		return true;
	}
	png_set_read_fn(png, &in, IPNG_ReadProc);

	// Read image
	png_set_sig_bytes(png, 8);
	png_read_png(png, info, PNG_TRANSFORM_GRAY_TO_RGB | PNG_TRANSFORM_SCALE_16, NULL);

	// Copy image data
	width = png_get_image_width(png, info);
	height = png_get_image_height(png, info);
	comp = png_get_channels(png, info);
	switch (comp) {
	case 3:
		type = IMGTYPE_RGB;
		break;
	case 4:
		type = IMGTYPE_RGBA;
		break;
	default:
		con->Warning("PNG '%s': unknown image type %d", fileName, comp);
		png_destroy_read_struct(&png, &info, NULL);
		return true;
	};
	dat = new byte[width * height * comp];
	byte** rows = png_get_rows(png, info);
	for (dword y = 0; y < height; y++) {
		memcpy(dat + y * width * comp, rows[y], width * comp);
	}

	png_destroy_read_struct(&png, &info, NULL);
	return false;
}

// PNG Writing

static void IPNG_WriteProc(png_structp png, png_bytep data, png_size_t len)
{
	fileOutputStream_c* out = (fileOutputStream_c*)png_get_io_ptr(png);
	out->Write(data, len);
}

static void IPNG_FlushProc(png_structp png)
{
	fileOutputStream_c* out = (fileOutputStream_c*)png_get_io_ptr(png);
	out->FileFlush();
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

	// Initialise PNG writer
	clientData_s cd;
	cd.fileName = fileName;
	cd.con = con;
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, &cd, IPNG_ErrorProc, IPNG_ErrorProc);
	png_infop pnginfo = png_create_info_struct(png);
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_write_struct(&png, &pnginfo);
		return true;
	}
	png_set_write_fn(png, &out, IPNG_WriteProc, IPNG_FlushProc);

	// Write image
	png_set_IHDR(png, pnginfo, width, height, 8, type == IMGTYPE_RGBA? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	byte** rows = new byte*[height];
	for (dword r = 0; r < height; r++) {
		rows[r] = dat + r * width * comp;
	}
	png_set_rows(png, pnginfo, rows);
	png_write_png(png, pnginfo, PNG_TRANSFORM_IDENTITY, NULL);
	delete[] rows;

	png_destroy_write_struct(&png, &pnginfo);
	return false;
}

// PNG Image Info

bool png_c::ImageInfo(const char* fileName, imageInfo_s* info)
{
	// Open file and check signature
	fileInputStream_c in;
	if (in.FileOpen(fileName, true)) {
		return true;
	}
	byte sig[8];
	if (in.Read(sig, 8) || png_sig_cmp(sig, 0, 8)) {
		return true;
	}

	// Initialise PNG reader
	clientData_s cd;
	cd.con = con;
	cd.fileName = fileName;
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, &cd, IPNG_ErrorProc, IPNG_ErrorProc);
	png_infop pnginfo = png_create_info_struct(png);
	if (setjmp(png_jmpbuf(png))) {
		png_destroy_read_struct(&png, &pnginfo, NULL);
		return true;
	}
	png_set_read_fn(png, &in, IPNG_ReadProc);

	// Get image info
	png_set_sig_bytes(png, 8);
	png_read_info(png, pnginfo);
	info->width = png_get_image_width(png, pnginfo);
	info->height = png_get_image_height(png, pnginfo);
	int type = png_get_color_type(png, pnginfo);
	png_destroy_read_struct(&png, &pnginfo, NULL);
	if (type & PNG_COLOR_MASK_COLOR) {
		if (type & PNG_COLOR_MASK_ALPHA) {
			info->alpha = true;
			info->comp = 4;
		} else {
			info->alpha = false;
			info->comp = 3;
		}
	} else {
		return true;
	}
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
