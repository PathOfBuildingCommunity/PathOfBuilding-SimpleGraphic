// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Texture
//

#include <mutex>
#include <vector>
#include <atomic>
#include "r_local.h"

#include "stb_image_resize.h"

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

static const byte t_blackImage[64 * 4] = {};

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
	void	ProcessPendingTextureUploads() override;

	// Encapsulated
	t_manager_c(r_renderer_c* renderer);
	~t_manager_c();

	r_renderer_c* renderer;

	r_tex_c* whiteTex;
	r_tex_c* blackTex;

	bool	AsyncAdd(r_tex_c* tex);
	bool	AsyncRemove(r_tex_c* tex);

	void	EnqueueTextureUpload(r_tex_c* tex);
	void	RemovePendingTextureUpload(r_tex_c* tex);

private:
	std::atomic<bool> doRun;
	std::atomic<int> runnersRunning;

	std::vector<std::thread> workers;
	std::vector<r_tex_c *> textureQueue;
	std::mutex mutex;

	std::vector<r_tex_c *> uploadQueue;
	std::mutex uploadMutex;

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
	blackTex = new r_tex_c(this, "@black", 0);

	doRun = true;
	runnersRunning = 0;
	const int runnersWanted = 4;

	for (int i = 0; i < runnersWanted; ++i)
	{
		workers.emplace_back([this] {
			ThreadProc();
			});
	}
	//ThreadStart();
	while (runnersRunning < runnersWanted) {
		renderer->sys->Sleep( 1 );
	}
}

t_manager_c::~t_manager_c()
{
	doRun = false;
	for (auto& worker : workers)
		worker.join();

	for (auto tex : textureQueue)
		delete tex;

	delete whiteTex;
	delete blackTex;
}

// =====================
// Texture Manager Class
// =====================

int t_manager_c::GetAsyncCount()
{
	std::lock_guard<std::mutex> lock ( mutex );
	return (int)textureQueue.size();
}

void t_manager_c::ProcessPendingTextureUploads()
{
	std::unique_lock lk(uploadMutex);
	for (auto tex : uploadQueue) {
		r_tex_c::PerformUpload(tex);
	}
	uploadQueue.clear();
}

bool t_manager_c::AsyncAdd(r_tex_c* tex)
{
	std::lock_guard<std::mutex> lock( mutex );
	if ( runnersRunning == 0 ) {
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
	while (tex->status == r_tex_c::PROCESSING || tex->status == r_tex_c::SIZE_KNOWN) {
		renderer->sys->Sleep( 1 );
	}

	if (tex->status == r_tex_c::PENDING_UPLOAD) {
		RemovePendingTextureUpload(tex);
	}
	
	return true;
}

void t_manager_c::EnqueueTextureUpload(r_tex_c* tex)
{
	std::scoped_lock lk(uploadMutex);
	uploadQueue.push_back(tex);
}

void t_manager_c::RemovePendingTextureUpload(r_tex_c* tex)
{
	std::scoped_lock lk(uploadMutex);
	if (auto I = std::find(uploadQueue.begin(), uploadQueue.end(), tex); I != uploadQueue.end())
		uploadQueue.erase(I);
}

void t_manager_c::ThreadProc()
{
	++runnersRunning;
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
	--runnersRunning;
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
	mipSet = BuildMipSet(std::unique_ptr<image_c>(img));
	Upload(*mipSet, flags);
	status = DONE;
}

r_tex_c::~r_tex_c()
{
	if (status >= IN_QUEUE && status < DONE) {
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
		0,
		0,
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
	fileName = i_fileName ? i_fileName : "";
	fileWidth = 0;
	fileHeight = 0;
}

void r_tex_c::Bind()
{
	if (status == DONE) {
		glBindTexture(GL_TEXTURE_2D, texId);
	} else {
		manager->blackTex->Bind();
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
{
	if (flags & TF_ASYNC)
		manager->AsyncAdd(this);
}

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

class mip_set_c {
public:
	// Take ownership of the top-level image or a shrunk copy of it if it exceeds the max texture dimensions.
	mip_set_c(std::unique_ptr<image_c>&& image, dword maxDim)
	{
		dword up_w = 1, up_h = 1;
		if (image->type >= IMGTYPE_RGB_DXT1 && image->type <= IMGTYPE_RGBA_DXT5 || image->width <= maxDim && image->height <= maxDim) {
			topImage = std::move(image);
		}
		else {
			// Find next highest powers of 2
			while (up_w < image->width) up_w <<= 1;
			while (up_h < image->height) up_h <<= 1;
			while (up_w > maxDim || up_h > maxDim) {
				// Really big image, downscale to legal limits
				up_w >>= 1;
				up_h >>= 1;
			}

			topImage = std::make_shared<image_c>();
			int comp = image->comp;
			topImage->comp = comp;
			topImage->width = up_w;
			topImage->height = up_h;
			topImage->type = image->type;
			topImage->dat = new byte[up_w * up_h * comp];
			stbir_resize_uint8_srgb(
				image->dat, image->width, image->height, image->width * comp,
				topImage->dat, up_w, up_h, up_w * comp, comp, comp == 4 ? 3 : STBIR_ALPHA_CHANNEL_NONE, 0);
		}
	}

	virtual int GetMipCount() const = 0;
	virtual std::weak_ptr<image_c> BorrowMipImage(int level) = 0;

protected:
	std::shared_ptr<image_c> topImage;
};

struct single_mip_set_c : mip_set_c {
	single_mip_set_c(std::unique_ptr<image_c>&& img, dword maxDim) : mip_set_c(std::move(img), maxDim) {}

	virtual int GetMipCount() const override { return 1; }

	virtual std::weak_ptr<image_c> BorrowMipImage(int level) override
	{
		if (level != 0)
			return {};
		return topImage;
	}
};

struct incremental_mip_set_c : mip_set_c {
	incremental_mip_set_c(std::unique_ptr<image_c>&& img, dword maxDim) : mip_set_c(std::move(img), maxDim), currentImage(topImage) {
		dword longest = (unsigned)(std::max)(topImage->width, topImage->height);
		mipCount = 1;
		while (longest > 1) {
			longest /= 2;
			++mipCount;
		}
	}

	virtual int GetMipCount() const override { return mipCount; }

	virtual std::weak_ptr<image_c> BorrowMipImage(int level) override {
		if (level < 0 || level >= GetMipCount())
			return {};
		if (level == 0)
			return topImage;

		// We can only ever incrementally step down in size, if a rewind is required we regenerate from the top.
		if (level < currentLevel) {
			currentImage = topImage;
			currentLevel = 0;
		}
		while (currentLevel < level) {
			currentImage = GenerateNextLevel();
			++currentLevel;
		}
		return currentImage;
	}

	std::shared_ptr<image_c> GenerateNextLevel()
	{
		int comp = currentImage->comp;
		dword up_w = currentImage->width;
		dword up_h = currentImage->height;

		// Calculate mipmap size
		dword mm_w = (up_w > 1) ? up_w >> 1 : 1;
		dword mm_h = (up_h > 1) ? up_h >> 1 : 1;

		// Set mipmapping source/dest
		const byte* in = currentImage->dat;

		auto nextImage = std::make_shared<image_c>();
		nextImage->comp = comp;
		nextImage->type = currentImage->type;
		nextImage->width = mm_w;
		nextImage->height = mm_h;
		nextImage->dat = new byte[mm_w * mm_h * comp];
		byte* out = nextImage->dat;

		stbir_resize_uint8_srgb(
			in, up_w, up_h, up_w * comp,
			out, mm_w, mm_h, mm_w * comp,
			comp, comp == 4 ? 3 : STBIR_ALPHA_CHANNEL_NONE, 0);

		return nextImage;
	}

	std::shared_ptr<image_c> currentImage;
	int currentLevel = 0;
	int mipCount;
};

struct filled_mip_set_c : incremental_mip_set_c {
	using Base = incremental_mip_set_c;
	filled_mip_set_c(std::unique_ptr<image_c>&& img, dword maxDim) : incremental_mip_set_c(std::move(img), maxDim)
	{
		for (int level = 1; level < GetMipCount(); ++level) {
			tailImages.push_back(Base::BorrowMipImage(level).lock());
		}
	}

	virtual std::weak_ptr<image_c> BorrowMipImage(int level) override
	{
		if (level < 0 || level >= GetMipCount())
			return {};
		if (level == 0)
			return topImage;
		return tailImages[level - 1];
	}

	// All but the top level image
	std::vector<std::shared_ptr<image_c>> tailImages;
};

std::shared_ptr<mip_set_c> r_tex_c::BuildMipSet(std::unique_ptr<image_c> img)
{
	const bool is_async = !!(flags & TF_ASYNC);
	const bool has_existing_mips = img->type >= IMGTYPE_RGB_DXT1 && img->type <= IMGTYPE_RGBA_DXT5;
	const bool want_mips = !(flags & TF_NOMIPMAP);

	if (!want_mips)
		// No mips, just conform to the interface.
		return std::make_shared<single_mip_set_c>(std::move(img), renderer->texMaxDim);
	else if (is_async)
		// Compute all the mips into memory.
		return std::make_shared<filled_mip_set_c>(std::move(img), renderer->texMaxDim);
	else
		// Lazily produce the mips on demand.
		return std::make_shared<incremental_mip_set_c>(std::move(img), renderer->texMaxDim);
}

void r_tex_c::LoadFile()
{
	if (_stricmp(fileName.c_str(), "@white") == 0) {
		// Upload an 8x8 white image
		auto raw = std::make_unique<image_c>();
		raw->CopyRaw(IMGTYPE_GRAY, 8, 8, t_whiteImage);
		Upload(single_mip_set_c(std::move(raw), renderer->texMaxDim), TF_NOMIPMAP);
		status = DONE;
		return;
	}
	else if (_stricmp(fileName.c_str(), "@black") == 0) {
		// Upload an 8x8 black image
		auto raw = std::make_unique<image_c>();
		raw->CopyRaw(IMGTYPE_RGBA, 8, 8, t_blackImage);
		Upload(single_mip_set_c(std::move(raw), renderer->texMaxDim), TF_NOMIPMAP);
		status = DONE;
		return;
	}

	// Try to load image file using appropriate loader
	std::string loadPath = fileName;
	std::unique_ptr<image_c> img(image_c::LoaderForFile(renderer->sys->con, loadPath.c_str()));
	if (img) {
		auto sizeCallback = [this](int width, int height) {
			this->fileWidth = width;
			this->fileHeight = height;
			this->status = SIZE_KNOWN;
		};
		error = img->Load(loadPath.c_str(), sizeCallback);
		if ( !error ) {
			const bool is_async = !!(flags & TF_ASYNC);
			mipSet = BuildMipSet(std::move(img));

			status = PENDING_UPLOAD;
			if (is_async) {
				// Post a main thread task to create and fill GPU textures.
				manager->EnqueueTextureUpload(this);
			}
			else {
				Upload(*mipSet, flags);
				status = DONE;
			}
			return;
		}
	}

	auto raw = std::make_unique<image_c>();
	raw->CopyRaw(IMGTYPE_GRAY, 8, 8, t_defaultTexture);
	Upload(single_mip_set_c(std::move(raw), renderer->texMaxDim), TF_NOMIPMAP);
	status = DONE;
}

void r_tex_c::PerformUpload(r_tex_c* tex)
{
	tex->Upload(*tex->mipSet, tex->flags);
	tex->status = DONE;
}

static std::atomic<size_t> inputBytes = 0;
static std::atomic<size_t> uploadedBytes = 0;

void r_tex_c::Upload(mip_set_c& mipSet, int flags)
{
	// Find and bind texture name
	glGenTextures(1, &texId);
	glBindTexture(GL_TEXTURE_2D, texId);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Set filters
	if (flags & TF_NOMIPMAP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	if (flags & TF_NEAREST) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	constexpr float anisotropyCap = 16.0f;
	static const float maxAnisotropy = [] {
		float ret{};
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &ret);
		return ret;
		}();
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, (std::min)(maxAnisotropy, anisotropyCap));

	// Set repeating
	if (flags & TF_CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	auto image = mipSet.BorrowMipImage(0).lock();

	int intformat = 0;
	if (renderer->r_compress->intVal && renderer->glCompressedTexImage2D) {
		// Enable compression
		if (image->comp == 3) {
			intformat = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
		}
		else if (image->comp == 4) {
			intformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		}
	}
	if (!intformat) {
		intformat = GLTypeForImgType(image->type);
	}

	for (int miplevel = 0; miplevel < mipSet.GetMipCount(); ++miplevel) {
		if (miplevel)
			image = mipSet.BorrowMipImage(miplevel).lock();

		const int up_w = image->width;
		const int up_h = image->height;

		// Upload the mipmap
		uploadedBytes += up_w * up_h * image->comp;
		glTexImage2D(GL_TEXTURE_2D, miplevel, intformat, up_w, up_h, 0, GLTypeForImgType(image->type), GL_UNSIGNED_BYTE, image->dat);
		if ((up_w == 1 && up_h == 1) || flags & TF_NOMIPMAP) {
			break;
		}
	}
}
