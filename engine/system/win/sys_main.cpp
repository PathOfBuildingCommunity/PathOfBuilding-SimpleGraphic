// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: System Main
// Platform: Windows
//

#include <fmt/chrono.h>
#include <re2/re2.h>

#include "sys_local.h"

#include "core.h"

#ifdef _WIN32
#include <eh.h>
#include <Shlobj.h>
#elif __linux__
#include <unistd.h>
#include <limits.h>
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
#include <set>
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
#define GLFW_HAS_GET_KEY_NAME 1

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

bool GlobMatch(const std::filesystem::path& glob, const std::filesystem::path& file)
{
	using namespace std::literals::string_view_literals;
	auto globStr = glob.generic_u8string();

	// Deal with traditional "everything" wildcards.
	if (glob == "*" || glob == "*.*") {
		return true;
	}

	fmt::memory_buffer buf;

	// If no wildcards are present, test file path verbatim.
	// We use a regex rather than string comparisons to make it case-insensitive.
	if (globStr.find_first_of("?*") == std::string::npos) {
		buf.reserve(globStr.size() * 3); // Decent estimate of final pattern size.

		for (char ch : globStr) {
			fmt::format_to(fmt::appender(buf), "[{}]", ch);
		}
	}
	else {
		// Otherwise build a regular expression from the glob and use that to match files.
		auto it = fmt::appender(buf);
		for (char ch : globStr) {
			if (ch == '*') {
				it = fmt::format_to(it, ".*");
			}
			else if (ch == '?') {
				*it++ = '.';
			}
			else if ("+[]{}+()|"sv.find(ch) != std::string_view::npos) {
				// Escape metacharacters
				it = fmt::format_to(it, "\\{}", ch);
			}
			else if (std::isalnum((unsigned char)ch)) {
				*it++ = ch;
			}
			else {
				it = fmt::format_to(it, "[{}]", ch);
			}
		}
	}

	// Assume case-insensitive comparisons are desired.
	RE2::Options reOpts;
	reOpts.set_case_sensitive(false);
	RE2 reGlob{to_string(buf), reOpts};

	auto fileStr = file.generic_u8string();
	return RE2::FullMatch(fileStr, reGlob);
}

bool find_c::FindFirst(const char* fileSpec)
{
#ifdef _WIN32
	wchar_t* wideSpec = WidenUTF8String(fileSpec);
	std::filesystem::path p(wideSpec);
	FreeWideString(wideSpec);
#else
	std::filesystem::path p(fileSpec);
#endif
	glob = p.filename();

	std::error_code ec;
	for (iter = std::filesystem::directory_iterator(p.parent_path(), ec); iter != std::filesystem::directory_iterator{}; ++iter) {
		auto candFilename = iter->path().filename();
		if (GlobMatch(glob, candFilename)) {
			fileName = candFilename.u8string();
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
		auto candFilename = iter->path().filename();
		if (GlobMatch(glob, candFilename)) {
			fileName = candFilename.u8string();
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


static int ImGui_ImplGlfw_TranslateUntranslatedKey(int key, int scancode)
{
#if GLFW_HAS_GET_KEY_NAME
	// GLFW 3.1+ attempts to "untranslate" keys, which goes the opposite of what every other framework does, making using lettered shortcuts difficult.
	// (It had reasons to do so: namely GLFW is/was more likely to be used for WASD-type game controls rather than lettered shortcuts, but IHMO the 3.1 change could have been done differently)
	// See https://github.com/glfw/glfw/issues/1502 for details.  
	// Adding a workaround to undo this (so our keys are translated->untranslated->translated, likely a lossy process).
	// This won't cover edge cases but this is at least going to cover common cases.
	if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL)
		return key;
	const char* key_name = glfwGetKeyName(key, scancode);
	if (key_name && key_name[0] != 0 && key_name[1] == 0)
	{
		const char char_names[] = "`-=[]\\,;\'./";
		const int char_keys[] = { GLFW_KEY_GRAVE_ACCENT, GLFW_KEY_MINUS, GLFW_KEY_EQUAL, GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET, GLFW_KEY_BACKSLASH, GLFW_KEY_COMMA, GLFW_KEY_SEMICOLON, GLFW_KEY_APOSTROPHE, GLFW_KEY_PERIOD, GLFW_KEY_SLASH, 0 };
		if (key_name[0] >= '0' && key_name[0] <= '9') { key = GLFW_KEY_0 + (key_name[0] - '0'); }
		else if (key_name[0] >= 'A' && key_name[0] <= 'Z') { key = GLFW_KEY_A + (key_name[0] - 'A'); }
		else if (key_name[0] >= 'a' && key_name[0] <= 'z') { key = GLFW_KEY_A + (key_name[0] - 'a'); }
		else if (const char* p = strchr(char_names, key_name[0])) { key = char_keys[p - char_names]; }
	}
#endif
	return key;
}

byte sys_main_c::GlfwKeyToKey(int key, int scancode) {
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
		{GLFW_KEY_KP_0, '0'},
		{GLFW_KEY_KP_SUBTRACT, '-'},
		{GLFW_KEY_KP_ADD, '+'},
		{GLFW_KEY_KP_ENTER, KEY_RETURN},
	};

	key = ImGui_ImplGlfw_TranslateUntranslatedKey(key, scancode);

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
	if (k < heldKeyState.size()) {
		return !!heldKeyState[k];
	}
	return false;
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
	SHELLEXECUTEINFOA sinfo;
	memset(&sinfo, 0, sizeof(SHELLEXECUTEINFOA));
	sinfo.cbSize       = sizeof(SHELLEXECUTEINFOA);
	sinfo.fMask        = SEE_MASK_NOCLOSEPROCESS;
	sinfo.lpFile       = cmd;
	sinfo.lpParameters = argList;
	sinfo.lpVerb       = "open";
	sinfo.nShow        = SW_SHOWMAXIMIZED;
	if ( !ShellExecuteExA(&sinfo) ) {
		sinfo.lpVerb = "runas";
		ShellExecuteExA(&sinfo);
	}
#else
#warning LV: Subprocesses not implemented on this OS.
	// TODO(LV): Implement subprocesses for other OSes.
#endif
}

#if _WIN32 || __linux__
void PlatformOpenURL(const char* url)
{
#ifdef _WIN32
	ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWDEFAULT);
#else
#warning LV: URL opening not implemented on this OS.
	// TODO(LV): Implement URL opening for other OSes.
#endif
}
#else
void PlatformOpenURL(const char* url);
#endif

void sys_main_c::OpenURL(const char* url)
{
	PlatformOpenURL(url);
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
	char basePath[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", basePath, sizeof(basePath));
    if (len == -1 || len == sizeof(basePath))
    	len = 0;
	basePath[len] = '\0';
	*strrchr(basePath, '/') = 0;
	return basePath;
#elif __APPLE__ && __MACH__
	pid_t pid = getpid();
	char basePath[PROC_PIDPATHINFO_MAXSIZE]{};
	proc_pidpath(pid, basePath, sizeof(basePath));
	*strrchr(basePath, '/') = 0;
	return basePath;
#endif
}

std::optional<std::string> FindUserPath(std::optional<std::string>& invalidPath)
{
#ifdef _WIN32
    PWSTR osPath{};
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &osPath);
	if (FAILED(hr)) {
		// The path may be inaccessible due to malfunctioning cloud providers.
		CoTaskMemFree(osPath);
		invalidPath.reset();
		return {};
	}
	std::wstring pathStr = osPath;
	CoTaskMemFree(osPath);
	std::filesystem::path path(pathStr);
	try {
    	return path.string();
	}
	catch (std::system_error) {
		// The path could not be converted into the narrow representation, convert the path
		// string lossily for use in an ASCII error message on the Lua side.
		invalidPath.reset();
		std::wstring pathStr = path.wstring();
		char defGlyph = '?';
		BOOL defGlyphUsed{};
		DWORD convFlags = WC_COMPOSITECHECK | WC_NO_BEST_FIT_CHARS | WC_DEFAULTCHAR;
		int cb = WideCharToMultiByte(CP_ACP, 0, pathStr.c_str(), -1, nullptr, 0, &defGlyph, &defGlyphUsed);
		if (cb) {
			std::vector<char> buf(cb);
			WideCharToMultiByte(CP_ACP, 0, pathStr.c_str(), -1, buf.data(), cb, &defGlyph, &defGlyphUsed);
			for (auto& ch : buf) {
				if ((unsigned char)ch >= 0x80) {
					ch = '?'; // Substitute characters that we can represent but can't draw.
				}
			}
			invalidPath = buf.data();
		}
		return {};
	}
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
	: heldKeyState(KEY_SCROLL + 1, (uint8_t)0)
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
	userPath = FindUserPath(invalidUserPath);
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
			if (minimized) {
				glfwWaitEventsTimeout(0.1);
			}
			else {
				glfwPollEvents();
			}
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
