// SimpleGraphic Engine
//
// Input Method Editor bridge
//
// GLFW receives the composition text an input method produces but discards
// it, and reports a fixed rectangle when asked where the caret is, so the
// candidate window has nowhere sensible to appear. Without both of those a
// user typing Chinese, Japanese or Korean sees nothing happen at all.
//
// This module fills those two gaps on the platform side. It does not touch
// committed text, which already reaches the app through the normal character
// callback.
//
// Only implemented on macOS; elsewhere the calls are inert and behaviour is
// unchanged.

#pragma once

struct GLFWwindow;

// Called whenever the composition text changes. `utf8Text` is the text being
// composed (empty when composition ends). `caret` is a byte offset into it.
using ime_preeditFn_t = void (*)(const char* utf8Text, int caret, void* userData);

// Hook into the window's text input so composition updates are reported.
// Safe to call more than once; the last callback wins.
void IME_Install(GLFWwindow* window, ime_preeditFn_t fn, void* userData);

// Tell the input method where the caret currently is, in window coordinates
// with the origin at the top left, so the candidate window can be placed
// next to it. Height should cover the text line.
void IME_SetCaretRect(int x, int y, int width, int height);

// Whether an IME bridge is active on this build.
bool IME_Available();
