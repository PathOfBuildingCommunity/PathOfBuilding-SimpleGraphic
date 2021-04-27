// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// System Local Header
// Platform: Windows
//

#include "common.h"

#include "system.h"

#define _WIN32_WINNT _WIN32_WINNT_WINXP
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <mmsystem.h>

// =======
// Classes
// =======

// System Main: sys_main.cpp
class sys_main_c: public sys_IMain {
public:
	// Interface
	int		GetTime();
	void	Sleep(int msec);
	bool	IsKeyDown(byte key);
	void	ShowCursor(int doShow);
	void	ClipboardCopy(const char* str);
	char*	ClipboardPaste();
	bool	SetWorkDir(const char* newCwd = NULL);
	void	SpawnProcess(const char* cmdName, const char* argList);
	void	OpenURL(const char* url);
	void	Error(const char* fmt, ...);
	void	Exit(const char* msg = NULL);
	void	Restart();

	// Encapsulated
	sys_main_c();

	bool	Run(int argc, char** argv);

	static LRESULT __stdcall WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void	RunMessages(HWND hwnd = NULL);
	void	PrintLastError(const char* msg);

	int		KeyToVirtual(byte key);
	byte	VirtualToKey(int virt);
	byte	GlfwKeyToKey(int key);
	char	GlfwKeyExtraChar(int key);

	HINSTANCE hinst = nullptr;
	HICON	icon = nullptr;

	class core_IMain* core = nullptr;

	bool	initialised = false;
	volatile bool	exitFlag = false;
	volatile bool	restartFlag = false;
	char*	exitMsg = nullptr;
	char*	threadError = nullptr;
	bool	errorRaised = false;
	int		baseTime = 0;
};
