// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: System Main
// Platform: Windows
//

#include <fmt/chrono.h>

#include "sys_local.h"

#include "core.h"

#ifdef _WIN32
#include <eh.h>
#include <Shlobj.h>
#elif __APPLE__ && __MACH__
#include <libproc.h>
#endif

#ifndef _WIN32
#include <sys/types.h>
#include <pwd.h>
#include <uuid/uuid.h>
#endif

#include <GLFW/glfw3.h>
#include <filesystem>
#include <map>
#include <thread>

#include <fmt/core.h>

// ======
// Locals
// ======

#ifdef _WIN32
static void SE_ErrorTrans(unsigned int code, EXCEPTION_POINTERS* exPtr)
{
	throw exPtr;
}
#endif

// ===========
// Timer class
// ===========

timer_c::timer_c()
{}

void timer_c::Start()
{
	startTime = std::chrono::system_clock::now();
}

int timer_c::Get()
{
	auto curTime = std::chrono::system_clock::now();
	return (int)std::chrono::duration_cast<std::chrono::milliseconds>(curTime - startTime).count();
}

// ============
// Thread class
// ============

thread_c::thread_c(sys_IMain* sys)
{
	_sysMain = (sys_main_c*)sys;
}

unsigned long thread_c::statThreadProc(void* obj)
{
	thread_c* thread = (thread_c*)obj;
	try {
#ifdef _WIN32
		// Enable translation to catch C exceptions if debugger is not present
		if ( !thread->_sysMain->debuggerRunning ) {
			_set_se_translator(SE_ErrorTrans);
		}
#endif
		// Run thread procedure
		thread->ThreadProc();	
	}
#ifdef _WIN32
	catch (EXCEPTION_POINTERS* exPtr) {
		// C exception
		PEXCEPTION_RECORD exRec = exPtr->ExceptionRecord;
		DWORD code =  exRec->ExceptionCode;
		char detail[512];
		if (code == EXCEPTION_ACCESS_VIOLATION && exRec->NumberParameters == 2) {
			sprintf_s(detail, 512, "Access violation: attempted to %s address %08Xh", 
				exRec->ExceptionInformation[0]? "write to":"read from", static_cast<int>(exRec->ExceptionInformation[1]));
		} else if (code == EXCEPTION_STACK_OVERFLOW) {
			strcpy_s(detail, 512, "Stack overflow");
		} else {
			sprintf_s(detail, 512, "Error code: %08Xh", code);
		}
		char err[1024];
		sprintf_s(err, 1024, "Critical error at address %08Xh in thread %d:\n%s", static_cast<int>((ULONG_PTR)exRec->ExceptionAddress), GetCurrentThreadId(), detail);
		thread->_sysMain->threadError = AllocString(err);
	}
#else
	catch (std::exception& e) {
		thread->_sysMain->threadError = AllocString(fmt::format("Exception: {}", e.what()).c_str());
	}
#endif
	return 0;
}

void thread_c::ThreadStart(bool lowPri)
{
	std::thread t(statThreadProc, this);
#ifdef _WIN32
	HANDLE thr = t.native_handle();
	if (thr && lowPri) {
		SetThreadPriority(thr, THREAD_PRIORITY_BELOW_NORMAL);
	}
#endif
	t.detach();
}

// ===========
// File Finder
// ===========

find_c::find_c()
{}

find_c::~find_c()
{}

bool GlobMatch(const std::filesystem::path& glob, const std::filesystem::path& file) {
	// TODO(LV): Future-proof to handle more than /* and /*.ext
	if (glob == "*" || glob == "*.*") {
		return true;
	}
	if (glob.stem() == "*" && glob.has_extension() && file.has_extension()) {
		return glob.extension() == file.extension();
	}
	return glob == file;
}

bool find_c::FindFirst(const char* fileSpec)
{
	std::filesystem::path p(fileSpec);
	glob = p.filename();

	std::error_code ec;
	for (iter = std::filesystem::directory_iterator(p.parent_path(), ec); iter != std::filesystem::directory_iterator{}; ++iter) {
		if (GlobMatch(glob, *iter)) {
			fileName = iter->path().filename().string();
			isDirectory = iter->is_directory();
			fileSize = iter->file_size();
			auto mod = iter->last_write_time();
			modified = mod.time_since_epoch().count();
			return true;
		}
	}
	return false;
}

bool find_c::FindNext()
{
	if (iter == std::filesystem::directory_iterator{}) {
		return false;
	}

	for (++iter; iter != std::filesystem::directory_iterator{}; ++iter) {
		if (GlobMatch(glob, *iter)) {
			fileName = iter->path().filename().string();
			isDirectory = iter->is_directory();
			fileSize = iter->file_size();
			auto mod = iter->last_write_time();
			modified = mod.time_since_epoch().count();
			return true;
		}
	}
	return false;
}

// ===========
// Key Mapping
// ===========

byte KeyRemapGLFW(int key) {
	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
		return '0' + (key - GLFW_KEY_0);
	}
	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
		return 'a' + (key - GLFW_KEY_A);
	}
	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F15) {
		return KEY_F1 + (key - GLFW_KEY_F1);
	}
	switch (key) {
	case GLFW_KEY_BACKSPACE: return KEY_BACK;
	case GLFW_KEY_TAB: return KEY_TAB;
	case GLFW_KEY_ENTER: return KEY_RETURN;
	case GLFW_KEY_LEFT_SHIFT:
	case GLFW_KEY_RIGHT_SHIFT: return KEY_SHIFT;
	case GLFW_KEY_LEFT_CONTROL:
	case GLFW_KEY_RIGHT_CONTROL: return KEY_CTRL;
	case GLFW_KEY_LEFT_ALT:
	case GLFW_KEY_RIGHT_ALT: return KEY_ALT;
	case GLFW_KEY_PAUSE: return KEY_PAUSE;
	case GLFW_KEY_ESCAPE: return KEY_ESCAPE;
	case GLFW_KEY_SPACE: return ' ';
	case GLFW_KEY_PAGE_UP: return KEY_PGUP;
	case GLFW_KEY_PAGE_DOWN: return KEY_PGDN;
	case GLFW_KEY_END: return KEY_END;
	case GLFW_KEY_HOME: return KEY_HOME;
	case GLFW_KEY_LEFT: return KEY_LEFT;
	case GLFW_KEY_UP: return KEY_UP;
	case GLFW_KEY_RIGHT: return KEY_RIGHT;
	case GLFW_KEY_DOWN: return KEY_DOWN;
	case GLFW_KEY_PRINT_SCREEN: return KEY_PRINTSCRN;
	case GLFW_KEY_INSERT: return KEY_INSERT;
	case GLFW_KEY_DELETE: return KEY_DELETE;
	case GLFW_KEY_NUM_LOCK: return KEY_NUMLOCK;
	case GLFW_KEY_SCROLL_LOCK: return KEY_SCROLL;
	case GLFW_KEY_SEMICOLON: return ';';
	case GLFW_KEY_COMMA: return ',';
	case GLFW_KEY_MINUS: return '-';
	case GLFW_KEY_PERIOD: return '.';
	case GLFW_KEY_SLASH: return '/';
	case GLFW_KEY_GRAVE_ACCENT: return '`';
	case GLFW_KEY_LEFT_BRACKET: return '[';
	case GLFW_KEY_BACKSLASH: return '\\';
	case GLFW_KEY_RIGHT_BRACKET: return ']';
	case GLFW_KEY_APOSTROPHE: return '\'';
	}
	return 0;
}

static const byte sys_keyRemap[] = {
	0,				// 00		Null
	KEY_LMOUSE,		// 01		VK_LBUTTON
	KEY_RMOUSE,		// 02		VK_RBUTTON
	0,				// 03		VK_CANCEL
	KEY_MMOUSE,		// 04		VK_MBUTTON
	KEY_MOUSE4,		// 05		VK_XBUTTON1
	KEY_MOUSE5,		// 06		VK_XBUTTON2
	0,				// 07		-
	KEY_BACK,		// 08		VK_BACK
	KEY_TAB,		// 09		VK_TAB
	0,0,			// 0A 0B	-
	0,				// 0C		VK_CLEAR
	KEY_RETURN,		// 0D		VK_RETUN
	0,0,			// 0E 0F	-
	KEY_SHIFT,		// 10		VK_SHIFT
	KEY_CTRL,		// 11		VK_CONTROL
	KEY_ALT,		// 12		VK_MENU
	KEY_PAUSE,		// 13		VK_PAUSE
	0,				// 14		VK_CAPITAL
	0,0,0,0,0,		// 15-19	IME
	0,				// 1A		-
	KEY_ESCAPE,		// 1B		VK_ESCAPE
	0,0,0,0,		// 1C-1F	IME
	' ',			// 20		VK_SPACE
	KEY_PGUP,		// 21		VK_PRIOR
	KEY_PGDN,		// 22		VK_NEXT
	KEY_END,		// 23		VK_END
	KEY_HOME,		// 24		VK_HOME
	KEY_LEFT,		// 25		VK_LEFT
	KEY_UP,			// 26		VK_UP
	KEY_RIGHT,		// 27		VK_RIGHT
	KEY_DOWN,		// 28		VK_DOWN
	0,				// 29		VK_SELECT
	0,				// 2A		VK_PRINT
	0,				// 2B		VK_EXECUTE
	KEY_PRINTSCRN,	// 2C		VK_SNAPSHOT
	KEY_INSERT,		// 2D		VK_INSERT
	KEY_DELETE,		// 2E		VK_DELETE
	0,				// 2F		VK_HELP
	'0',			// 30		VK_0
	'1',			// 31		VK_1
	'2',			// 32		VK_2
	'3',			// 33		VK_3
	'4',			// 34		VK_4
	'5',			// 35		VK_5
	'6',			// 36		VK_6
	'7',			// 37		VK_7
	'8',			// 38		VK_8
	'9',			// 39		VK_9
	0,0,0,0,0,0,0,	// 3A-40	-
	'a',			// 41		VK_A
	'b',			// 42		VK_B
	'c',			// 43		VK_C
	'd',			// 44		VK_D
	'e',			// 45		VK_E
	'f',			// 46		VK_F
	'g',			// 47		VK_G
	'h',			// 48		VK_H
	'i',			// 49		VK_I
	'j',			// 4A		VK_J
	'k',			// 4B		VK_K
	'l',			// 4C		VK_L
	'm',			// 4D		VK_M
	'n',			// 4E		VK_N
	'o',			// 4F		VK_O
	'p',			// 50		VK_P
	'q',			// 51		VK_Q
	'r',			// 52		VK_R
	's',			// 53		VK_S
	't',			// 54		VK_T
	'u',			// 55		VK_U
	'v',			// 56		VK_V
	'w',			// 57		VK_W
	'x',			// 58		VK_X
	'y',			// 59		VK_Y
	'z',			// 5A		VK_Z
	0,				// 5B		VK_LWIN
	0,				// 5C		VK_RWIN
	0,				// 5D		VK_APPS
	0,				// 5E		-
	0,				// 5F		VK_SLEEP
	0,				// 60		VK_NUMPAD0
	0,				// 61		VK_NUMPAD1
	0,				// 62		VK_NUMPAD2
	0,				// 63		VK_NUMPAD3
	0,				// 64		VK_NUMPAD4
	0,				// 65		VK_NUMPAD5
	0,				// 66		VK_NUMPAD6
	0,				// 67		VK_NUMPAD7
	0,				// 68		VK_NUMPAD8
	0,				// 69		VK_NUMPAD9
	0,				// 6A		VK_MULTIPLY
	0,				// 6B		VK_ADD
	0,				// 6C		VK_SEPARATOR
	0,				// 6D		VK_SUBTRACT
	0,				// 6E		VK_DECIMAL
	0,				// 6F		VK_DIVIDE
	KEY_F1,			// 70		VK_F1
	KEY_F2,			// 71		VK_F2
	KEY_F3,			// 72		VK_F3
	KEY_F4,			// 73		VK_F4
	KEY_F5,			// 74		VK_F5
	KEY_F6,			// 75		VK_F6
	KEY_F7,			// 76		VK_F7
	KEY_F8,			// 77		VK_F8
	KEY_F9,			// 78		VK_F9
	KEY_F10,		// 79		VK_F10
	KEY_F11,		// 7A		VK_F11
	KEY_F12,		// 7B		VK_F12
	KEY_F13,		// 7C		VK_F13
	KEY_F14,		// 7D		VK_F14
	KEY_F15,		// 7E		VK_F15
	0,				// 7F		VK_F16
	0,				// 80		VK_F17
	0,				// 81		VK_F18
	0,				// 82		VK_F19
	0,				// 83		VK_F20
	0,				// 84		VK_F21
	0,				// 85		VK_F22
	0,				// 86		VK_F23
	0,				// 87		VK_F24
	0,0,0,0,0,0,0,0,// 88-8F	-
	KEY_NUMLOCK,	// 90		VK_NUMLOCK
	KEY_SCROLL,		// 91		VK_SCROLL
	0,0,0,0,0,		// 92-96	OEM
	0,0,0,0,0,0,0,0,0, // 97-9F	-
	0,				// A0		VK_LSHIFT
	0,				// A1		VK_RSHIFT
	0,				// A2		VK_LCONTROL
	0,				// A3		VK_RCONTROL
	0,				// A4		VK_LMENU
	0,				// A5		VK_RMENU
	0,				// A6		VK_BROWSER_BACK
	0,				// A7		VK_BROWSER_FORWARD
	0,				// A8		VK_BROWSER_REFRESH
	0,				// A9		VK_BROWSER_STOP
	0,				// AA		VK_BROWSER_SEARCH
	0,				// AB		VK_BROWSER_FAVORITES
	0,				// AC		VK_BROWSER_HOME
	0,				// AD		VK_VOLUME_MUTE
	0,				// AE		VK_VOLUME_DOWN
	0,				// AF		VK_VOLUME_UP
	0,				// B0		VK_MEDIA_NEXT_TRACK
	0,				// B1		VK_MEDIA_PREV_TRACK
	0,				// B2		VK_MEDIA_STOP
	0,				// B3		VK_MEDIA_PLAY_PAUSE
	0,				// B4		VK_LAUNCH_MAIL
	0,				// B5		VK_LAUNCH_MEDIA_SELECT
	0,				// B6		VK_LAUNCH_APP1
	0,				// B7		VK_LAUNCH_APP2
	0,0,			// B8,B9	-
	';',			// BA		VK_OEM_1
	'+',			// BB		VK_OEM_PLUS
	',',			// BC		VK_OEM_COMMA
	'-',			// BD		VK_OEM_MINUS
	'.',			// BE		VK_OEM_PERIOD
	'/',			// BF		VK_OEM_2
	'`',			// C0		VK_OEM_3
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // C1-D7 -
	0,0,0,			// D8-DA	-
	'[',			// DB		VK_OEM_4
	'\\',			// DC		VK_OEM_5
	']',			// DD		VK_OEM_6
	'\'',			// DE		VK_OEM_7
};

#ifdef _WIN32
int sys_main_c::KeyToVirtual(byte key)
{
	if (key >= 32 && key <= 127) {
		return toupper(key);
	}
	for (int v = 0; v <= VK_OEM_7; v++) {
		if (sys_keyRemap[v] == key) {
			return v;
		}
	}
	return 0;
}

byte sys_main_c::VirtualToKey(int virt)
{
	if (virt >= 0 && virt <= VK_OEM_7) {
		return sys_keyRemap[virt];
	} else {
		return 0;
	}
}
#endif

byte sys_main_c::GlfwKeyToKey(int key) {
	static std::map<int, byte> s_lookup = {
		{GLFW_KEY_BACKSPACE, KEY_BACK},
		{GLFW_KEY_TAB, KEY_TAB},
		{GLFW_KEY_ENTER, KEY_RETURN},
		{GLFW_KEY_LEFT_SHIFT, KEY_SHIFT},
		{GLFW_KEY_RIGHT_SHIFT, KEY_SHIFT},
		{GLFW_KEY_LEFT_CONTROL, KEY_CTRL},
		{GLFW_KEY_RIGHT_CONTROL, KEY_CTRL},
		{GLFW_KEY_LEFT_ALT, KEY_ALT},
		{GLFW_KEY_RIGHT_ALT, KEY_ALT},
		{GLFW_KEY_PAUSE, KEY_PAUSE},
		{GLFW_KEY_ESCAPE, KEY_ESCAPE},
		{GLFW_KEY_SPACE, ' '},
		{GLFW_KEY_PAGE_UP, KEY_PGUP},
		{GLFW_KEY_PAGE_DOWN, KEY_PGDN},
		{GLFW_KEY_END, KEY_END},
		{GLFW_KEY_HOME, KEY_HOME},
		{GLFW_KEY_LEFT, KEY_LEFT},
		{GLFW_KEY_UP, KEY_UP},
		{GLFW_KEY_RIGHT, KEY_RIGHT},
		{GLFW_KEY_DOWN, KEY_DOWN},
		{GLFW_KEY_PRINT_SCREEN, KEY_PRINTSCRN},
		{GLFW_KEY_INSERT, KEY_INSERT},
		{GLFW_KEY_DELETE, KEY_DELETE},
		{GLFW_KEY_NUM_LOCK, KEY_NUMLOCK},
		{GLFW_KEY_SCROLL_LOCK, KEY_SCROLL},
		{GLFW_KEY_SEMICOLON, ';'},
		// GLFW defines no plus key
		{GLFW_KEY_EQUAL, '+'},
		{GLFW_KEY_COMMA, ','},
		{GLFW_KEY_MINUS, '-'},
		{GLFW_KEY_PERIOD, '.'},
		{GLFW_KEY_SLASH, '/'},
		{GLFW_KEY_GRAVE_ACCENT, '`'},
		{GLFW_KEY_LEFT_BRACKET, '['},
		{GLFW_KEY_BACKSLASH, '\\'},
		{GLFW_KEY_RIGHT_BRACKET, ']'},
		{GLFW_KEY_APOSTROPHE, '\''},
	};

	auto I = s_lookup.find(key);
	if (I != s_lookup.end()) {
		return I->second;
	}

	if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F15) {
		return KEY_F1 + (key - GLFW_KEY_F1);
	}

	if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
		return '0' + (key - GLFW_KEY_0);
	}

	if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
		return 'a' + (key - GLFW_KEY_A);
	}

	return 0;
}

char sys_main_c::GlfwKeyExtraChar(int key) {
	static std::map<int, byte> s_lookup = {
		{GLFW_KEY_BACKSPACE, 0x8},
		{GLFW_KEY_TAB, 0x9},
		{GLFW_KEY_ENTER, 0xd},
		{GLFW_KEY_ESCAPE, 0x1b},
	};

	auto I = s_lookup.find(key);
	if (I != s_lookup.end()) {
		return I->second;
	}

	return 0;
}

// =====================
// Misc System Functions
// =====================

int sys_main_c::GetTime()
{
	auto curTime = std::chrono::system_clock::now();
	return (int)std::chrono::duration_cast<std::chrono::milliseconds>(curTime - baseTime).count();
}

void sys_main_c::Sleep(int msec)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(msec));
}

bool sys_main_c::IsKeyDown(byte k)
{
#ifdef _WIN32
	return !!(GetKeyState(KeyToVirtual(k)) & 0x8000);
#else
	// TODO(LV): Implement on other OSes
	return false;
#endif
}

void sys_main_c::ClipboardCopy(const char* str)
{
	glfwSetClipboardString(nullptr, str);
}

char* sys_main_c::ClipboardPaste()
{
	return AllocString(glfwGetClipboardString(nullptr));
}

bool sys_main_c::SetWorkDir(const char* newCwd)
{
	if (newCwd) {
		return _chdir(newCwd) != 0;
	} else {
		return _chdir(basePath.c_str()) != 0;
	}
}

void sys_main_c::SpawnProcess(const char* cmdName, const char* argList)
{
#ifdef _WIN32
	char cmd[512];
	strcpy(cmd, cmdName);
	if ( !strchr(cmd, '.') ) {
		strcat(cmd, ".exe");
	}
	SHELLEXECUTEINFO sinfo;
	memset(&sinfo, 0, sizeof(SHELLEXECUTEINFO));
	sinfo.cbSize       = sizeof(SHELLEXECUTEINFO);
	sinfo.fMask        = SEE_MASK_NOCLOSEPROCESS;
	sinfo.lpFile       = cmd;
	sinfo.lpParameters = argList;
	sinfo.lpVerb       = "open";
	sinfo.nShow        = SW_SHOWMAXIMIZED;
	if ( !ShellExecuteEx(&sinfo) ) {
		sinfo.lpVerb = "runas";
		ShellExecuteEx(&sinfo);
	}
#else
	// TODO(LV): Implement subprocesses for other OSes.
#endif
}

void sys_main_c::OpenURL(const char* url)
{
#ifdef _WIN32
	ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWDEFAULT);
#else
	// TODO(LV): Implement URL opening for other OSes.
#endif
}

// ==============================
// System Initialisation/Shutdown
// ==============================

void sys_main_c::Error(const char *fmt, ...)
{
	if (errorRaised) return;
	errorRaised = true;
	
	if (initialised) {
		video->SetVisible(false);
		conWin->SetVisible(true);
	}

	va_list va;
	va_start(va, fmt);
#ifdef _WIN32
	char msg[4096];
	vsprintf_s(msg, 4096, fmt, va);
#else
	char* msg{};
	vasprintf(&msg, fmt, va);
#endif
	va_end(va);
	con->Printf("\n--- ERROR ---\n%s", msg);
#ifndef _WIN32
	free(msg);
#endif

	exitFlag = false;
	while (exitFlag == false) {
		Sleep(50);
	}

#ifdef _MEMTRAK_H
	_memTrak_suppressReport = true;
#endif
#ifdef _WIN32
	ExitProcess(0);
#else
	std::exit(0);
#endif
}

void sys_main_c::Exit(const char* msg)
{
	if (initialised) {
		video->SetVisible(false);
	}
	FreeString(exitMsg);
	exitMsg = msg? AllocString(msg) : NULL;
	if (exitMsg) {
		conWin->SetVisible(true);
	}
	exitFlag = true;
}

void sys_main_c::Restart()
{
	video->SetVisible(false);
	conWin->SetVisible(true);
	restartFlag = true;
	FreeString(exitMsg);
	exitMsg = NULL;
	exitFlag = true;
}

std::string FindBasePath()
{
#ifdef _WIN32
	std::vector<char> basePath(512);
	GetModuleFileNameA(NULL, basePath.data(), basePath.size());
	*strrchr(basePath.data(), '\\') = 0;
	return basePath.data();
#elif __linux__
#elif __APPLE__ && __MACH__
	pid_t pid = getpid();
	char basePath[PROC_PIDPATHINFO_MAXSIZE]{};
	proc_pidpath(pid, basePath, sizeof(basePath));
	*strrchr(basePath, '/') = 0;
	return basePath;
#endif
}

std::string FindUserPath()
{
#ifdef _WIN32
    PWSTR os_path{};
    SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &os_path);
    std::filesystem::path path(os_path);
    CoTaskMemFree(os_path);
    return path.string();
#else
    if (char const* data_home_path = getenv("XDG_DATA_HOME")) {
        return data_home_path;
    }
    if (char const* home_path = getenv("HOME")) {
        return std::filesystem::path(home_path) / ".local/share";
    }
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    return std::filesystem::path(pw->pw_dir) / ".local/share";
#endif
}

sys_main_c::sys_main_c()
{
#ifdef _WIN64
	x64 = true;
#else
	x64 = false;
#endif
#ifdef _DEBUG
	debug = true;
#else
	debug = false;
#endif
#ifdef _WIN32
	debuggerRunning = IsDebuggerPresent() == TRUE;
#else
	debuggerRunning = false;
#endif
	processorCount = std::thread::hardware_concurrency();

	// Set the local system information
	basePath = FindBasePath();
	userPath = FindUserPath();
}

bool sys_main_c::Run(int argc, char** argv)
{
	initialised = false;
	exitFlag = false;
	restartFlag = false;
	exitMsg = NULL;
	threadError = NULL;
	errorRaised = false;
	baseTime = std::chrono::system_clock::now();

	SetWorkDir();

	// Get system interfaces
	con = IConsole::GetHandle();
	conWin = sys_IConsole::GetHandle(this);
	video = sys_IVideo::GetHandle(this);
	core = core_IMain::GetHandle(this);

	// Print some handy information
	con->Printf(CFG_VERSION" %s %s, built " __DATE__ "\n", x64? "x64":"x86", debug? "Debug":"Release");
	if (debuggerRunning) {
		con->Printf("Debugger is present.\n");
	}
	con->Printf("\n");

	initialised = true;

	try {
#ifdef _WIN32
		// Enable translation to catch C exceptions if debugger is not present
		if ( !debuggerRunning ) {
			_set_se_translator(SE_ErrorTrans);
		}
#endif

		// Initialise engine
		core->Init(argc, argv);

		// Run frame loop
		while (exitFlag == false) {
			glfwPollEvents();
			auto wnd = (GLFWwindow*)video->GetWindowHandle();
			if (glfwWindowShouldClose(wnd)) {
				Exit();
				break;
			}

			core->Frame();

			if (threadError) {
				Error(threadError);
			}
		}

		// Shutdown engine
		core->Shutdown();
	}
#ifdef _WIN32
	catch (EXCEPTION_POINTERS* exPtr) { 
		// C exception
		PEXCEPTION_RECORD exRec = exPtr->ExceptionRecord;
		DWORD code =  exRec->ExceptionCode;
		char detail[512];
		if (code == EXCEPTION_ACCESS_VIOLATION && exRec->NumberParameters == 2) {
			sprintf_s(detail, 512, "Access violation: attempted to %s address %08Xh", 
				exRec->ExceptionInformation[0]? "write to":"read from", static_cast<int>(exRec->ExceptionInformation[1]));
		} else if (code == EXCEPTION_STACK_OVERFLOW) {
			strcpy_s(detail, 512, "Stack overflow");
		} else {
			sprintf_s(detail, 512, "Error code: %08Xh", code);
		}
		Error("Critical error at address %08Xh:\n%s", static_cast<int>((ULONG_PTR)exRec->ExceptionAddress), detail);
	}
#else
	catch (std::exception& e) {
		Error("Exception: ", e.what());
	}
#endif

	if (exitMsg) {
		exitFlag = false;
		video->SetVisible(false);
		conWin->SetVisible(true);
		if (exitMsg) {
			con->Printf("\n%s", exitMsg);
			FreeString(exitMsg);
			exitMsg = NULL;
		}
		while (exitFlag == false) {
			Sleep(50);
		}
	}	

	initialised = false;

	// Release system interfaces
	core_IMain::FreeHandle(core);
	sys_IVideo::FreeHandle(video);
	sys_IConsole::FreeHandle(conWin);
	IConsole::FreeHandle(con);

	return restartFlag;
}
