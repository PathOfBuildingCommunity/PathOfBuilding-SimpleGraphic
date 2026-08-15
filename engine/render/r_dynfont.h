// SimpleGraphic Engine
//
// Dynamic Glyph Rasterisation
//
// The bundled bitmap fonts only cover the first 128 codepoints. Anything
// outside that range used to be drawn as a "[U+XXXX]" tofu placeholder.
// This module rasterises arbitrary codepoints on demand using the platform
// font engine, which also gives us automatic font fallback (CJK, Cyrillic,
// emoji, ...) without having to ship or pick a font ourselves.
//
// Only implemented on macOS for now; other platforms report "no glyph" and
// keep the existing tofu behaviour.

#pragma once

#include <cstdint>
#include <vector>

struct dynGlyph_s {
	int		width = 0;		// bitmap width in pixels
	int		height = 0;		// bitmap height in pixels
	int		bearingX = 0;	// x offset from the pen position to the bitmap's left edge
	int		bearingY = 0;	// y offset from the baseline up to the bitmap's top edge
	int		advance = 0;	// how far to move the pen after drawing
	int		ascent = 0;		// distance from the top of the line box down to the baseline
	std::vector<uint8_t> coverage;	// width * height, 8-bit alpha coverage
};

// Rasterise one codepoint at the requested pixel size.
// Returns false when the platform has no glyph for it, or on any failure —
// callers should fall back to their existing placeholder rendering.
bool DynFontRasterize(char32_t cp, int pixelSize, dynGlyph_s& out);

// Whether dynamic rasterisation is available at all on this build.
bool DynFontAvailable();
