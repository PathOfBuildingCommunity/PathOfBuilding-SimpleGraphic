#include <mach-o/dyld.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

extern int RunLuaFileAsWin(int argc, char **argv);

int main(int argc, char **argv)
{
	char executablePath[4096];
	uint32_t size = sizeof(executablePath);
	if (_NSGetExecutablePath(executablePath, &size) == 0) {
		char resourcesPath[4096];
		snprintf(resourcesPath, sizeof(resourcesPath), "%s/../Resources", dirname(executablePath));
		if (chdir(resourcesPath) != 0) {
			perror("Could not enter app resources directory");
			return 1;
		}
	}

	if (argc > 1) {
		return RunLuaFileAsWin(argc - 1, argv + 1);
	}
	char *defaultArgs[] = { "Launch.lua" };
	return RunLuaFileAsWin(1, defaultArgs);
}
