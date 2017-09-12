// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: System Console
// Platform: Windows
//

#include "sys_local.h"

// =============
// Configuration
// =============

#define SCON_WIDTH 550
#define SCON_HEIGHT 500

#define SCON_STYLE WS_VISIBLE | WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN

// ======================
// sys_IConsole Interface
// ======================

class sys_console_c: public sys_IConsole, public conPrintHook_c, public thread_c {
public:
	// Interface
	void	SetVisible(bool show);
	void	SetTitle(const char* title);

	// Encapsulated
	sys_console_c(sys_IMain* sysHnd);
	~sys_console_c();

	sys_main_c* sys;

	bool	shown;		// Is currently shown?
	HWND	hwMain;		// Main window
	HWND	hwOut;		// Output
	HFONT	font;		// Font handle
	HBRUSH	bBackground;// Background brush

	static LRESULT __stdcall WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	volatile bool doRun;
	volatile bool isRunning;

	void	ThreadProc();

	void	Print(const char* text);
	void	CopyToClipboard();

	void	ConPrintHook(const char* text);
	void	ConPrintClear();
};

sys_IConsole* sys_IConsole::GetHandle(sys_IMain* sysHnd)
{
	return new sys_console_c(sysHnd);
}

void sys_IConsole::FreeHandle(sys_IConsole* hnd)
{
	delete (sys_console_c*)hnd;
}

sys_console_c::sys_console_c(sys_IMain* sysHnd)
	: conPrintHook_c(sysHnd->con), sys((sys_main_c*)sysHnd), thread_c(sysHnd)
{
	isRunning = false;
	doRun = true;

	ThreadStart(true);
	while ( !isRunning );
}

void sys_console_c::ThreadProc()
{
	// Get info of the monitor containing the mouse cursor
	POINT curPos;
	GetCursorPos(&curPos);
	HMONITOR hMon = MonitorFromPoint(curPos, MONITOR_DEFAULTTOPRIMARY);
	MONITORINFO monInfo;
	monInfo.cbSize = sizeof(monInfo);
	GetMonitorInfo(hMon, &monInfo);

	// Find the window position and size
	RECT wrec;
	wrec.left = (monInfo.rcMonitor.right + monInfo.rcMonitor.left - SCON_WIDTH) / 2;
	wrec.top = (monInfo.rcMonitor.bottom + monInfo.rcMonitor.top - SCON_HEIGHT) / 2;
	wrec.right = wrec.left + SCON_WIDTH;
	wrec.bottom = wrec.top + SCON_HEIGHT;
	AdjustWindowRect(&wrec, SCON_STYLE, FALSE);

	// Register the class
	WNDCLASS conClass;
	memset(&conClass, 0, sizeof(WNDCLASS));
	conClass.hInstance			= sys->hinst;
	conClass.lpszClassName		= CFG_SCON_TITLE " Class";
	conClass.lpfnWndProc		= WndProc;
	conClass.hbrBackground		= CreateSolidBrush(CFG_SCON_WINBG);
	conClass.hCursor			= LoadCursor(NULL, IDC_ARROW);
	conClass.hIcon				= sys->icon;
	if (RegisterClass(&conClass) == 0) exit(0);

	// Create the system console window
	hwMain = CreateWindowEx(
		0, CFG_SCON_TITLE " Class", CFG_SCON_TITLE, SCON_STYLE, 
		wrec.left, wrec.top, wrec.right - wrec.left, wrec.bottom - wrec.top,
		NULL, NULL, sys->hinst, NULL
	);
	SetWindowLongPtr(hwMain, GWLP_USERDATA, (LONG_PTR)this);

	// Populate window
	hwOut = CreateWindowEx(
		WS_EX_CLIENTEDGE, "EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
		10, 10, SCON_WIDTH - 20, SCON_HEIGHT - 20, 
		hwMain, NULL, sys->hinst, NULL
	);

	// Create the output window font
	font = CreateFont(
		12, 0, 0, 0, 
		FW_LIGHT, FALSE, FALSE, FALSE, 
		DEFAULT_CHARSET, 
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
		FIXED_PITCH|FF_MODERN, "Lucida Console"
	);
	SetWindowFont(hwOut, font, FALSE);
	Edit_LimitText(hwOut, 0xFFFF);

	// Create the output window background brush
	bBackground = CreateSolidBrush(CFG_SCON_TEXTBG);

	// Flush any messages created
	sys->RunMessages();

	shown = true;

	InstallPrintHook();
	
	isRunning = true;
	while (doRun) {
		sys->RunMessages(hwMain);
		sys->Sleep(1);
	}

	RemovePrintHook();

	// Delete font and brush
	DeleteObject(bBackground);
	DeleteObject(font);

	// Destroy window
	DestroyWindow(hwMain);
	UnregisterClass(CFG_SCON_TITLE " Class", sys->hinst);

	isRunning = false;
}

sys_console_c::~sys_console_c()
{
	doRun = false;
	while (isRunning);
}

// ========================
// Console Window Procedure
// ========================

LRESULT __stdcall sys_console_c::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	sys_console_c* conWin = (sys_console_c*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg) {
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORSTATIC:
		// Set text color of output window
		SetTextColor((HDC)wParam, CFG_SCON_TEXTCOL);
		SetBkColor((HDC)wParam, CFG_SCON_TEXTBG);
		return (LRESULT)conWin->bBackground;

	case WM_KEYUP:
		if (wParam != VK_ESCAPE) {
			break;
		}
	case WM_CLOSE:
		// Quit
		PostQuitMessage(0);
		return FALSE;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ==============
// System Console
// ==============

void sys_console_c::SetVisible(bool show)
{
	if (show) {
		if ( !shown ) {
			shown = true;

			// Show the window
			ShowWindow(hwMain, SW_SHOW);
			SetForegroundWindow(hwMain);

			// Select all text and replace with full text
			Edit_SetText(hwOut, "");
			char* buffer = sys->con->BuildBuffer();
			Print(buffer);
			delete buffer;
	
			sys->RunMessages(hwMain);
		}
	} else {
		shown = false;
	
		// Hide the window
		ShowWindow(hwMain, SW_HIDE);
	}
}

void sys_console_c::SetTitle(const char* title)
{
	SetWindowText(hwMain, (title && *title)? title : CFG_SCON_TITLE);
}

void sys_console_c::Print(const char* text)
{
	if ( !shown ) {
		return;
	}

	int escLen;

	// Find the required buffer length
	int len = 0;
	for (int b = 0; text[b]; b++) {
		if (text[b] == '\n') {
			// Newline takes 2 characters
			len+= 2;
		} else if (escLen = IsColorEscape(&text[b])) {
			// Skip colour escapes
			b+= escLen - 1;
		} else {
			len++;
		}
	}

	// Parse into the buffer
	char* winText = AllocStringLen(len);
	char* p = winText;
	for (int b = 0; text[b]; b++) {
		if (text[b] == '\n') {
			// Append newline
			*(p++) = '\r';
			*(p++) = '\n';
		} else if (escLen = IsColorEscape(&text[b])) {
			// Skip colour escapes
			b+= escLen - 1;
		} else {
			// Add character
			*(p++) = text[b];
		}
	}

	// Append to the output
	Edit_SetSel(hwOut, Edit_GetTextLength(hwOut), -1);
	Edit_ReplaceSel(hwOut, winText);
	Edit_Scroll(hwOut, 0xFFFF, 0);
	sys->RunMessages(hwMain);
	delete winText;
}

void sys_console_c::CopyToClipboard()
{
	int len = GetWindowTextLength(hwOut);
	if (len) {
		HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len + 1);
		if ( !hg ) return;
		char* cp = (char*)GlobalLock(hg);
		GetWindowText(hwOut, cp, len + 1);
		GlobalUnlock(hg);
		OpenClipboard(hwMain);
		EmptyClipboard();
		SetClipboardData(CF_TEXT, hg);
		CloseClipboard();
	}
}

void sys_console_c::ConPrintHook(const char* text)
{
	Print(text);
}

void sys_console_c::ConPrintClear()
{
	Edit_SetText(hwOut, "");
}
