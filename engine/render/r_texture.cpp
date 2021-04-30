// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Texture
//

#include <mutex>
#include <vector>
#include <atomic>
#include "r_local.h"

// ===================
// Predefined textures
// ===================

static const byte t_whiteImage[64] = { 
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

static const byte t_defaultTexture[64] = {
	0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F,
	0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x7F, 0x7F,
	0x7F, 0x7F, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x7F,
	0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
	0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
	0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
	0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F,
	0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F
};

// =======================
// r_ITexManager Interface
// =======================

class t_manager_c: public r_ITexManager, public thread_c {
public:
	// Interface
	int		GetAsyncCount() override;
	bool	GetImageInfo(const char* fileName, imageInfo_s* info) override;

	// Encapsulated
	t_manager_c(r_renderer_c* renderer);
	~t_manager_c();

	r_renderer_c* renderer;

	r_tex_c* whiteTex;

	bool	AsyncAdd(r_tex_c* tex);
	bool	AsyncRemove(r_tex_c* tex);

private:
	std::atomic<bool> doRun;
	std::atomic<bool> isRunning;

	std::vector<r_tex_c *> textureQueue;
	std::mutex mutex;

	void	ThreadProc() override;
};

r_ITexManager* r_ITexManager::GetHandle(r_renderer_c* renderer)
{
	return new t_manager_c(renderer);
}

void r_ITexManager::FreeHandle(r_ITexManager* hnd)
{
	delete (t_manager_c*)hnd;
}

t_manager_c::t_manager_c(r_renderer_c* renderer)
	: thread_c(renderer->sys), renderer(renderer)
{
	whiteTex = new r_tex_c(this, "@white", 0);

	doRun = true;
	isRunning = false;
	ThreadStart();
	while (!isRunning) {
		renderer->sys->Sleep( 1 );
	}
}

t_manager_c::~t_manager_c()
{
	doRun = false;
	while (isRunning) {
		renderer->sys->Sleep( 1 );
	}
	for (auto tex : textureQueue)
	{
		delete tex;
	}
	delete whiteTex;
}

// =====================
// Texture Manager Class
// =====================

int t_manager_c::GetAsyncCount()
{
	std::lock_guard<std::mutex> lock ( mutex );
	return textureQueue.size();
}

bool t_manager_c::GetImageInfo(const char* fileName, imageInfo_s* info)
{
	image_c* img = image_c::LoaderForFile(renderer->sys->con, fileName);
	if (img) {
		bool error = img->ImageInfo(fileName, info);
		delete img;
		return error;
	}
	return true;
}

bool t_manager_c::AsyncAdd(r_tex_c* tex)
{
	std::lock_guard<std::mutex> lock( mutex );
	if ( !isRunning ) {
		return true;
	}
	textureQueue.push_back( tex );
	tex->status = r_tex_c::IN_QUEUE;
	return false;
}

bool t_manager_c::AsyncRemove(r_tex_c* tex)
{
	{
		std::lock_guard<std::mutex> lock( mutex );
		if (tex->status == r_tex_c::IN_QUEUE) {
			for (auto itr = textureQueue.begin(); itr != textureQueue.end(); ++itr) {
				if (*itr == tex) {
					textureQueue.erase( itr );
					tex->status = r_tex_c::INIT;
					return false;
				}
			}
		}
	}
	while (tex->status == r_tex_c::PROCESSING) {
		renderer->sys->Sleep( 1 );
	}
	
	return true;
}

void t_manager_c::ThreadProc()
{
	isRunning = true;
	while (doRun) {
		r_tex_c *doTex = nullptr;
		{
			std::lock_guard<std::mutex> lock( mutex );

			// Find a texture with the highest loading priority
			int maxPri = 0;
			auto doTexItr = textureQueue.end();
			for (auto curTexItr = textureQueue.begin(); curTexItr != textureQueue.end(); ++curTexItr) {
				auto curTex = *curTexItr;
				if (doTexItr == textureQueue.end() || curTex->loadPri > maxPri) {
					maxPri = curTex->loadPri;
					doTexItr = curTexItr;
				}
			}

			if (doTexItr != textureQueue.end()) {
				doTex = *doTexItr;
				textureQueue.erase(doTexItr);
				doTex->status = r_tex_c::PROCESSING;
			}
		}
	
		if (doTex != nullptr) {
			// Load this texture
			doTex->LoadFile();
			doTex = nullptr;
		} else {
			// Idle
			renderer->sys->Sleep(1);
		}
	}
	isRunning = false;
}

// ===============
// Image Resampler
// ===============

class t_sampleDim_c {
public:
	int		max = 0;
	int		i1 = 0, i2 = 0;
	double	w1 = 0.0, w2 = 0.0;

	t_sampleDim_c(int imax)
	{
		max = imax;
	}

	void GenIndicies(double di)
	{
		i1 = (int)floor(di);
		i2 = (int)ceil(di);
		w2 = di - i1;
		w1 = 1.0f - w2;
		if (i2 >= max) {
			i2 = max - 1;
		}
	}
};

static void T_ResampleImage(byte* in, dword in_w, dword in_h, int in_comp, byte* out, dword out_w, dword out_h)
{
	// Initialise sample dimensions
	t_sampleDim_c six(in_w), siy(in_h);

	double xst = (double)in_w / out_w;
	double yst = (double)in_h / out_h;
		
	double dy = 0;
	for (dword y = 0; y < out_h; y++, dy+= yst) {
		// Generate Y indicies
		siy.GenIndicies(dy);
		double dx = 0;
		for (dword x = 0; x < out_w; x++, dx+= xst) {
			// Generate X indicies
			six.GenIndicies(dx);

			// Resample each component
			for (int c = 0; c < in_comp; c++) {
				out[in_comp * (y*out_w + x) + c] = 
					(byte)
					(
						(double)in[in_comp * (siy.i1 * six.max + six.i1) + c] * six.w1 * siy.w1 + 
						(double)in[in_comp * (siy.i2 * six.max + six.i1) + c] * six.w1 * siy.w2 +
						(double)in[in_comp * (siy.i1 * six.max + six.i2) + c] * six.w2 * siy.w1 + 
						(double)in[in_comp * (siy.i2 * six.max + six.i2) + c] * six.w2 * siy.w2 
					);
			}
		}
	}
}

// ====================
// OpenGL Texture Class
// ====================

r_tex_c::r_tex_c(r_ITexManager* manager, const char* fileName, int flags)
{
	Init(manager, fileName, flags);

	StartLoad();
	if (status == INIT) {
		// Load it now
		LoadFile();
	}
}

r_tex_c::r_tex_c(r_ITexManager* manager, image_c* img, int flags)
{
	Init(manager, NULL, flags);

	// Direct upload
	Upload(img, flags);
}

r_tex_c::~r_tex_c()
{
	if (status == IN_QUEUE || status == PROCESSING) {
		manager->AsyncRemove(this);
	}
	glDeleteTextures(1, &texId);
}

int r_tex_c::GLTypeForImgType(int type)
{
	static int gt[] = {
		0,
		GL_LUMINANCE,
		GL_RGB,
		GL_RGBA,
		GL_BGR,
		GL_BGRA,
		GL_COMPRESSED_RGB_S3TC_DXT1_EXT,
		GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
		GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,
		GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
	};
	return gt[type >> 4];
}

void r_tex_c::Init(r_ITexManager* i_manager, const char* i_fileName, int i_flags)
{
	manager = (t_manager_c*)i_manager;
	renderer = manager->renderer;
	error = 0;
	status = INIT;
	loadPri = 0;
	texId = 0;
	flags = i_flags;
	fileName = i_fileName;
	fileWidth = 0;
	fileHeight = 0;
}

void r_tex_c::Bind()
{
	ForceLoad();
	if (fileWidth) {
		glBindTexture(GL_TEXTURE_2D, texId);
	} else {
		manager->whiteTex->Bind();
	}
}

void r_tex_c::Unbind()
{
	glBindTexture(GL_TEXTURE_2D, 0);
}

void r_tex_c::Enable()
{	
	glEnable(GL_TEXTURE_2D);
}

void r_tex_c::Disable()
{
	Unbind();
	glDisable(GL_TEXTURE_2D);
}

void r_tex_c::StartLoad()
{}

void r_tex_c::AbortLoad()
{
	manager->AsyncRemove(this);
}

void r_tex_c::ForceLoad()
{
	if (status == INIT) {
		LoadFile();
	} else if (fileWidth == 0) {
		// Load not pending, do it now
		LoadFile();
	}
}

void r_tex_c::LoadFile()
{
	if (_stricmp(fileName.c_str(), "@white") == 0) {
		// Upload an 8x8 white image
		image_c raw;
		raw.CopyRaw(IMGTYPE_GRAY, 8, 8, t_whiteImage);
		Upload(&raw, TF_NOMIPMAP);
		status = DONE;
		return;
	}

	// Try to load image file using appropriate loader
	image_c* img = image_c::LoaderForFile(renderer->sys->con, fileName.c_str() );
	if (img) {
		error = img->Load(fileName.c_str());
		if ( !error ) {
			Upload(img, flags);
			delete img;
			status = DONE;
			return;
		}
		delete img;
	}

	image_c raw;
	raw.CopyRaw(IMGTYPE_GRAY, 8, 8, t_defaultTexture);
	Upload(&raw, TF_NOMIPMAP);
	status = DONE;
}

void r_tex_c::Upload(image_c* image, int flags)
{
	// Find and bind texture name
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Set filters
	if (flags & TF_NOMIPMAP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	if (flags & TF_NEAREST) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Set repeating
	if (flags & TF_CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	dword up_w = 1, up_h = 1;
	if (renderer->texNonPOT && (flags & TF_NOMIPMAP) && image->width <= renderer->texMaxDim && image->height <= renderer->texMaxDim) {
		up_w = image->width;
		up_h = image->height;
	} else {
		// Find next highest powers of 2
		while (up_w < image->width) up_w<<= 1;
		while (up_h < image->height) up_h<<= 1;
		if (up_w > renderer->texMaxDim || up_h > renderer->texMaxDim) {
			// Really big image, use next lowest instead
			up_w>>= 1;
			up_h>>= 1;
		}
	}

	// Special case for precompressed textures (BLP DXT)
	if (image->type >= IMGTYPE_RGB_DXT1 && image->type <= IMGTYPE_RGBA_DXT5) {
		if ( !renderer->glCompressedTexImage2DARB ) {
			image_c raw;
			raw.CopyRaw(IMGTYPE_GRAY, 8, 8, t_defaultTexture);
			Upload(&raw, TF_NOMIPMAP);
			return;
		}
		int miplevel = 0;
		byte* datPtr = image->dat;
		dword mipSize;
		while (1) {
			mipSize = *(dword*)datPtr;
			datPtr+= sizeof(dword);
			renderer->glCompressedTexImage2DARB(GL_TEXTURE_2D, miplevel, GLTypeForImgType(image->type), up_w, up_h, 0, mipSize, datPtr);
			datPtr+= mipSize;
			if ((up_w == 1 && up_h == 1) || flags & TF_NOMIPMAP) {
				break;
			}
			up_w = (up_w > 1)? up_w >> 1 : 1;
			up_h = (up_h > 1)? up_h >> 1 : 1;
			miplevel++;
		}
		fileWidth = image->width;
		fileHeight = image->height;
		return;
	}

	byte* mipBuf[3] = {NULL, NULL};

	if (up_w != image->width || up_h != image->height) {
		// Image needs resampling
		mipBuf[2] = new byte[image->comp * up_w * up_h];
		T_ResampleImage(image->dat, image->width, image->height, image->comp, mipBuf[2], up_w, up_h);
	} else {
		// Use original image data
		mipBuf[2] = image->dat;
	}

	int intformat = image->comp;
	if (renderer->r_compress->intVal && renderer->glCompressedTexImage2DARB) {
		// Enable compression
		if (image->comp == 3) {
			intformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		} else if (image->comp == 4) {
			intformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		}
	}

	int miplevel = 0;
	int cibuf = 2;

	while (1) {
		if (up_w <= renderer->texMaxDim && up_h <= renderer->texMaxDim) {
			// Upload the mipmap
			glTexImage2D(GL_TEXTURE_2D, miplevel, intformat, up_w, up_h, 0, GLTypeForImgType(image->type), GL_UNSIGNED_BYTE, mipBuf[cibuf]);	
			if ((up_w == 1 && up_h == 1) || flags & TF_NOMIPMAP) {
				break;
			}
			miplevel++;
		}

		// Calculate mipmap size
		dword mm_w = (up_w > 1)? up_w >> 1 : 1;
		dword mm_h = (up_h > 1)? up_h >> 1 : 1;

		// Set mipmapping source/dest
		byte* in = mipBuf[cibuf];
		cibuf = !cibuf;
		if ( !mipBuf[cibuf] ) {
			mipBuf[cibuf] = new byte[image->comp * mm_w * mm_h];
		}
		byte* out = mipBuf[cibuf];

		if (up_w == 1) {
			// Only halve height
			for (dword y = 0; y < mm_h; y++) {
				int iy = y * 2;
				for (dword x = 0; x < mm_w; x++) {
					for (int c = 0; c < image->comp; c++) {
						out[image->comp * (y * mm_w + x) + c] = (
							in[image->comp * ((iy  ) * up_w + x) + c] + 
							in[image->comp * ((iy+1) * up_w + x) + c]
						) >> 1;
					}
				}
			}
		}
		else if (up_h == 1) {
			// Only halve width
			for (dword y = 0; y < mm_h; y++) {
				for (dword x = 0; x < mm_w; x++) {
					int ix = x * 2;
					for (int c = 0; c < image->comp; c++) {
						out[image->comp * (y * mm_w + x) + c] = (
							in[image->comp * (y * up_w + (ix  )) + c] + 
							in[image->comp * (y * up_w + (ix+1)) + c]
						) >> 1;
					}
				}
			}
		}
		else {
			// Halve image size
			for (dword y = 0; y < mm_h; y++) {
				int iy = y * 2;
				for (dword x = 0; x < mm_w; x++) {
					int ix = x * 2;
					for (int c = 0; c < image->comp; c++) {
						out[image->comp * (y * mm_w + x) + c] = (
							in[image->comp * ((iy  ) * up_w + (ix  )) + c] + 
							in[image->comp * ((iy  ) * up_w + (ix+1)) + c] +
							in[image->comp * ((iy+1) * up_w + (ix  )) + c] + 
							in[image->comp * ((iy+1) * up_w + (ix+1)) + c]
						) >> 2;
					}
				}
			}
		}

		up_w = mm_w;
		up_h = mm_h;
	}

	delete mipBuf[0];
	delete mipBuf[1];
	if (mipBuf[2] != image->dat) {
		delete mipBuf[2];
	}

	fileWidth = image->width;
	fileHeight = image->height;
}
