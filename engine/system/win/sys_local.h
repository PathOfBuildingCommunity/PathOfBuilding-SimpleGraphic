// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// System Local Header
// Platform: Windows
//

#include "common.h"

#include "system.h"

#ifdef _WIN32
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shellapi.h>
#include <mmsystem.h>
#endif

#include <chrono>
#include <vector>

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

	int		KeyToVirtual(byte key);
	byte	VirtualToKey(int virt);
	byte	GlfwKeyToKey(int key);
	char	GlfwKeyExtraChar(int key);

#ifdef _WIN32
	HINSTANCE hinst = nullptr;
	HICON	icon = nullptr;
#endif

	class core_IMain* core = nullptr;

	bool	initialised = false;
	volatile bool	exitFlag = false;
	volatile bool	restartFlag = false;
	char*	exitMsg = nullptr;
	char*	threadError = nullptr;
	bool	errorRaised = false;
	std::chrono::system_clock::time_point baseTime;
	std::vector<uint8_t> heldKeyState;
};
