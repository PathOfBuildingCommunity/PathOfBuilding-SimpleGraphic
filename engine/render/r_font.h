// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Font Header
//

// =======
// Classes
// =======

// Font
class r_font_c {
public:
	r_font_c(class r_renderer_c* renderer, char* fontName);
	~r_font_c();

	int		StringWidth(int height, const char* str);
	int		StringCursorIndex(int height, const char* str, int curX, int curY);
	void	Draw(scp_t pos, int align, int height, col4_t col, const char* str);
	void	FDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, ...);
	void	VDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, va_list va);

private:
	int		StringWidthInternal(struct f_fontHeight_s* fh, const char* str);
	const char*	StringCursorInternal(struct f_fontHeight_s* fh, const char* str, int curX);
	void	DrawTextLine(scp_t pos, int align, int height, col4_t col, const char* str);

	class r_renderer_c* renderer = nullptr;
	int		numFontHeight = 0;
	struct f_fontHeight_s *fontHeights[32] = {};
	int		maxHeight = 0;
	int*	fontHeightMap = nullptr;
};
