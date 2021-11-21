// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Font
//

#include "r_local.h"

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

#include "FontFactory.h"

// =======
// Classes
// =======

// Glyph parameters
struct f_glyph_s
{
	double tcLeft = 0.0;
	double tcRight = 0.0;
	double tcTop = 0.0;
	double tcBottom = 0.0;
	int width = 0;
	int spLeft = 0;
	int spRight = 0;
};

// Font height info
struct f_fontHeight_s
{
	r_tex_c* tex;
	int height;
	int numGlyph;
	f_glyph_s glyphs[128];
};


// ===========
// Font Loader
// ===========
std::vector<char>* r_font_u::vecFontBuffer_ = nullptr;

r_font_u::r_font_u(r_renderer_c* renderer, const char* fontName)
	: renderer(renderer)
{
	numFontHeight = 0;
	fontHeightMap = NULL;

	char fileNameBase[260];
	sprintf_s(fileNameBase, 260, CFG_DATAPATH "fonts/%s", fontName);

	// Open info file
	char tgfName[260];
	sprintf_s(tgfName, 260, "%s.tgf", fileNameBase);
	std::ifstream tgf(tgfName);
	if (!tgf)
	{
		renderer->sys->con->Warning("font \"%s\" not found", fontName);
		return;
	}

	maxHeight = 0;
	f_fontHeight_s* fh = NULL;

	// Parse info file
	std::string sub;
	while (std::getline(tgf, sub))
	{
		int h, x, y, w, sl, sr;
		if (sscanf_s(sub.c_str(), "HEIGHT %u;", &h) == 1)
		{
			// New height
			fh = new f_fontHeight_s;
			fontHeights[numFontHeight++] = fh;
			char tgaName[260];
			sprintf_s(tgaName, 260, "%s.%d.tga", fileNameBase, h);
			fh->tex = new r_tex_c(renderer->texMan, tgaName, TF_NOMIPMAP);
			fh->height = h;
			if (h > maxHeight)
			{
				maxHeight = h;
			}
			fh->numGlyph = 0;
		}
		else if (fh && sscanf_s(sub.c_str(), "GLYPH %u %u %u %d %d;", &x, &y, &w, &sl, &sr) == 5)
		{
			// Add glyph
			if (fh->numGlyph >= 128) continue;
			f_glyph_s* glyph = &fh->glyphs[fh->numGlyph++];
			glyph->tcLeft = (double)x / fh->tex->fileWidth;
			glyph->tcRight = (double)(x + w) / fh->tex->fileWidth;
			glyph->tcTop = (double)y / fh->tex->fileHeight;
			glyph->tcBottom = (double)(y + fh->height) / fh->tex->fileHeight;
			glyph->width = w;
			glyph->spLeft = sl;
			glyph->spRight = sr;
		}
	}

	// Generate mapping of text height to font height
	fontHeightMap = new int[maxHeight + 1];
	memset(fontHeightMap, 0, sizeof(int) * (maxHeight + 1));
	for (int i = 0; i < numFontHeight; i++)
	{
		int gh = fontHeights[i]->height;
		for (int h = gh; h <= maxHeight; h++)
		{
			fontHeightMap[h] = i;
		}
		if (i > 0)
		{
			int belowH = fontHeights[i - 1]->height;
			int lim = (gh - belowH - 1) / 2;
			for (int b = 0; b < lim; b++)
			{
				fontHeightMap[gh - b - 1] = i;
			}
		}
	}

	//std::string sFontHeight;
	//for (int i = 0; i < numFontHeight; i++)
	//{
	//	sFontHeight.append(std::to_string(fontHeights[i]->height)).append(",");
	//}
}

r_font_u::r_font_u(r_renderer_c* renderer, bool bold)
	: renderer(renderer), fontSpacingX_(0)
{
	if (!vecFontBuffer_)
	{
		vecFontBuffer_ = new std::vector<char>();
		const conVar_c* varFontName = renderer->sys->con->Cvar_Add("font_name", CV_ARCHIVE, "ZdDroidF");
		std::wstring wsFontName;
		UTF8A2W(varFontName->strVal, &wsFontName);
		//Here is the WIN API feature support, if you transplant Linux, you need to pay attention
		wchar_t wildcardPath[MAX_PATH]{ '\0' };
		swprintf(wildcardPath, MAX_PATH, CFG_DATAPATH L"fonts/%s.*", wsFontName.c_str());
		std::vector<std::wstring> files = ListFiles(wildcardPath);
		if (!files.empty())
		{
			//Select only the first one
			wchar_t fontPath[MAX_PATH]{ '\0' };
			swprintf(fontPath, MAX_PATH, CFG_DATAPATH L"fonts/%s", (*files.begin()).c_str());
			if (ZReadFile(fontPath, vecFontBuffer_))
				renderer->sys->con->Printf("Current font: \"%s\"", fontPath);
			else
				renderer->sys->con->Warning("Font \"%s\" not found", varFontName->strVal);

		}
		else
		{
			renderer->sys->con->Warning("Font \"%s\" not found, use system font", varFontName->strVal);
			*vecFontBuffer_ = FontFactory::GetSystemFont(strcmp(varFontName->strVal, varFontName->defVal) == 0 ? nullptr : wsFontName.c_str());

			//std::ofstream outFile;
			//outFile.open("./testX.ttc", std::ios::binary);
			//outFile.write(vecFontBuffer_->data(), vecFontBuffer_->size());
			//outFile.close();

			renderer->sys->con->Printf("Current font: \"%s\"", varFontName->strVal);
		}
	}
	if (vecFontBuffer_->empty())
	{
		delete vecFontBuffer_;
		vecFontBuffer_ = nullptr;
		return;
	}

	const conVar_c* varFontDPI = renderer->sys->con->Cvar_Add("font_resolution", CV_ARCHIVE, "72", 50, 150);
	//Font Heights 6,10,12,14,16,18,20,24,28,32
	for (const int h : {6, 10, 12, 14, 16, 18, 20, 24, 28, 32})
	{
		ZFont* font = FontFactory::Me()->createFont(*vecFontBuffer_, h, bold, varFontDPI->intVal);
		if (!font)
		{
			renderer->sys->con->Warning("Create font failed: height %d", h);
			return;
		}
		mapFonts_[h] = font;
	}
	const conVar_c* varSpacingX = renderer->sys->con->Cvar_Add("font_spacing_x", CV_ARCHIVE, "1", -2, 10);
	fontSpacingX_ = varSpacingX->intVal;

}

r_font_u::~r_font_u()
{
	// Delete textures
	for (int i = 0; i < numFontHeight; i++)
	{
		delete fontHeights[i]->tex;
		delete fontHeights[i];
	}
	delete fontHeightMap;
}

// =============
// Font Renderer
// =============

int r_font_u::StringWidthInternal(ZFont* font, const wstring& strW)
{
	int width = 0;
	std::wstring::const_iterator it = strW.begin();
	std::wstring::const_iterator itEnd = strW.end();
	while (it != itEnd)
	{
		int cLen = IsColorEscape(it);
		if (cLen)
			it += cLen;
		else
		{
			ZFont::ZGlyph* glyph = font->getCharacter(*it);
			width += glyph->displayWidth() + fontSpacingX_;
			++it;
		}
	}
	return width;
}

int r_font_u::StringWidth(int height, const char* str)
{
	wstring sText;
	int nRet = UTF8A2W(str, &sText);
	if (!nRet)
		return 0;
	ZFont* font = mapFonts_[height];
	if (!font)
		font = mapFonts_.end()->second;

	int maxWidth = 0;
	double scale = (double)height / font->height();
	vector<wstring> vecLines = StringSplit(sText, '\n');
	for (auto& s : vecLines)
	{
		const double nRetWidth = StringWidthInternal(font, s) * scale;
		if (nRetWidth > maxWidth)
			maxWidth = (int)nRetWidth;
	}
	return maxWidth;
}

int r_font_u::StringCursorIndex(int height, const char* str, int curX, int curY)
{
	wstring sText;
	UTF8A2W(str, &sText);
	if (sText.empty())
		return 0;
	ZFont* font = mapFonts_[height];
	if (!font)
		font = mapFonts_.end()->second;

	size_t lastIndex = 0;
	int lineY = height;
	curX = (int)(curX / (double)height * font->height());
	vector<wstring> vecLines = StringSplit(sText, '\n');
	for (auto& s : vecLines)
	{
		if (lineY <= curY)
			lastIndex += s.size() + 1; //string + '\n'
		else
		{
			lastIndex += StringCursorInternal(font, s, curX);
			break;
		}
		lineY += height;
	}
	//std::string subText;
	//UTF8W2A(sText.substr(0, lastIndex).c_str(), &subText);
	return lastIndex;
}

int r_font_u::StringCursorInternal(ZFont* font, const wstring& strW, int curX)
{
	int x = 0;
	std::wstring::const_iterator it = strW.begin();
	std::wstring::const_iterator itEnd = strW.end();
	while (it != itEnd)
	{
		int cLen = IsColorEscape(it);
		if (cLen)
			it += cLen;
		else
		{
			ZFont::ZGlyph* glyph = font->getCharacter(*it);
			x += glyph->displayWidth() + fontSpacingX_;
			if (curX <= x)
			{
				break;
			}
			++it;
		}
	}
	return it - strW.begin();
}


void r_font_u::DrawTextLine(scp_t pos, int align, int height, col4_t col, const wstring& strW)
{
	std::wstring::const_iterator it = strW.begin();
	std::wstring::const_iterator itEnd = strW.end();
	// Check if the line is visible
	if (pos[Y] >= renderer->sys->video->vid.size[1] || pos[Y] <= -height)
	{
		// Just process the colour codes
		while (it != itEnd)
		{
			// Check for escape character
			int cLen = IsColorEscape(it);
			if (cLen)
			{
				ReadColorEscape(it, col);
				col[3] = 1.0f;
				renderer->curLayer->Color(col);
				it += cLen;
				continue;
			}
			++it;
		}
		return;
	}

	//// Find best height to use
	ZFont* font = mapFonts_[height];
	if (!font)
		font = mapFonts_.end()->second;

	double scale = (double)height / font->height();

	// Calculate the string position
	double x = pos[X];
	double y = pos[Y];
	if (align != F_LEFT)
	{
		// Calculate the real width of the string
		double width = StringWidthInternal(font, strW) * scale;
		switch (align)
		{
		case F_CENTRE:
			x = floor((renderer->sys->video->vid.size[0] - width) / 2.0f + pos[X]);
			break;
		case F_RIGHT:
			x = floor(renderer->sys->video->vid.size[0] - width - pos[X]);
			break;
		case F_CENTRE_X:
			x = floor(pos[X] - width / 2.0f);
			break;
		case F_RIGHT_X:
			x = floor(pos[X] - width);
			break;
		}
	}

	renderer->curLayer->Bind(font->tex());

	// Render the string
	while (it != itEnd)
	{
		// Check for escape character
		int cLen = IsColorEscape(it);
		if (cLen)
		{
			ReadColorEscape(it, col);
			col[3] = 1.0f;
			renderer->curLayer->Color(col);
			it += cLen;
			continue;
		}
#ifdef _DEBUG
		if (*it == std::wstring(L"\r")[0]) //L"\x4E00" 一 //L"\xFF08" (
			int zz = 0;
#endif
		ZFont::ZGlyph* glyph = font->getCharacter(*it);
		// Draw glyph
		if (glyph->width)
		{
			double w = glyph->width * scale;
			if (x + w >= 0 && x < renderer->sys->video->vid.size[0])
			{
				double destX = (x + glyph->HBearingX) * scale;
				double destY = y + (height - glyph->HBearingY) - 2;
				renderer->curLayer->Quad(
					glyph->left, glyph->top, destX, destY,
					glyph->right, glyph->top, destX + w, destY,
					glyph->right, glyph->bottom, destX + w, destY + glyph->height,
					glyph->left, glyph->bottom, destX, destY + glyph->height
				);
			}
		}
		x += glyph->displayWidth() * scale + fontSpacingX_;

		++it;
	}
}

void r_font_u::Draw(scp_t pos, int align, int height, col4_t col, const char* str)
{
	if (*str == 0)
	{
		pos[Y] += height;
		return;
	}
	wstring sText;
	int nRet = UTF8A2W(str, &sText);
	if (!nRet)
		return;

	// Prepare for rendering
	renderer->curLayer->Color(col);
	// Separate into lines and render them
	vector<wstring> vecLines = StringSplit(sText, '\n');
	for (auto& s : vecLines)
	{
		DrawTextLine(pos, align, height, col, s);
		pos[Y] += height;
	}
}

void r_font_u::FDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	VDraw(pos, align, height, col, fmt, va);
	va_end(va);
}

void r_font_u::VDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, va_list va)
{
	char str[65536];
	vsnprintf(str, 65535, fmt, va);
	str[65535] = 0;
	Draw(pos, align, height, col, str);
}
