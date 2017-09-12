// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Core Config Header
//

// ==========
// Interfaces
// ==========

// Core Config
class core_IConfig {
public:
	static core_IConfig* GetHandle(sys_IMain* sysHnd);
	static void FreeHandle(core_IConfig* hnd);
	
	virtual bool	LoadConfig(const char* cfgName) = 0;
	virtual bool	SaveConfig(const char* cfgName) = 0;
};
