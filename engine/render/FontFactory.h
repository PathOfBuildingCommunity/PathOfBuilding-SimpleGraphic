#ifndef FONTMANAGER_H
#define FONTMANAGER_H

// OpenGL library

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_OUTLINE_H

#include "r_local.h"

class ZFont;

class FontFactory
{
public:
	static FontFactory* Me();

	FontFactory();
	~FontFactory();
	ZFont* createFont(const char* fileName, int height, bool bold, int resolution = 96);
	ZFont* createFont(const std::vector<char>& fontBuffer,int height,bool bold,int resolution = 96);
	static std::vector<char> GetSystemFont(const wchar_t* faceName);

private:
	FT_Library ftLibrary_;
	std::vector<ZFont*> vFonts_;
};

class ZFont
{
public:
	struct ZGlyph
	{
		double left;
		double right;
		double top;
		double bottom;
		int32_t width;
		int32_t height;
		int32_t HBearingX;
		int32_t HBearingY;
		int32_t HAdvance;

		ZGlyph();
		inline int displayWidth()
		{
			return width > HAdvance ? width : HAdvance;
		}
	};

	ZFont(int height);
	~ZFont();
	bool create(FT_Library library, const char* fileName, bool bold, int resolution);
	bool create(FT_Library library,const std::vector<char>& fontBuffer, bool bold, int resolution);

	//inline ZGlyph* getCharacter(wchar_t wc)
	//{
	//	return getCharacter((uint32_t)wc);
	//}

	ZGlyph* getCharacter(wchar_t wc);

	inline int height()
	{
		return nHeight_;
	}

	inline r_tex_c* tex()
	{
		return pTex_;
	}

private:
	FT_Face ftFace_;

	int nHeight_;
	bool bBold_;

	//OpenGL
	int32_t nTextureWidth;
	int32_t nTextureHeight;
	GLuint hTexture_;
	int32_t nTexOffsetX_;
	int32_t nTexOffsetY_;
	r_tex_c* pTex_;

	std::unordered_map<wchar_t, ZGlyph*> mapCharacter_;
};

#endif // FONTMANAGER_H
