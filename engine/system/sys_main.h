// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// System Main Header
//

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
	static int atomicInc(volatile int* val);
	static int atomicDec(volatile int* val);
	static int atomicExch(volatile int* out, int val);
private:
	class sys_main_c* _sysMain;
	virtual void ThreadProc() = 0;
	static unsigned long __stdcall statThreadProc(void* obj);
};

// File finder
class find_c {
public:
	char	fileName[512];
	bool	isDirectory;
	dword	fileSize;
	unsigned long long modified;
	char	modifiedDate[256];
	char	modifiedTime[256];

	find_c();
	~find_c();
	bool	FindFirst(const char* fileSpec);
	bool	FindNext();
private:
	void*	handle;
};

// ==========
// Interfaces
// ==========

// System Main
class sys_IMain {
public:
	IConsole* con;
	sys_IConsole* conWin;
	sys_IVideo* video;

	bool	x64;
	bool	debug;
	bool	debuggerRunning;
	int		processorCount;
	char	basePath[512];
	char	userPath[512];

	virtual int		GetTime() = 0;
	virtual void	Sleep(int msec) = 0;
	virtual bool	IsKeyDown(byte key) = 0;
	virtual void	ShowCursor(int doShow) = 0;
	virtual void	ClipboardCopy(const char* str) = 0;
	virtual char*	ClipboardPaste() = 0;
	virtual bool	SetWorkDir(const char* newCwd = NULL) = 0;
	virtual void	SpawnProcess(const char* cmdName, const char* argList) = 0;
	virtual void	OpenURL(const char* url) = 0;
	virtual void	Error(char* fmt, ...) = 0;
	virtual void	Exit(const char* msg = NULL) = 0;
	virtual void	Restart() = 0;
};
