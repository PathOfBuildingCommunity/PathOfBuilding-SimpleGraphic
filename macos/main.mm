// Path of Building launcher for macOS.
// Resolves the Lua entry script, prepares LuaJIT search paths (macOS defaults
// have no exe-relative entries, unlike Windows), captures a pob:// launch URL,
// then hands off to SimpleGraphic's RunLuaFileAsWin.
#import <Foundation/Foundation.h>
#import <CoreServices/CoreServices.h>
// kInternetEventClass/kAEGetURL live here, not pulled in transitively by
// CoreServices.h on this SDK.
#import <ApplicationServices/ApplicationServices.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mach-o/dyld.h>
#include <string>
#include <unistd.h>
#include <vector>

extern "C" int RunLuaFileAsWin(int argc, char** argv);

static std::string g_launchUrl;

@interface PobUrlHandler : NSObject
- (void)handleGetURLEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)reply;
@end

@implementation PobUrlHandler
- (void)handleGetURLEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)reply {
	NSString* url = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
	if (url) {
		g_launchUrl = url.UTF8String;
	}
}
@end

static std::string ExecutableDir() {
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::vector<char> buf(size + 1);
	_NSGetExecutablePath(buf.data(), &size);
	char resolved[PATH_MAX];
	if (!realpath(buf.data(), resolved)) {
		return ".";
	}
	std::string path = resolved;
	return path.substr(0, path.find_last_of('/'));
}

int main(int argc, char** argv) {
	@autoreleasepool {
		// Capture the URL if the app was launched via the pob: scheme; the
		// event is queued at launch, a short run-loop spin collects it.
		PobUrlHandler* handler = [PobUrlHandler new];
		[[NSAppleEventManager sharedAppleEventManager]
			setEventHandler:handler
			andSelector:@selector(handleGetURLEvent:withReplyEvent:)
			forEventClass:kInternetEventClass
			andEventID:kAEGetURL];
		[[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.15]];

		std::string script;
		if (argc > 1 && argv[1][0] != '-' && strncmp(argv[1], "pob:", 4) != 0) {
			script = argv[1];
		} else if (const char* env = getenv("POB_SCRIPT_PATH")) {
			script = env;
		}
#ifdef POB_DEFAULT_SCRIPT
		if (script.empty()) {
			script = POB_DEFAULT_SCRIPT;
		}
#endif
		if (script.empty()) {
			fprintf(stderr, "pob: no Lua entry script; pass a path or set POB_SCRIPT_PATH\n");
			return 1;
		}
		char resolved[PATH_MAX];
		if (!realpath(script.c_str(), resolved)) {
			fprintf(stderr, "pob: script not found: %s\n", script.c_str());
			return 1;
		}

		std::string exeDir = ExecutableDir();
		setenv("LUA_PATH", (exeDir + "/lua/?.lua;" + exeDir + "/lua/?/init.lua;;").c_str(), 1);
		setenv("LUA_CPATH", (exeDir + "/?.so;;").c_str(), 1);
		// Fonts/screenshots resolve relative to the default workdir, which the
		// host derives from the executable's directory; make cwd match it too.
		chdir(exeDir.c_str());

		std::vector<char*> args;
		args.push_back(resolved);
		std::string url = g_launchUrl;
		if (url.empty() && argc > 1 && strncmp(argv[1], "pob:", 4) == 0) {
			url = argv[1];
		}
		if (!url.empty()) {
			args.push_back(url.data());
		}
		for (int i = 2; i < argc; i++) {
			args.push_back(argv[i]);
		}
		return RunLuaFileAsWin((int)args.size(), args.data());
	}
}
