// DyLua: SimpleGraphic
// (c) David Gowor, 2014
//
// UI Main Header
//

// =======
// Classes
// =======

// UI Manager
class ui_main_c: public ui_IMain {
public:
	// Interface
	void	Init(int argc, char** argv);
	void	Frame();
	void	Shutdown();
	void	KeyEvent(int key, int type);
	bool	CanExit();

	// Encapsulated
	ui_main_c(sys_IMain* sysHnd, core_IConfig* cfgHnd);

	sys_IMain* sys = nullptr;
	core_IConfig* cfg = nullptr;

	r_IRenderer* renderer = nullptr;

	ui_IConsole* conUI = nullptr;
	ui_IDebug* debug = nullptr;

	dword	subScriptSize = 0;
	ui_ISubScript** subScriptList = nullptr;

	lua_State* L = nullptr;
	char*	scriptName = nullptr;
	char*	scriptCfg = nullptr;
	char*	scriptPath = nullptr;
	char*	scriptWorkDir = nullptr;
	int		scriptArgc = 0;
	char**	scriptArgv = nullptr;
	bool	restartFlag = false;
	bool	didExit = false;
	bool	renderEnable = false;
	int		cursorX = 0;
	int		cursorY = 0;
	int		framesSinceWindowHidden = 0;
	volatile bool	inLua = false;

	static int InitAPI(lua_State* L);

	void	ScriptInit();
	void	ScriptShutdown();

	void	LAssert(lua_State* L, int cond, const char* fmt, ...);
	int		IsUserData(lua_State* L, int index, const char* metaName);
	int		PushCallback(const char* name);
	void	PCall(int narg, int nret);
	void	DoError(const char* msg, const char* error);

	void	CallKeyHandler(const char* hname, int key, bool dblclk);
	const char* NameForKey(int key);
	int		KeyForName(const char* name);
};
