// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Font Header
//

// =======
// Classes
// =======

#include <cstdint>
#include <string_view>
#include <unordered_map>

// Font
class r_font_c {
public:
	r_font_c(class r_renderer_c* renderer, const char* fontName);
	~r_font_c();

	int		StringWidth(int height, std::u32string_view str);
	int		StringCursorIndex(int height, std::u32string_view str, int curX, int curY);
	void	Draw(scp_t pos, int align, int height, col4_t col, std::u32string_view str);
	void	FDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, ...);
	void	VDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, va_list va);

private:
	int		StringWidthInternal(struct f_fontHeight_s* fh, std::u32string_view str, int height, float scale);
	size_t	StringCursorInternal(struct f_fontHeight_s* fh, std::u32string_view str, int height, float scale, int curX);
	void	DrawTextLine(scp_t pos, int align, int height, col4_t col, std::u32string_view str);

	struct EmbeddedFontSpec {
		f_fontHeight_s* fh;
		int yPad;
	};
	EmbeddedFontSpec FindSmallerFontHeight(int height, int heightIdx, int sizeReduction);
	
	struct FontHeightEntry {
		f_fontHeight_s* fh;
		int heightIdx;
	};
	FontHeightEntry FindFontHeight(int height);

	// A glyph rasterised on demand for a codepoint the bitmap fonts don't
	// cover. Each one owns a small texture; they are cached for the lifetime
	// of the font since the set of characters a build uses is small and stable.
	// Metrics are in layout (logical) units; the texture itself is rasterised at
	// the display's pixel density so it stays sharp on HiDPI screens.
	struct f_dynGlyph_s {
		class r_tex_c* tex = nullptr;	// null for blank glyphs (e.g. ideographic space)
		float	width = 0.0f;
		float	height = 0.0f;
		float	bearingX = 0.0f;
		float	bearingY = 0.0f;
		float	advance = 0.0f;
		bool	valid = false;
	};

	// Returns null when the platform can't produce a glyph, in which case
	// callers fall back to the "[U+XXXX]" placeholder.
	f_dynGlyph_s const* FindDynGlyph(char32_t cp, int pixelHeight);

	std::unordered_map<uint64_t, f_dynGlyph_s> dynGlyphs;

	class r_renderer_c* renderer = nullptr;
	int		numFontHeight = 0;
	struct f_fontHeight_s *fontHeights[32] = {};
	int		maxHeight = 0;
	int*	fontHeightMap = nullptr;
};
