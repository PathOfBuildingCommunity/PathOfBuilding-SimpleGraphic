// SimpleGraphic Engine
//
// Dynamic Glyph Rasterisation — macOS (CoreText) backend
//
// We deliberately go through CoreText rather than bundling a rasteriser and a
// font file: CTFontCreateForString gives us the system's font fallback chain,
// so a codepoint gets rendered with whatever installed font actually covers it
// (Songti/PingFang for Han, Hiragino for kana, Apple Color Emoji, ...) with no
// font list to curate.

#include "r_dynfont.h"

#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cmath>

namespace {

// Convert a codepoint to the UTF-16 units CoreText expects.
// Returns the number of units written (1 for the BMP, 2 for a surrogate pair).
int CodepointToUTF16(char32_t cp, UniChar out[2])
{
	if (cp < 0x10000) {
		out[0] = (UniChar)cp;
		return 1;
	}
	char32_t v = cp - 0x10000;
	out[0] = (UniChar)(0xD800 + (v >> 10));
	out[1] = (UniChar)(0xDC00 + (v & 0x3FF));
	return 2;
}

// Pick the font that actually has a glyph for this codepoint, starting from
// the standard UI font and letting CoreText walk the fallback chain.
// Returns a +1 reference the caller must release, or nullptr.
CTFontRef FontForCodepoint(char32_t cp, int pixelSize, CGGlyph& glyphOut)
{
	UniChar units[2];
	int unitCount = CodepointToUTF16(cp, units);

	CTFontRef base = CTFontCreateUIFontForLanguage(kCTFontUIFontUser, (CGFloat)pixelSize, nullptr);
	if (!base) {
		return nullptr;
	}

	CGGlyph glyphs[2] = {0, 0};
	if (CTFontGetGlyphsForCharacters(base, units, glyphs, unitCount) && glyphs[0]) {
		glyphOut = glyphs[0];
		return base;
	}

	// The base font can't render it — ask CoreText for a substitute.
	CFStringRef str = CFStringCreateWithCharacters(nullptr, units, unitCount);
	if (!str) {
		CFRelease(base);
		return nullptr;
	}
	CTFontRef fallback = CTFontCreateForString(base, str, CFRangeMake(0, unitCount));
	CFRelease(str);
	CFRelease(base);
	if (!fallback) {
		return nullptr;
	}

	glyphs[0] = glyphs[1] = 0;
	if (!CTFontGetGlyphsForCharacters(fallback, units, glyphs, unitCount) || !glyphs[0]) {
		CFRelease(fallback);
		return nullptr;
	}
	glyphOut = glyphs[0];
	return fallback;
}

} // namespace

bool DynFontAvailable()
{
	return true;
}

bool DynFontRasterize(char32_t cp, int pixelSize, dynGlyph_s& out)
{
	if (pixelSize <= 0 || pixelSize > 512) {
		return false;
	}

	CGGlyph glyph = 0;
	CTFontRef font = FontForCodepoint(cp, pixelSize, glyph);
	if (!font) {
		return false;
	}

	CGRect bounds = CTFontGetBoundingRectsForGlyphs(font, kCTFontOrientationHorizontal, &glyph, nullptr, 1);
	CGSize advanceSize{};
	CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, &glyph, &advanceSize, 1);

	out = dynGlyph_s{};
	out.advance = (int)std::ceil(advanceSize.width);
	// The caller positions text by the top of the line box, so it needs to know
	// where the baseline sits within it.
	out.ascent = (int)std::ceil(CTFontGetAscent(font));

	// Whitespace and other zero-area glyphs carry an advance but no pixels.
	if (CGRectIsEmpty(bounds) || CGRectIsNull(bounds)) {
		CFRelease(font);
		return true;
	}

	// Pad by one pixel on each side so antialiased edges aren't clipped.
	int x0 = (int)std::floor(CGRectGetMinX(bounds)) - 1;
	int y0 = (int)std::floor(CGRectGetMinY(bounds)) - 1;
	int x1 = (int)std::ceil(CGRectGetMaxX(bounds)) + 1;
	int y1 = (int)std::ceil(CGRectGetMaxY(bounds)) + 1;

	int w = x1 - x0;
	int h = y1 - y0;
	if (w <= 0 || h <= 0 || w > 1024 || h > 1024) {
		CFRelease(font);
		return false;
	}

	std::vector<uint8_t> pixels((size_t)w * h, 0);
	CGContextRef ctx = CGBitmapContextCreate(pixels.data(), w, h, 8, w, nullptr, kCGImageAlphaOnly);
	if (!ctx) {
		CFRelease(font);
		return false;
	}

	CGContextSetShouldAntialias(ctx, true);
	CGContextSetShouldSmoothFonts(ctx, false);	// grayscale AA; subpixel makes no sense in an alpha mask

	// Place the glyph so its bounding box lands at the bitmap origin.
	CGPoint pos = CGPointMake((CGFloat)-x0, (CGFloat)-y0);
	CTFontDrawGlyphs(font, &glyph, &pos, 1, ctx);
	CGContextFlush(ctx);
	CGContextRelease(ctx);
	CFRelease(font);

	out.width = w;
	out.height = h;
	out.bearingX = x0;
	// CoreGraphics y grows upwards; our renderer addresses rows downwards from
	// the top, so the top edge sits y1 above the baseline.
	out.bearingY = y1;
	// A CGBitmapContext stores its first row at the top of the image even
	// though its drawing origin is bottom-left, which already matches the
	// renderer's top-down texture rows — no flip needed here.
	out.coverage = std::move(pixels);

	return true;
}
