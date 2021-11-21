// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Font Header
//

// =======
// Classes
// =======
#include <unordered_map>

using namespace std;

class ZFont;

// Font
class r_font_u
{
public:
	r_font_u(class r_renderer_c* renderer, const char* fontName);
	r_font_u(class r_renderer_c* renderer, bool bold);
	~r_font_u();

	int StringWidth(int height, const char* str);
	int StringCursorIndex(int height, const char* str, int curX, int curY);
	void Draw(scp_t pos, int align, int height, col4_t col, const char* str);
	void FDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, ...);
	void VDraw(scp_t pos, int align, int height, col4_t col, const char* fmt, va_list va);

private:
	int StringWidthInternal(ZFont* font, const wstring& strW);
	int StringCursorInternal(ZFont* font, const wstring& strW, int curX);
	void DrawTextLine(scp_t pos, int align, int height, col4_t col, const wstring& strW);

	class r_renderer_c* renderer = nullptr;
	int numFontHeight = 0;
	struct f_fontHeight_s* fontHeights[32] = {};
	int maxHeight = 0;
	int* fontHeightMap = nullptr;
	int fontSpacingX_;
	static std::vector<char>* vecFontBuffer_;

	std::unordered_map<int, ZFont*> mapFonts_;
};
