// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// System Main Header
//

#include <filesystem>
#include <string>

// =======
// Classes
// =======

// Timer
class timer_c {
public:
	timer_c();
	void	Start();
	int		Get();
private:
	int		startTime;
};

// Thread
class thread_c {
public:
	thread_c(class sys_IMain* sys);
	void	ThreadStart(bool lowPri = false);
private:
	class sys_main_c* _sysMain;
	virtual void ThreadProc() = 0;
	static unsigned long statThreadProc(void* obj);
};

// File finder
class find_c {
public:
	std::string fileName;
	bool	isDirectory = false;
	uintmax_t	fileSize = 0;
	unsigned long long modified = 0;

	find_c();
	~find_c();
	bool	FindFirst(const char* fileSpec);
	bool	FindNext();
private:
	std::filesystem::path glob;
	std::filesystem::directory_iterator iter;
};

// ==========
// Interfaces
// ==========

// System Main
class sys_IMain {
public:
	IConsole* con = nullptr;
	sys_IConsole* conWin = nullptr;
	sys_IVideo* video = nullptr;

	bool	x64 = false;
	bool	debug = false;
	bool	debuggerRunning = false;
	int		processorCount = 0;
	char	basePath[512] = {};
	char	userPath[512] = {};

	virtual int		GetTime() = 0;
	virtual void	Sleep(int msec) = 0;
	virtual bool	IsKeyDown(byte key) = 0;
	virtual void	ShowCursor(int doShow) = 0;
	virtual void	ClipboardCopy(const char* str) = 0;
	virtual char*	ClipboardPaste() = 0;
	virtual bool	SetWorkDir(const char* newCwd = NULL) = 0;
	virtual void	SpawnProcess(const char* cmdName, const char* argList) = 0;
	virtual void	OpenURL(const char* url) = 0;
	virtual void	Error(const char* fmt, ...) = 0;
	virtual void	Exit(const char* msg = NULL) = 0;
	virtual void	Restart() = 0;
};
