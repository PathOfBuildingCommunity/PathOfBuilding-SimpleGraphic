// DyLua: SimpleGraphic
// (c) David Gowor, 2014
//
// Entry Point
// Platform: Windows
//

#include "system/win/sys_local.h"
#if 0
#pragma comment(lib, "winmm")
#pragma comment(lib, "opengl32")
#include "_lib/lua.h"
#if LUA_VERSION_NUM >= 503
#if defined(_WIN64) && defined(_DEBUG)
#pragma comment(lib, "_lib\\lua53_64MT_D")
#elif defined(_WIN64)
#pragma comment(lib, "_lib\\lua53_64MT")
#elif defined(_DEBUG)
#pragma comment(lib, "_lib\\lua53_32MT_D")
#else
#pragma comment(lib, "_lib\\lua53_32MT")
#endif
#else
#if defined(_WIN64) 
#pragma comment(lib, "_lib\\lua51_64")
#else
#pragma comment(lib, "_lib\\lua51_32")
#endif
#endif
#if defined(_WIN64) && defined(_DEBUG)
#pragma comment(lib, "_lib\\zlib64MT_D")
#pragma comment(lib, "_lib\\jpeg64MT_D")
#pragma comment(lib, "_lib\\png64MT_D")
#pragma comment(lib, "_lib\\gif64MT_D")
#elif defined(_WIN64)
#pragma comment(lib, "_lib\\zlib64MT")
#pragma comment(lib, "_lib\\jpeg64MT")
#pragma comment(lib, "_lib\\png64MT")
#pragma comment(lib, "_lib\\gif64MT")
#elif defined(_DEBUG)
#pragma comment(lib, "_lib\\zlib32MT_D")
#pragma comment(lib, "_lib\\jpeg32MT_D")
#pragma comment(lib, "_lib\\png32MT_D")
#pragma comment(lib, "_lib\\gif32MT_D")
#else
#pragma comment(lib, "_lib\\zlib32MT")
#pragma comment(lib, "_lib\\jpeg32MT")
#pragma comment(lib, "_lib\\png32MT")
#pragma comment(lib, "_lib\\gif32MT")
#endif
#endif

extern "C" __declspec(dllexport) int RunLuaFileAsWin(int argc, char** argv)
{
#ifdef _MEMTRAK_H
	strcpy_s(_memTrak_reportName, 512, "SimpleGraphic/memtrak.log");
#endif

	timeBeginPeriod(1);

	sys_main_c* sys = new sys_main_c;

	while (sys->Run(argc, argv));

	delete sys;

	timeEndPeriod(1);
	return 0;
}
