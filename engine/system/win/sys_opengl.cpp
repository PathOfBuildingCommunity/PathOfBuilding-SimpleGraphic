// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: System OpenGL
// Platform: Windows
//

#include "sys_local.h"

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

// =====================
// sys_IOpenGL Interface
// =====================

typedef char* (WINAPI *PFNWGLGETEXTENSIONSTRINGEXTPROC)(void);
typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXTPROC)(int);

class sys_openGL_c: public sys_IOpenGL {
public:
	// Interface
	bool	Init(sys_glSet_s* set);
	bool	Shutdown();
	void	Swap();

	void*	GetProc(const char* name);
	bool	CaptureSecondary();
	bool	ReleaseSecondary();

	// Encapsulated
	sys_openGL_c(sys_IMain* sysHnd);

	sys_main_c* sys;

	PFNWGLGETEXTENSIONSTRINGEXTPROC wglGetExtensionsStringEXT = nullptr;
	PFNWGLSWAPINTERVALEXTPROC		wglSwapIntervalEXT = nullptr;

	HDC		hdc = nullptr;				// Device contexts
	HDC		hdc2 = nullptr;
	HGLRC	hglrc = nullptr;			// Rendering contexts
	HGLRC	hglrc2 = nullptr;
};

sys_IOpenGL* sys_IOpenGL::GetHandle(sys_IMain* sysHnd)
{
	return new sys_openGL_c(sysHnd);
}

void sys_IOpenGL::FreeHandle(sys_IOpenGL* hnd)
{
	delete (sys_openGL_c*)hnd;
}

sys_openGL_c::sys_openGL_c(sys_IMain* sysHnd)
	: sys((sys_main_c*)sysHnd)
{
}

// ===================
// System OpenGL Class
// ===================

bool sys_openGL_c::Init(sys_glSet_s* set)
{
	// Get device handles
	auto wnd = (GLFWwindow*)sys->video->GetWindowHandle();
	HWND hwnd = (HWND)glfwGetWin32Window(wnd);
	hdc = GetDC(hwnd);
	hdc2 = GetDC(hwnd);

	// Initialise pixel format
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize			= sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion		= 1;
	pfd.dwFlags			= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType		= PFD_TYPE_RGBA;
	pfd.cColorBits		= set->bColor? set->bColor : 32;
	pfd.cDepthBits		= set->bDepth;
	pfd.cStencilBits	= set->bStencil;

	sys->con->Printf("Pixel format: need %d,%d,%d", pfd.cColorBits, pfd.cDepthBits, pfd.cStencilBits);

	// Find pixel format
	int pfi = ChoosePixelFormat(hdc, &pfd);
	if (pfi == 0) {
		sys->PrintLastError("\nChoosePixelFormat() failed");
		return true;
	}

	// Set pixel format
	if (SetPixelFormat(hdc, pfi, &pfd) == 0) {
		sys->PrintLastError("\nSetPixelFormat() failed");
		return true;
	}

	// Retrieve actual pixel format
	DescribePixelFormat(hdc, pfi, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	sys->con->Printf(", using %d,%d,%d\n", pfd.cColorBits, pfd.cDepthBits, pfd.cStencilBits);

	// Prepare secondary device
	SetPixelFormat(hdc2, pfi, &pfd);

	// Create contexts
	sys->con->Printf("Creating context...\n");

	hglrc = wglCreateContext(hdc);
	if (hglrc == NULL) {
		sys->PrintLastError("wglCreateContext() failed");
		return true;
	}
	hglrc2 = wglCreateContext(hdc2);
	wglShareLists(hglrc, hglrc2);

	if (wglMakeCurrent(hdc, hglrc) == 0) {
		sys->PrintLastError("wglMakeCurrent() failed");
		return true;
	}

	// Load extensions
	wglSwapIntervalEXT = NULL;

	wglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");
	if (wglGetExtensionsStringEXT) { 
		sys->con->Printf("Loading WGL extensions...\n");

		char* wgle = wglGetExtensionsStringEXT();

		if (strstr(wgle, "WGL_EXT_swap_control")) {
			sys->con->Printf("using WGL_EXT_swap_control\n");
			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		} else {
			sys->con->Printf("WGL_EXT_swap_control not supported\n");
		}
	} else {
		sys->con->Printf("WGL extensions not supported.\n");
	}

	// Set swap interval
	if (wglSwapIntervalEXT) {
		wglSwapIntervalEXT(set->vsync? 1 : 0);
	}

	return false;
}

bool sys_openGL_c::Shutdown()
{
	// Delete context
	sys->con->Printf("Deleting context...\n");

	if (wglMakeCurrent(NULL, NULL) == 0) {
		return true;
	}
	if (wglDeleteContext(hglrc) == 0) {
		return true;
	}

	return false;
}

void sys_openGL_c::Swap()
{
	SwapBuffers(hdc);
}

void* sys_openGL_c::GetProc(const char* name)
{
	return wglGetProcAddress(name);
}

bool sys_openGL_c::CaptureSecondary()
{
	return !wglMakeCurrent(hdc2, hglrc2);
}

bool sys_openGL_c::ReleaseSecondary()
{
	return !wglMakeCurrent(NULL, NULL);
}
