// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// System Video Header
//

// =======
// Classes
// =======

// Video settings flags
enum vidFlags_e {
	VID_FULLSCREEN = 0x01,
	VID_TOPMOST = 0x02,
	VID_RESIZABLE = 0x04,
	VID_MAXIMIZE = 0x08,
	VID_USESAVED = 0x10,
};

// Saved video state structure
struct sys_vidSave_s {
	int		size[2];
	int		pos[2];
	bool	maximised;
};

// Video settings structure
struct sys_vidSet_s {
	bool	shown;		// Show window?
	int		flags;		// Flags
	int		display;	// Display number
	int		mode[2];	// Resolution or window size
	int		depth;		// Bit depth
	int		minSize[2];	// Minimum size for resizable windows
	sys_vidSave_s save; // Saved state
};

// ==========
// Interfaces
// ==========

// System Video
class sys_IVideo {
public:
	static sys_IVideo* GetHandle(class sys_IMain* sysHnd);
	static void FreeHandle(sys_IVideo* hnd);

	sys_vidSave_s vid;	// Current state

	virtual	int		Apply(sys_vidSet_s* set) = 0;	// Apply settings

	virtual	void	SetActive(bool active) = 0;		// Respond to window activated status change
	virtual bool	IsActive() = 0; // Get activated status
	virtual void	SizeChanged(int width, int height, bool max) = 0; // Respond to window size change
	virtual void	PosChanged(int x, int y) = 0; // Respond to window position change
	virtual void	GetMinSize(int &width, int &height) = 0; // Get minimum window size
	virtual void	SetVisible(bool vis) = 0;		// Show/hide window
	virtual void	SetTitle(const char* title) = 0;// Change window title
	virtual void*	GetWindowHandle() = 0;			// Get window handle
	virtual void	GetRelativeCursor(int &x, int &y) = 0; // Get cursor position relative to window
	virtual void	SetRelativeCursor(int x, int y) = 0; // Set cursor position relative to window
};
