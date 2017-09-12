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

	sys_IMain* sys;
	core_IConfig* cfg;

	r_IRenderer* renderer;

	ui_IConsole* conUI;
	ui_IDebug* debug;

	dword	subScriptSize;
	ui_ISubScript** subScriptList;

	lua_State* L;
	char*	scriptName;
	char*	scriptCfg;
	char*	scriptPath;
	char*	scriptWorkDir;
	int		scriptArgc;
	char**	scriptArgv;
	bool	restartFlag;
	bool	didExit;
	bool	renderEnable;
	int		cursorX;
	int		cursorY;
	volatile bool	inLua;

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
