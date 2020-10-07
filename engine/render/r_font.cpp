// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: Render Font
//

#include "r_local.h"

// =======
// Classes
// =======

// Glyph parameters
struct f_glyph_s {
	double	tcLeft;
	double	tcRight;
	double	tcTop;
	double	tcBottom;
	int		width;
	int		spLeft;
	int		spRight;
};

// Font height info
struct f_fontHeight_s {
	r_tex_c* tex;
	int		height;
	int		numGlyph;
	f_glyph_s glyphs[128];
};

// ===========
// Font Loader
// ===========

r_font_c::r_font_c(r_renderer_c* renderer, const char* fontName)
	: renderer(renderer)
{
	numFontHeight = 0;
	fontHeightMap = NULL;

	char fileNameBase[260];
	sprintf_s(fileNameBase, 260, CFG_DATAPATH "fonts/%s", fontName);

	// Open info file
	char tgfName[260];
	sprintf_s(tgfName, 260, "%s.tgf", fileNameBase);
	fileInputStream_c tgf;
	if (tgf.FileOpen(tgfName, false)) {
		renderer->sys->con->Warning("font \"%s\" not found", fontName);
		return;
	}

	// Read contents of info file
	size_t tgfLen = tgf.GetLen();
	char* tgfDat = AllocStringLen(tgfLen);
	tgf.Read(tgfDat, tgfLen);
	tgf.FileClose();

	maxHeight = 0;
	f_fontHeight_s* fh = NULL;

	// Parse info file
	char* tk_context;
	char* sub = strtok_s(tgfDat, "\n", &tk_context);
	do {
		int h, x, y, w, sl, sr;
		if (sscanf_s(sub, "HEIGHT %u;", &h) == 1) {
			// New height
			fh = new f_fontHeight_s;
			fontHeights[numFontHeight++] = fh;
			char tgaName[260];
			sprintf_s(tgaName, 260, "%s.%d.tga", fileNameBase, h);
			fh->tex = new r_tex_c(renderer->texMan, tgaName, TF_NOMIPMAP);
			fh->height = h;
			if (h > maxHeight) {
				maxHeight = h;
			}
			fh->numGlyph = 0;
		} else if (fh && sscanf_s(sub, "GLYPH %u %u %u %d %d;", &x, &y, &w, &sl, &sr) == 5) {
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
	} while (sub = strtok_s(NULL, "\n", &tk_context));

	FreeString(tgfDat);

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

int r_font_c::StringWidthInternal(f_fontHeight_s* fh, const char* str)
{
	int width = 0;
	while (*str && *str != '\n') {
		int escLen = IsColorEscape(str);
		if (escLen) {
			str+= escLen;
		} else if (*str == '\t') {
			int spWidth = fh->glyphs[' '].width + fh->glyphs[' '].spLeft + fh->glyphs[' '].spRight;
			width+= spWidth << 2;
			str++;
		} else {
			width+= fh->glyphs[*str].width + fh->glyphs[*str].spLeft + fh->glyphs[*str].spRight;
			str++;
		}
	}
	return width;
}

int r_font_c::StringWidth(int height, const char* str)
{
	f_fontHeight_s *fh = fontHeights[height > maxHeight? (numFontHeight - 1) : fontHeightMap[height]];
	int max = 0;
	const char* lptr = str;
	while (*lptr) {
		if (*lptr != '\n') {
			int lw = (int)(StringWidthInternal(fh, lptr) * (double)height / fh->height);
			if (lw > max) max = lw;
		}
		const char* nptr = strchr(lptr, '\n');
		if (nptr) {
			lptr = nptr + 1;
		} else {
			break;
		}
	}
	return max;
}

const char* r_font_c::StringCursorInternal(f_fontHeight_s* fh, const char* str, int curX)
{
	int x = 0;
	while (*str && *str != '\n') {
		int escLen = IsColorEscape(str);
		if (escLen) {
			str+= escLen;
		} else if (*str == '\t') {
			int spWidth = fh->glyphs[' '].width + fh->glyphs[' '].spLeft + fh->glyphs[' '].spRight;
			x+= spWidth << 1;
			if (curX <= x) {
				break;
			}
			x+= spWidth << 1;
			str++;
		} else {
			x+= fh->glyphs[*str].width + fh->glyphs[*str].spLeft + fh->glyphs[*str].spRight;
			if (curX <= x) {
				break;
			}
			str++;
		}
	}
	return str;
}

int	r_font_c::StringCursorIndex(int height, const char* str, int curX, int curY)
{
	f_fontHeight_s *fh = fontHeights[height > maxHeight? (numFontHeight - 1) : fontHeightMap[height]];
	int lastIndex = 0;
	int lineY = height;
	curX = (int)(curX / (double)height * fh->height);
	const char* lptr = str;
	while (1) {
		lastIndex = (int)(StringCursorInternal(fh, lptr, curX) - str);
		if (curY <= lineY) {
			break;
		}
		const char* nptr = strchr(lptr, '\n');
		if (nptr) {
			lptr = nptr + 1;
		} else {
			break;
		}
		lineY+= height;
	}
	return lastIndex;
}

void r_font_c::DrawTextLine(scp_t pos, int align, int height, col4_t col, const char* str)
{
	// Check if the line is visible
	if (pos[Y] >= renderer->sys->video->vid.size[1] || pos[Y] <= -height) {
		// Just process the colour codes
		while (*str && *str != '\n') {
			// Check for escape character
			int escLen = IsColorEscape(str);
			if (escLen) {
				ReadColorEscape(str, col);
				col[3] = 1.0f;
				renderer->curLayer->Color(col);
				str+= escLen;
				continue;
			}
			str++;
		}
		return;
	}

	// Find best height to use
	f_fontHeight_s *fh = fontHeights[height > maxHeight? (numFontHeight - 1) : fontHeightMap[height]];
	double scale = (double)height / fh->height;

	// Calculate the string position
	double x = pos[X];
	double y = pos[Y];
	if (align != F_LEFT) {
		// Calculate the real width of the string
		double width = StringWidthInternal(fh, str) * scale;
		switch (align) {
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

	renderer->curLayer->Bind(fh->tex);

	// Render the string
	while (*str && *str != '\n') {
		// Skip unprintable characters
		if (*str >= fh->numGlyph || *str < 0) {
			str++;
			continue;
		}

		// Check for escape character
		int escLen = IsColorEscape(str);
		if (escLen) {
			ReadColorEscape(str, col);
			col[3] = 1.0f;
			renderer->curLayer->Color(col);
			str+= escLen;
			continue;
		}

		// Handle tabs
		if (*str == '\t') {
			int spWidth = fh->glyphs[' '].width + fh->glyphs[' '].spLeft + fh->glyphs[' '].spRight;
			x+= (spWidth << 2) * scale;
			str++;
			continue;
		}

		// Draw glyph
		f_glyph_s* glyph = fh->glyphs + *(str++);
		x+= glyph->spLeft * scale;
		if (glyph->width) {
			double w = glyph->width * scale;
			if (x + w >= 0 && x < renderer->sys->video->vid.size[0]) {
				renderer->curLayer->Quad(
					glyph->tcLeft, glyph->tcTop, x, y,
					glyph->tcRight, glyph->tcTop, x + w, y,
					glyph->tcRight, glyph->tcBottom, x + w, y + height,
					glyph->tcLeft, glyph->tcBottom, x, y + height
				);
			}
			x+= w;
		}
		x+= glyph->spRight * scale;
	}
}

void r_font_c::Draw(scp_t pos, int align, int height, col4_t col, const char* str)
{
	if (*str == 0) {
		pos[Y]+= height;
		return;
	}

	// Prepare for rendering
	renderer->curLayer->Color(col);

	// Separate into lines and render them
	const char* lptr = str;
	while (*lptr) {
		if (*lptr != '\n') {
			DrawTextLine(pos, align, height, col, lptr);
		}
		pos[Y]+= height;
		const char* nptr = strchr(lptr, '\n');
		if (nptr) {
			lptr = nptr + 1;
		} else {
			break;
		}
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
	Draw(pos, align, height, col, str);
}
