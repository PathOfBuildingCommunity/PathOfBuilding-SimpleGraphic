// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Module: System Video
// Platform: Windows
//

#include "sys_local.h"

// ====================
// sys_IVideo Interface
// ====================

class sys_video_c: public sys_IVideo {
public:
	// Interface
	int		Apply(sys_vidSet_s* set);

	void	SetActive(bool active);
	bool	IsActive();
	void	SizeChanged(int width, int height, bool max);
	void	PosChanged(int x, int y);
	void	GetMinSize(int &width, int &height);
	void	SetVisible(bool vis);
	bool	IsVisible();
	void	SetTitle(const char* title);
	void*	GetWindowHandle();
	void	GetRelativeCursor(int &x, int &y);
	void	SetRelativeCursor(int x, int y);
	bool	IsCursorOverWindow();

	// Encapsulated
	sys_video_c(sys_IMain* sysHnd);
	~sys_video_c();

	sys_main_c* sys = nullptr;

	bool	initialised = false;
	HWND	hwnd = nullptr;			// Window handle

	static BOOL __stdcall MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT rect, LPARAM data);
	bool	AddMonitor(HMONITOR hMonitor);
	void	RefreshMonitorInfo();

	int		numMon = 0;			// Number of monitors
	int		priMon = 0;			// Index of primary monitor
	struct {
		HMONITOR hnd = nullptr;
		int		left = 0;
		int		top = 0;
		int		width = 0;
		int		height = 0;
		char	devName[CCHDEVICENAME] = {};
	} mon[16];					// Array of monitor specs

	int		defRes[2] = {};		// Default resolution
	sys_vidSet_s cur;			// Current settings
	int		scrSize[2] = {};	// Screen size
	int		minSize[2] = {};	// Minimum window size
};

sys_IVideo* sys_IVideo::GetHandle(sys_IMain* sysHnd)
{
	return new sys_video_c(sysHnd);
}

void sys_IVideo::FreeHandle(sys_IVideo* hnd)
{
	delete (sys_video_c*)hnd;
}

sys_video_c::sys_video_c(sys_IMain* sysHnd)
	: sys((sys_main_c*)sysHnd)
{
	initialised = false;

	minSize[0] = minSize[1] = 0;

	// Register the window class
	WNDCLASS wndClass;
	ZeroMemory(&wndClass, sizeof(WNDCLASS));
	wndClass.lpszClassName	= CFG_TITLE " Class";
	wndClass.hInstance		= sys->hinst;
	wndClass.lpfnWndProc	= sys_main_c::WndProc;
	wndClass.hbrBackground	= CreateSolidBrush(CFG_SCON_WINBG);
	wndClass.hIcon			= sys->icon;
	wndClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wndClass.style			= CS_DBLCLKS;
	if (RegisterClass(&wndClass) == 0) {
		sys->Error("Unable to register main window class");
	}
	// Create the window
	hwnd = CreateWindowEx(
		0, CFG_TITLE " Class", CFG_TITLE, WS_POPUP,/* | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX | WS_MAXIMIZEBOX, */
		0, 0, 500, 500,
		NULL, NULL, sys->hinst, NULL
	);
	if (hwnd == NULL) {
		sys->Error("Unable to create window");
	}
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)sys);
	// Process any messages generated during creation
	sys->RunMessages();
}

sys_video_c::~sys_video_c()
{
	// Destroy the window
	DestroyWindow(hwnd);
	
	// Unregister the window class
	UnregisterClass(CFG_TITLE " Class", sys->hinst);

	if (initialised && (cur.flags & VID_FULLSCREEN)) {
		// Reset resolution
		ChangeDisplaySettingsEx(mon[cur.display].devName, NULL, NULL, 0, NULL);
	}
}

// ==================
// System Video Class
// ==================

int sys_video_c::Apply(sys_vidSet_s* set)
{
	cur = *set;

	// Enumerate monitors
	priMon = -1;
	numMon = 0;
	EnumDisplayMonitors(NULL, NULL, &sys_video_c::MonitorEnumProc, (LPARAM)this);

	// Determine which monitor to create window on
	if (cur.display >= numMon) {
		sys->con->Warning("display #%d doesn't exist (max display number is %d)", cur.display, numMon - 1);
		cur.display = 0;
	} else if (cur.display < 0) {
		// Use monitor containing the mouse cursor 
		POINT curPos;
		GetCursorPos(&curPos);
		HMONITOR curMon = MonitorFromPoint(curPos, MONITOR_DEFAULTTOPRIMARY);
		cur.display = 0;
		for (int m = 0; m < numMon; m++) {
			if (mon[m].hnd == curMon) {
				cur.display = m;
				break;
			}
		}
	}
	defRes[0] = mon[cur.display].width;
	defRes[1] = mon[cur.display].height;

	minSize[0] = minSize[1] = 0;

	if (sys->debuggerRunning) {
		// Force topmost off if debugger is attached
		cur.flags&= ~VID_TOPMOST;
	}
	if (cur.mode[0] == 0) {
		// Use default resolution if one isn't specified
		Vector2Copy(defRes, cur.mode);
	}
	Vector2Copy(cur.mode, vid.size);
	if (cur.flags & VID_FULLSCREEN) {
		Vector2Copy(cur.mode, scrSize);
		if (cur.shown) {
			// Change screen resolution
			SetActive(true);
		}
	} else {
		Vector2Copy(defRes, scrSize);
	}

	// Select window styles
	int style = WS_POPUP;
	if ( !(cur.flags & VID_FULLSCREEN) && (cur.flags & VID_USESAVED || cur.flags & VID_RESIZABLE || cur.mode[0] < defRes[0] || cur.mode[1] < defRes[1]) ) {
		style|= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		if (cur.flags & VID_RESIZABLE) {
			style|= WS_SIZEBOX | WS_MAXIMIZEBOX;
		}
	}
	int exStyle = (cur.flags & VID_TOPMOST)? WS_EX_TOPMOST : 0;

	SetWindowLong(hwnd, GWL_STYLE, style);
	SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

	// Get window rectangle
	RECT wrec;
	if (cur.flags & VID_USESAVED) {
		POINT savePos = { cur.save.pos[0], cur.save.pos[1] };
		HMONITOR winMon = MonitorFromPoint(savePos, MONITOR_DEFAULTTONULL);
		if (winMon == NULL) {
			// Window is offscreen, move it to the nearest monitor
			winMon = MonitorFromPoint(savePos, MONITOR_DEFAULTTONEAREST);
			for (int m = 0; m < numMon; m++) {
				if (mon[m].hnd == winMon) {
					wrec.left = 0;
					wrec.top = 0;
					wrec.right = 1000;
					wrec.bottom = 1000;
					AdjustWindowRectEx(&wrec, style, FALSE, exStyle);
					cur.save.pos[0] = mon[m].left - wrec.left;
					cur.save.pos[1] = mon[m].top - wrec.top;
					break;
				}
			}
		}
		wrec.left = cur.save.pos[0];
		wrec.top = cur.save.pos[1];
		if (cur.save.maximised) {
			cur.flags|= VID_MAXIMIZE;
		} else {
			cur.mode[0] = cur.save.size[0];
			cur.mode[1] = cur.save.size[1];
		}
	} else {
		wrec.left = (scrSize[0] - cur.mode[0])/2 + mon[cur.display].left;
		wrec.top = (scrSize[1] - cur.mode[1])/2 + mon[cur.display].top;
	}
	vid.pos[0] = wrec.left;
	vid.pos[1] = wrec.top;
	wrec.right = wrec.left + cur.mode[0];
	wrec.bottom = wrec.top + cur.mode[1];
	AdjustWindowRectEx(&wrec, style, FALSE, exStyle);
	SetWindowPos(hwnd, NULL, wrec.left, wrec.top, wrec.right - wrec.left, wrec.bottom - wrec.top, SWP_FRAMECHANGED);

	if (cur.shown) {
		ShowWindow(hwnd, SW_SHOW);
		SetFocus(hwnd);
		HWND fgHwnd = GetForegroundWindow();
		if (fgHwnd) {
			DWORD processId = 0;
			GetWindowThreadProcessId(fgHwnd, &processId);
			if (processId == GetCurrentProcessId()) {
				SetForegroundWindow(hwnd);
			} else {
				FlashWindow(hwnd, FALSE);
			}
		} else {
			SetForegroundWindow(hwnd);
		}
	}
	
	// Calculate minimum size
	if (cur.flags & VID_RESIZABLE && cur.minSize[0]) {
		RECT rec;
		rec.left = 0;
		rec.top = 0;
		rec.right = cur.minSize[0];
		rec.bottom = cur.minSize[1];
		AdjustWindowRectEx(&rec, style, FALSE, exStyle);
		minSize[0] = rec.right - rec.left;
		minSize[1] = rec.bottom - rec.top;
	}

	if (cur.flags & VID_MAXIMIZE) {
		ShowWindow(hwnd, SW_MAXIMIZE);
		GetClientRect(hwnd, &wrec);
		vid.size[0] = wrec.right;
		vid.size[1] = wrec.bottom;
	}

	// Process any messages generated during application
	sys->RunMessages();

	initialised = true;
	return 0;
}

void sys_video_c::SetActive(bool active)
{
	if ((cur.flags & VID_FULLSCREEN) && (cur.mode[0] != defRes[0] || cur.mode[1] != defRes[1] || cur.depth)) {
		// Fullscreen and mode specified
		if (active) {
			// Change resolution		
			DEVMODE dm;
			ZeroMemory(&dm, sizeof(dm));
			dm.dmSize		= sizeof(dm);
			dm.dmPelsWidth  = cur.mode[0];
			dm.dmPelsHeight = cur.mode[1];
			dm.dmFields		= DM_PELSWIDTH | DM_PELSHEIGHT;
			if (cur.depth)	{
				dm.dmFields|= DM_BITSPERPEL;
				dm.dmBitsPerPel = cur.depth;
			}
			if (ChangeDisplaySettingsEx(mon[cur.display].devName, &dm, NULL, CDS_FULLSCREEN, NULL) != DISP_CHANGE_SUCCESSFUL) {
				sys->con->Warning("failed to change screen resolution");
			}

			// Make sure window is positioned correctly
			RefreshMonitorInfo();
			SetWindowPos(hwnd, NULL, mon[cur.display].left, mon[cur.display].top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		} else {
			// Change back to default
			ChangeDisplaySettingsEx(mon[cur.display].devName, NULL, NULL, CDS_FULLSCREEN, NULL);
		}
	}

	if (initialised && (cur.flags & VID_TOPMOST) && IsWindowVisible(hwnd)) {
		if (active) {
			// Make it topmost
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		} else {
			if (cur.flags & VID_FULLSCREEN) {
				// Just minimize it
				ShowWindow(hwnd, SW_MINIMIZE);
			} else {
				HWND fghwnd = GetForegroundWindow();
				if (GetWindowLong(fghwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) {
					// Switching to a topmost window, put our window at the top of all non-topmost windows
					SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				} else {
					// Switching to a non-topmost window, put our window behind it
					SetWindowPos(hwnd, fghwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				}
			}
		}
	}
}

bool sys_video_c::IsActive()
{
	return GetForegroundWindow() == hwnd;
}

void sys_video_c::SizeChanged(int width, int height, bool max)
{
	vid.size[0] = width;
	vid.size[1] = height;
	vid.maximised = max;
}

void sys_video_c::PosChanged(int x, int y)
{
	vid.pos[0] = x;
	vid.pos[1] = y;
}

void sys_video_c::GetMinSize(int &width, int &height)
{
	width = minSize[0];
	height = minSize[1];
}

void sys_video_c::SetVisible(bool vis)
{
	if ( !initialised ) return;
	ShowWindow(hwnd, vis? SW_SHOW : SW_HIDE);
}

bool sys_video_c::IsVisible()
{
	if ( !initialised || !hwnd ) return false;
	return IsWindowVisible(hwnd);
}

void sys_video_c::SetTitle(const char* title)
{
	SetWindowText(hwnd, (title && *title)? title : CFG_TITLE);
}

void* sys_video_c::GetWindowHandle()
{
	return hwnd;
}

void sys_video_c::GetRelativeCursor(int &x, int &y)
{
	if ( !initialised ) return;
	POINT cp;
	GetCursorPos(&cp);
	ScreenToClient(hwnd, &cp);
	x = cp.x;
	y = cp.y;
}

void sys_video_c::SetRelativeCursor(int x, int y)
{
	if ( !initialised ) return;
	POINT cp = {x, y};
	ClientToScreen(hwnd, &cp);
	SetCursorPos(cp.x, cp.y);
}

bool sys_video_c::IsCursorOverWindow()
{
	if (initialised && hwnd && IsWindowVisible(hwnd))
	{
		POINT mousePoint;
		if (GetCursorPos(&mousePoint)) {
			if (WindowFromPoint(mousePoint) != hwnd) {
				return false;
			}
		}
	}
	return true;
}

BOOL __stdcall sys_video_c::MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT rect, LPARAM data)
{
	sys_video_c* vid = (sys_video_c*)data;
	return vid->AddMonitor(hMonitor);
}

bool sys_video_c::AddMonitor(HMONITOR hMonitor)
{
	MONITORINFOEX info;
	info.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &info);
	mon[numMon].hnd = hMonitor;
	mon[numMon].left = info.rcMonitor.left;
	mon[numMon].top = info.rcMonitor.top;
	mon[numMon].width = info.rcMonitor.right - info.rcMonitor.left;
	mon[numMon].height = info.rcMonitor.bottom - info.rcMonitor.top;
	if (info.dwFlags & MONITORINFOF_PRIMARY) {
		priMon = numMon;
	}
	strcpy(mon[numMon].devName, info.szDevice);
	return ++numMon < 16;
}

void sys_video_c::RefreshMonitorInfo()
{
	for (int m = 0; m < numMon; m++) {
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		if (GetMonitorInfo(mon[m].hnd, &info)) {
			mon[m].left = info.rcMonitor.left;
			mon[m].top = info.rcMonitor.top;
			mon[m].width = info.rcMonitor.right - info.rcMonitor.left;
			mon[m].height = info.rcMonitor.bottom - info.rcMonitor.top;
		} else {
			if (m == priMon) sys->Error("Primary monitor no longer exists!");
			mon[m].left = mon[priMon].left;
			mon[m].top = mon[priMon].top;
			mon[m].width = mon[priMon].width;
			mon[m].height = mon[priMon].height;
		}
	}
}
