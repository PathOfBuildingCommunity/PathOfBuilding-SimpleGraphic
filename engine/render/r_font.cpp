// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Font
//

#include "r_local.h"

#include <fmt/format.h>
#include <iostream>
#include <fstream>
#include <string>

// =======
// Classes
// =======

// Glyph parameters
struct f_glyph_s {
	float	tcLeft = 0.0;
	float	tcRight = 0.0;
	float	tcTop = 0.0;
	float	tcBottom = 0.0;
	int		width = 0;
	int		spLeft = 0;
	int		spRight = 0;
};

// Font height info
struct f_fontHeight_s {
	r_tex_c* tex;
	int		height;
	int		numGlyph;
	f_glyph_s glyphs[128];
	f_glyph_s defGlyph{0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0};

	f_glyph_s const& Glyph(char ch) const {
		if ((unsigned char)ch >= numGlyph) {
			return defGlyph;
		}
		return glyphs[(unsigned char)ch];
	}
};

// ===========
// Font Loader
// ===========

r_font_c::r_font_c(r_renderer_c* renderer, const char* fontName)
	: renderer(renderer)
{
	numFontHeight = 0;
	fontHeightMap = NULL;

	std::string fileNameBase = fmt::format(CFG_DATAPATH "Fonts/{}", fontName);

	// Open info file
	std::string tgfName = fileNameBase + ".tgf";
	std::ifstream tgf(tgfName);
	if (!tgf) {
		renderer->sys->con->Warning("font \"%s\" not found", fontName);
		return;
	}

	maxHeight = 0;
	f_fontHeight_s* fh = NULL;

	// Parse info file
	std::string sub;
	while (std::getline(tgf, sub)) {
		int h, x, y, w, sl, sr;
		if (sscanf(sub.c_str(), "HEIGHT %u;", &h) == 1) {
			// New height
			fh = new f_fontHeight_s;
			fontHeights[numFontHeight++] = fh;
			std::string tgaName = fmt::format("{}.{}.tga", fileNameBase, h);
			fh->tex = new r_tex_c(renderer->texMan, tgaName.c_str(), TF_NOMIPMAP);
			fh->height = h;
			if (h > maxHeight) {
				maxHeight = h;
			}
			fh->numGlyph = 0;
		}
		else if (fh && sscanf(sub.c_str(), "GLYPH %u %u %u %d %d;", &x, &y, &w, &sl, &sr) == 5) {
			// Add glyph
			if (fh->numGlyph >= 128) continue;
			f_glyph_s* glyph = &fh->glyphs[fh->numGlyph++];
			glyph->tcLeft = (float)x / fh->tex->fileWidth;
			glyph->tcRight = (float)(x + w) / fh->tex->fileWidth;
			glyph->tcTop = (float)y / fh->tex->fileHeight;
			glyph->tcBottom = (float)(y + fh->height) / fh->tex->fileHeight;
			glyph->width = w;
			glyph->spLeft = sl;
			glyph->spRight = sr;
		}
	}

	// Generate mapping of text height to font height
	fontHeightMap = new int[maxHeight + 1];
	memset(fontHeightMap, 0, sizeof(int) * (maxHeight + 1));
	for (int i = 0; i < numFontHeight; i++) {
		int gh = fontHeights[i]->height;
		for (int h = gh; h <= maxHeight; h++) {
			fontHeightMap[h] = i;
		}
		if (i > 0) {
			int belowH = fontHeights[i - 1]->height;
			int lim = (gh - belowH - 1) / 2;
			for (int b = 0; b < lim; b++) {
				fontHeightMap[gh - b - 1] = i;
			}
		}
	}
}

r_font_c::~r_font_c()
{
	// Delete textures
	for (int i = 0; i < numFontHeight; i++) {
		delete fontHeights[i]->tex;
		delete fontHeights[i];
	}
	delete fontHeightMap;
}

// =============
// Font Renderer
// =============

std::u32string BuildTofuString(char32_t cp) {
	// Format unhandled Unicode codepoints like U+0123, higher planes have wider numbers.
	fmt::memory_buffer buf;
	fmt::format_to(fmt::appender(buf), "[U+{:04X}]", (uint32_t)cp);
	std::u32string ret;
	ret.reserve(buf.size());
	std::copy(buf.begin(), buf.end(), std::back_inserter(ret));
	return ret;
}

int const tofuSizeReduction = 3;

int r_font_c::StringWidthInternal(f_fontHeight_s* fh, std::u32string_view str, int height, float scale)
{
	int heightIdx = (int)(std::find(fontHeights, fontHeights + numFontHeight, fh) - fontHeights);
	auto tofuFont = FindSmallerFontHeight(height, heightIdx, tofuSizeReduction);

	auto measureCodepoint = [](f_fontHeight_s* fh, char32_t cp) {
		auto& glyph = fh->Glyph((char)(unsigned char)cp);
		return glyph.width + glyph.spLeft + glyph.spRight;
	};

	int width = 0;
	for (size_t idx = 0; idx < str.size();) {
		auto ch = str[idx];
		int escLen = IsColorEscape(str.substr(idx));
		if (escLen) {
			idx += escLen;
		}
		else if (ch >= (unsigned)fh->numGlyph) {
			auto tofu = BuildTofuString(ch);
			for (auto ch : tofu) {
				width += measureCodepoint(tofuFont.fh, ch);
			}
			++idx;
		}
		else if (ch == U'\t') {
			auto& glyph = fh->Glyph(' ');
			int spWidth = glyph.width + glyph.spLeft + glyph.spRight;
			width += (int)(spWidth * 4 * scale);
			++idx;
		}
		else {
			width += (int)(measureCodepoint(fh, ch) * scale);
			++idx;
		}
	}
	return width;
}

int r_font_c::StringWidth(int height, std::u32string_view str)
{
	auto mainFont = FindFontHeight(height);
	f_fontHeight_s* fh = mainFont.fh;
	int max = 0;
	float const scale = (float)height / fh->height;
	for (auto I = str.begin(); I != str.end(); ++I) {
		auto lineEnd = std::find(I, str.end(), U'\n');
		if (I != lineEnd) {
			std::u32string_view line(&*I, std::distance(I, lineEnd));
			int lw = (int)StringWidthInternal(fh, line, height, scale);
			max = (std::max)(max, lw);
		}
		if (lineEnd == str.end()) {
			break;
		}
		I = lineEnd;
	}
	return max;
}

size_t r_font_c::StringCursorInternal(f_fontHeight_s* fh, std::u32string_view str, int height, float scale, int curX)
{
	int heightIdx = (int)(std::find(fontHeights, fontHeights + numFontHeight, fh) - fontHeights);
	auto tofuFont = FindSmallerFontHeight(height, heightIdx, tofuSizeReduction);

	auto measureCodepoint = [](f_fontHeight_s* fh, char32_t cp) {
		auto& glyph = fh->Glyph((char)(unsigned char)cp);
		return glyph.width + glyph.spLeft + glyph.spRight;
	};

	int x = 0;
	auto I = str.begin();
	auto lineEnd = std::find(I, str.end(), U'\n');
	while (I != lineEnd) {
		auto tail = str.substr(std::distance(str.begin(), I));
		int escLen = IsColorEscape(tail);
		if (escLen) {
			I += escLen;
		}
		else if (*I >= (unsigned)fh->numGlyph) {
			auto tofu = BuildTofuString(*I);
			int tofuWidth = 0;
			for (auto ch : tofu) {
				tofuWidth += measureCodepoint(tofuFont.fh, ch);
			}
			int halfWidth = (int)(tofuWidth / 2.0f);
			x += halfWidth;
			if (curX <= x) {
				break;
			}
			x += tofuWidth - halfWidth;
			++I;
		}
		else if (*I == U'\t') {
			auto& glyph = fh->Glyph(' ');
			int spWidth = glyph.width + glyph.spLeft + glyph.spRight;
			int fullWidth = (int)(spWidth * 4 * scale);
			int halfWidth = (int)(fullWidth / 2.0f);
			x += halfWidth;
			if (curX <= x) {
				break;
			}
			x += fullWidth - halfWidth;
			++I;
		}
		else {
			x += (int)(measureCodepoint(fh, *I) * scale);
			if (curX <= x) {
				break;
			}
			++I;
		}
	}
	return std::distance(str.begin(), I);
}

int	r_font_c::StringCursorIndex(int height, std::u32string_view str, int curX, int curY)
{
	auto mainFont = FindFontHeight(height);
	f_fontHeight_s* fh = mainFont.fh;
	int lastIndex = 0;
	int lineY = height;
	float scale = (float)height / fh->height;

	auto I = str.begin();
	while (I != str.end()) {
		auto lineEnd = std::find(I, str.end(), U'\n');
		auto line = str.substr(std::distance(str.begin(), I), std::distance(I, lineEnd));
		lastIndex = (int)(StringCursorInternal(fh, line, height, scale, curX));
		if (curY <= lineY) {
			break;
		}
		if (lineEnd == str.end()) {
			break;
		}
		I = lineEnd + 1;
		lineY += height;
	}
	return (int)std::distance(str.begin(), I) + lastIndex;
}

r_font_c::EmbeddedFontSpec r_font_c::FindSmallerFontHeight(int height, int heightIdx, int sizeReduction) {
	EmbeddedFontSpec ret{};
	ret.fh = fontHeights[heightIdx];
	ret.yPad = 0;
	for (int tofuIdx = heightIdx - 1; tofuIdx >= 0; --tofuIdx) {
		auto candFh = fontHeights[tofuIdx];
		int heightDiff = height - candFh->height;
		if (heightDiff >= sizeReduction) {
			ret.fh = candFh;
			ret.yPad = (int)std::ceil(heightDiff / 2.0f);
			break;
		}
	}
	return ret;
}

r_font_c::FontHeightEntry r_font_c::FindFontHeight(int height) {
	FontHeightEntry ret{};
	if (height > maxHeight) {
		// Too large heights get the largest font size.
		ret.heightIdx = numFontHeight - 1;
	}
	else if (height < 0) {
		// Negative heights get the smallest font size.
		ret.heightIdx = 0;
	}
	else {
		ret.heightIdx = fontHeightMap[height];
	}
	ret.fh = fontHeights[ret.heightIdx];
	return ret;
}

void r_font_c::DrawTextLine(scp_t pos, int align, int height, col4_t col, std::u32string_view str)
{
	// Check if the line is visible
	if (pos[Y] >= renderer->sys->video->vid.size[1] || pos[Y] <= -height) {
		// Just process the colour codes
		while (!str.empty()) {
			// Check for escape character
			int escLen = IsColorEscape(str);
			if (escLen) {
				str = ReadColorEscape(str, col);
				col[3] = 1.0f;
				renderer->curLayer->Color(col);
				continue;
			}
			str = str.substr(1);
		}
		return;
	}

	// Find best height to use
	auto mainFont = FindFontHeight(height);
	f_fontHeight_s* fh = mainFont.fh;
	float scale = (float)height / fh->height;
	auto tofuFont = FindSmallerFontHeight(height, mainFont.heightIdx, tofuSizeReduction);

	// Calculate the string position
	float x = pos[X];
	float y = pos[Y];
	if (align != F_LEFT) {
		// Calculate the real width of the string
		float width = (float)StringWidthInternal(fh, str, height, scale);
		switch (align) {
		case F_CENTRE:
			x = floor((renderer->VirtualScreenWidth() - width) / 2.0f + pos[X]);
			break;
		case F_RIGHT:
			x = floor(renderer->VirtualScreenWidth() - width - pos[X]);
			break;
		case F_CENTRE_X:
			x = floor(pos[X] - width / 2.0f);
			break;
		case F_RIGHT_X:
			x = floor(pos[X] - width);
			break;
		}
	}

	r_tex_c* curTex{};

	auto drawCodepoint = [this, &curTex, &x, y](f_fontHeight_s* fh, int height, float scale, int yShift, char32_t cp) {
		float cpY = y + yShift;
		if (curTex != fh->tex) {
			curTex = fh->tex;
			renderer->curLayer->Bind(fh->tex);
		}
		auto& glyph = fh->Glyph((char)(unsigned char)cp);
		x += glyph.spLeft * scale;
		if (glyph.width) {
			float w = glyph.width * scale;
			if (x + w >= 0 && x < renderer->VirtualScreenWidth()) {
				renderer->curLayer->Quad(
					glyph.tcLeft, glyph.tcTop, x, cpY,
					glyph.tcRight, glyph.tcTop, x + w, cpY,
					glyph.tcRight, glyph.tcBottom, x + w, cpY + height,
					glyph.tcLeft, glyph.tcBottom, x, cpY + height
				);
			}
			x += w;
		}
		x += glyph.spRight * scale;
		x = std::ceil(x);
	};

	// Render the string
	for (auto tail = str; !tail.empty();) {
		// Draw unprintable characters as tofu placeholders
		auto ch = tail[0];
		if (ch >= (unsigned)fh->numGlyph) {
			auto tofu = BuildTofuString(ch);
			for (auto ch : tofu) {
				drawCodepoint(tofuFont.fh, tofuFont.fh->height, 1.0f, tofuFont.yPad, ch);
			}
			tail = tail.substr(1);
			continue;
		}

		// Check for escape character
		int escLen = IsColorEscape(tail);
		if (escLen) {
			tail = ReadColorEscape(tail, col);
			col[3] = 1.0f;
			renderer->curLayer->Color(col);
			continue;
		}

		// Handle tabs
		if (ch == U'\t') {
			auto& glyph = fh->Glyph(' ');
			int spWidth = glyph.width + glyph.spLeft + glyph.spRight;
			x+= (spWidth << 2) * scale;
			tail = tail.substr(1);
			continue;
		}

		// Draw glyph
		drawCodepoint(fh, height, scale, 0, ch);
		tail = tail.substr(1);
	}
}

void r_font_c::Draw(scp_t pos, int align, int height, col4_t col, std::u32string_view str)
{
	if (str.empty()) {
		pos[Y]+= height;
		return;
	}

	// Prepare for rendering
	renderer->curLayer->Color(col);

	// Separate into lines and render them
	for (auto I = str.begin(); I != str.end(); ++I) {
		auto lineEnd = std::find(I, str.end(), U'\n');
		if (I != lineEnd) {
			std::u32string_view line(&*I, std::distance(I, lineEnd));
			DrawTextLine(pos, align, height, col, line);
		}
		pos[Y] += height;
		if (lineEnd == str.end()) {
			break;
		}
		I = lineEnd;
	}
}

void r_font_c::FDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	VDraw(pos, align, height, col, fmt, va);
	va_end(va);
}

void r_font_c::VDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, va_list va)
{
	char str[65536];
	vsnprintf(str, 65535, fmt, va);
	str[65535] = 0;
	auto idxStr = IndexUTF8ToUTF32(str);
	Draw(pos, align, height, col, idxStr.text);
}
