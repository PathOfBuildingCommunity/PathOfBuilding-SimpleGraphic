#include <mach-o/dyld.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern int RunLuaFileAsWin(int argc, char **argv);

int main(int argc, char **argv)
{
	char restartMarker[128];
	char failureMarker[144];
	char successMarker[144];
	snprintf(restartMarker, sizeof(restartMarker),
		"/tmp/org.pathofbuilding.simplegraphic-smoke-restart-%d", getpid());
	snprintf(failureMarker, sizeof(failureMarker), "%s-failed", restartMarker);
	snprintf(successMarker, sizeof(successMarker), "%s-passed", restartMarker);
	unlink(restartMarker);
	unlink(failureMarker);
	unlink(successMarker);
	setenv("SIMPLEGRAPHIC_SMOKE_RESTART_MARKER", restartMarker, 1);

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
	int result = RunLuaFileAsWin(1, defaultArgs);
	int passed = access(successMarker, F_OK) == 0;
	unlink(failureMarker);
	unlink(successMarker);
	return result == 0 && passed ? 0 : 1;
}
