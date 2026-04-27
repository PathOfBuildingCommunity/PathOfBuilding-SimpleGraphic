// Linux launcher for SimpleGraphic / Path of Building
// Loads libSimpleGraphic.so from the same directory and calls RunLuaFileAsWin.

#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef int (*RunLuaFileAsWin_t)(int argc, char** argv);

int main(int argc, char** argv)
{
    // Resolve our own executable's directory
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len == -1) {
        perror("readlink /proc/self/exe");
        return 1;
    }
    exePath[len] = '\0';

    char exeDir[PATH_MAX];
    strncpy(exeDir, exePath, sizeof(exeDir));
    char* dir = dirname(exeDir);

    // Load libSimpleGraphic.so from the same directory
    char libPath[PATH_MAX];
    snprintf(libPath, sizeof(libPath), "%s/libSimpleGraphic.so", dir);

    void* lib = dlopen(libPath, RTLD_LAZY | RTLD_GLOBAL);
    if (!lib) {
        fprintf(stderr, "Failed to load %s: %s\n", libPath, dlerror());
        return 1;
    }

    RunLuaFileAsWin_t runLua = (RunLuaFileAsWin_t)dlsym(lib, "RunLuaFileAsWin");
    if (!runLua) {
        fprintf(stderr, "dlsym RunLuaFileAsWin: %s\n", dlerror());
        return 1;
    }

    // argv[0] for RunLuaFileAsWin must be the Lua script path.
    // Skip our own argv[0] (the launcher binary).
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <script.lua> [args...]\n", argv[0]);
        return 1;
    }

    // Resolve script path to absolute so basePath resolution inside SimpleGraphic works correctly.
    // basePath is derived from /proc/self/exe (the launcher's dir), so a relative script path
    // would be resolved relative to the build dir rather than the current working directory.
    char scriptAbs[PATH_MAX];
    if (realpath(argv[1], scriptAbs) == NULL) {
        perror(argv[1]);
        return 1;
    }
    argv[1] = scriptAbs;

    // Derive the PoB root from the script location:
    //   script = <pob_root>/src/Launch.lua  →  script_dir = <pob_root>/src  →  pob_root = <pob_root>
    // (the runtime Lua modules live at <pob_root>/runtime/lua/)
    char scriptDirBuf[PATH_MAX];
    strncpy(scriptDirBuf, scriptAbs, sizeof(scriptDirBuf));
    char* scriptDir = dirname(scriptDirBuf);

    char pobRoot[PATH_MAX];
    snprintf(pobRoot, sizeof(pobRoot), "%s/..", scriptDir);
    char pobRootAbs[PATH_MAX];
    if (!realpath(pobRoot, pobRootAbs))
        strncpy(pobRootAbs, pobRoot, sizeof(pobRootAbs));

    // Tell SimpleGraphic where the runtime data (fonts, etc.) lives.
    // When installed, SimpleGraphic/Fonts/ sits alongside the launcher binary,
    // so the launcher's own directory serves as the base path.
    if (!getenv("SG_BASE_PATH"))
        setenv("SG_BASE_PATH", dir, 1);

    // Set LUA_PATH to include the PoB runtime Lua directory.
    // Work around pob-wide-crt.patch bug: _lua_getenvcopy() calls strdup(getenv(name))
    // which crashes on NULL, so we always set these even if we're not overriding.
    if (!getenv("LUA_PATH")) {
        char luaPath[PATH_MAX * 4];
        snprintf(luaPath, sizeof(luaPath),
            "%s/runtime/lua/?.lua;%s/runtime/lua/?/init.lua;;",
            pobRootAbs, pobRootAbs);
        setenv("LUA_PATH", luaPath, 1);
    }

    // Set LUA_CPATH to include the build dir (where our .so modules live without lib prefix).
    if (!getenv("LUA_CPATH")) {
        char luaCPath[PATH_MAX * 4];
        snprintf(luaCPath, sizeof(luaCPath), "%s/?.so;;", dir);
        setenv("LUA_CPATH", luaCPath, 1);
    }

    return runLua(argc - 1, argv + 1);
}
