# Cyrillic Notes regressions

On Windows, use an x64 Visual Studio developer shell after building the DLL.
Provide a runtime directory containing the built DLLs and `SimpleGraphic/`
resources, and the generated fonts from the companion PathOfBuilding change.
The tests copy them into a new directory, leaving the supplied runtime untouched.

```powershell
./tests/run-windows.ps1 -Runtime C:/test-runtime -Fonts C:/PathOfBuilding/runtime/SimpleGraphic/Fonts -Output C:/new-test-output
```

`cyrillic-render.lua` checks the complete Russian alphabet at every font height,
ASCII metrics, color escapes, UTF-8 cursor offsets and the missing-glyph fallback.
`cyrillic-input.lua` checks ASCII, Cyrillic, other UTF-8 sequence lengths and a
named arrow key. The Windows driver sends messages only to its own process and
the exact test-window title. Both waits are bounded.

The renderer tests require the extended Liberation Sans TGF/TGA assets generated
by PathOfBuilding-FontGenerator; a runtime without those assets is expected to fail.

## Extended TGF format

Legacy `GLYPH x y width left right;` entries retain their sequential ASCII indexes.
An optional `GLYPH_U32 codepoint x y width left right;` entry supplies an explicit
Unicode codepoint in the same atlas. All numbers are decimal. Missing codepoints
keep the existing `[U+XXXX]` fallback. Old TGF assets remain supported.
