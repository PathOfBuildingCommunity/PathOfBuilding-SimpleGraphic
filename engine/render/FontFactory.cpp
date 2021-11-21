#include "FontFactory.h"
#include <iostream>
#include <fstream>
//========= Font manager
#define TTCF_TABLE_TAG mmioFOURCC('t', 't', 'c', 'f')
#define MAX_FACE_NAME 32

FontFactory::FontFactory()
	: ftLibrary_(nullptr)
{
	FT_Error err;

	if ((err = FT_Init_FreeType(&ftLibrary_)) != FT_Err_Ok)
	{
		printf("FT_Init_FreeType() Error %d\n", err);
	}
}

FontFactory::~FontFactory()
{
	FT_Error err;

	for (auto* f : vFonts_)
	{
		delete f;
	}
	vFonts_.clear();

	if ((err = FT_Done_FreeType(ftLibrary_)) != FT_Err_Ok)
	{
		printf("FT_Done_FreeType() Error %d\n", err);
	}
}

ZFont* FontFactory::createFont(const char* fileName, int height, bool bold, int resolution)
{
	/*const auto font = new ZFont(height);
	if (!font->create(ftLibrary_, fileName, bold, resolution))
	{
		delete font;
		return nullptr;
	}

	vFonts_.push_back(font);
	return font;*/
	std::ifstream ifs;
	ifs.open(fileName, std::ios::binary);
	if(!ifs.is_open())
		return nullptr;
	ifs.seekg(0, std::ios::end);
	std::vector<char> vecFontBuffer(static_cast<size_t>(ifs.tellg()));
	ifs.seekg(0);
	ifs.read(vecFontBuffer.data(), vecFontBuffer.size());
	ifs.close();
	return createFont(vecFontBuffer,height,bold,resolution);
}

ZFont* FontFactory::createFont(const std::vector<char>& fontBuffer, int height, bool bold, int resolution)
{
	ZFont* font = new ZFont(height);
	if (!font->create(ftLibrary_, fontBuffer, bold, resolution))
	{
		delete font;
		return nullptr;
	}

	vFonts_.push_back(font);
	return font;
}

std::vector<char> FontFactory::GetSystemFont(const wchar_t* faceName)
{

	wchar_t szFaceName[MAX_FACE_NAME]{ '\0' };
	//Get System default font family
	if (!faceName)
	{
		LOGFONTW lf;
		SystemParametersInfoW(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
		wmemcpy(szFaceName, lf.lfFaceName, MAX_FACE_NAME);
	}
	else
		wmemcpy(szFaceName, faceName, MAX_FACE_NAME);

	HDC hDC = ::CreateCompatibleDC(NULL);
	HFONT hFont = CreateFontW(6, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, szFaceName);

	SelectObject(hDC, hFont);
	const size_t size = GetFontData(hDC, 0, 0, NULL, 0);

	std::vector<char> fontBuff(size);
	if (GetFontData(hDC, TTCF_TABLE_TAG, 0, fontBuff.data(), size) == size)
	{
		return fontBuff;
	}
	return {};
}

FontFactory* FontFactory::Me()
{
	static FontFactory _me;
	return &_me;
}

ZFont::ZGlyph::ZGlyph()
	: left(0), right(0), top(0), bottom(0), width(0), height(0), HBearingX(0), HBearingY(0), HAdvance(0)
{
}

//================ ZFont
ZFont::ZFont(int height)
	: ftFace_(nullptr), nHeight_(height), bBold_(false),
	  nTextureWidth(64 * height + 64), nTextureHeight(64 * height + 64),
	  hTexture_(0), nTexOffsetX_(0), nTexOffsetY_(0)
{
	mapCharacter_.reserve(64 * 64);

	glGenTextures(1, &hTexture_); // Using your API here
	glBindTexture(GL_TEXTURE_2D, hTexture_);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, nTextureWidth, nTextureHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);

	pTex_ = new r_tex_c(hTexture_);
}

ZFont::~ZFont()
{
	if (ftFace_)
		FT_Done_Face(ftFace_);

	delete pTex_;

	for (auto it = mapCharacter_.begin(); it != mapCharacter_.end(); ++it)
		delete it->second;
}

bool ZFont::create(FT_Library library, const char* fileName, bool bold, int resolution)
{
	FT_Error err;

	if ((err = FT_New_Face(library, fileName, 0, &ftFace_)) != FT_Err_Ok)
	{
		printf("FT_New_Face() Error %d\n", err);
		return false;
	}
	if ((err = FT_Select_Charmap(ftFace_, FT_ENCODING_UNICODE)) != FT_Err_Ok)
	{
		printf("FT_Select_Charmap() Error %d\n", err);
		return false;
	}

	if ((err = FT_Set_Char_Size(ftFace_, 0, nHeight_ << 6, resolution, 0)) != FT_Err_Ok)
	{
		printf("FT_Set_Char_Size() Error %d\n", err);
		return false;
	}
	//if ((err = FT_Set_Pixel_Sizes(ftFace_, 0, nHeight_)) != FT_Err_Ok)
	//{
	//	printf("FT_Set_Pixel_Sizes() Error %d\n", err);
	//	return false;
	//}

	if (!hTexture_)
	{
	}

	bBold_ = bold;

	return true;
}

bool ZFont::create(FT_Library library,const std::vector<char>& fontBuffer, bool bold, int resolution)
{
	FT_Error err;
	if((err = FT_New_Memory_Face(library, (FT_Byte*)fontBuffer.data(),fontBuffer.size(),0,&ftFace_)) != FT_Err_Ok)
	{
		return false;
	}
	if ((err = FT_Select_Charmap(ftFace_, FT_ENCODING_UNICODE)) != FT_Err_Ok)
	{
		printf("FT_Select_Charmap() Error %d\n", err);
		return false;
	}
	if ((err = FT_Set_Char_Size(ftFace_, 0, nHeight_ << 6, resolution, 0)) != FT_Err_Ok)
	{
		printf("FT_Set_Char_Size() Error %d\n", err);
		return false;
	}
	bBold_ = bold;

	return true;
}

ZFont::ZGlyph* ZFont::getCharacter(wchar_t uCode)
{
	auto it = mapCharacter_.find(uCode);
	if (it != mapCharacter_.end())
	{
		return it->second;
	}
	if (nTexOffsetX_ + nHeight_ > nTextureWidth)
	{
		nTexOffsetX_ = 0;
		nTexOffsetY_ += nHeight_ + 1;
	}

	ZGlyph* pCharacter = new ZGlyph;
	if (uCode == '\r')
	{
		//nothing
	}
	else
	{
		pCharacter->left = (double)nTexOffsetX_ / nTextureWidth;
		pCharacter->top = (double)nTexOffsetY_ / nTextureHeight;

		FT_Error err;
		if ((err = FT_Load_Char(ftFace_, uCode, FT_LOAD_RENDER | FT_LOAD_NO_BITMAP |
			(TRUE ? FT_LOAD_TARGET_NORMAL : FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO))) ==
			FT_Err_Ok)
		{
			FT_GlyphSlot glyph = ftFace_->glyph;
			if (bBold_ && (err = FT_Outline_EmboldenXY(&glyph->outline, 60, 0)) != FT_Err_Ok)
			{
				printf("FT_Outline_EmboldenXY() Error %d\n", err);
			}

			FT_Bitmap& bitmap = glyph->bitmap;

			pCharacter->width = bitmap.width;
			pCharacter->height = bitmap.rows;

			pCharacter->HAdvance = glyph->advance.x >> 6;
			if (bitmap.width)
			{
				pCharacter->right = (double)(nTexOffsetX_ + bitmap.width) / nTextureWidth;
				pCharacter->bottom = (double)(nTexOffsetY_ + bitmap.rows) / nTextureHeight;
				pCharacter->HBearingX = glyph->bitmap_left;
				pCharacter->HBearingY = glyph->bitmap_top;

				glBindTexture(GL_TEXTURE_2D, hTexture_);
				glTexSubImage2D(GL_TEXTURE_2D, 0, nTexOffsetX_, nTexOffsetY_,
					bitmap.width, bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.buffer);

				nTexOffsetX_ += bitmap.width + 1;
			}
		}
		else
		{
			printf("FT_Load_Char() Error %d\n", err);
		}
	}

	mapCharacter_.emplace(uCode, pCharacter);
	return pCharacter;
}
