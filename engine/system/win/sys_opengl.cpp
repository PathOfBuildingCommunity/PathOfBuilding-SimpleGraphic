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

	// Encapsulated
	sys_openGL_c(sys_IMain* sysHnd);

	sys_main_c* sys;

	PFNWGLGETEXTENSIONSTRINGEXTPROC wglGetExtensionsStringEXT = nullptr;
	PFNWGLSWAPINTERVALEXTPROC		wglSwapIntervalEXT = nullptr;
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
	return false;
}

void sys_openGL_c::Swap()
{
	glfwSwapBuffers((GLFWwindow*)sys->video->GetWindowHandle());
}

void* sys_openGL_c::GetProc(const char* name)
{
	return glfwGetProcAddress(name);
}
