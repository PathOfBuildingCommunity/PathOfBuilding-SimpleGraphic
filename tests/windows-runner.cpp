// Test host for SimpleGraphic's public runtime entry point.
#include <windows.h>
#include <thread>
#include <string>

static BOOL CALLBACK input(HWND window, LPARAM) {
	DWORD process;
	GetWindowThreadProcessId(window, &process);
	char title[100];
	GetWindowTextA(window, title, sizeof(title));
	if (process != GetCurrentProcessId() || std::string(title) != "Cyrillic input regression") return TRUE;
	for (WPARAM character : { 0x41, 0x401, 0x44f, 0x451, 0x100, 0x20ac, 0xd801, 0xdc00 }) SendMessageW(window, WM_CHAR, character, 1);
	SendMessageW(window, WM_KEYDOWN, VK_LEFT, 1 | (0x4b << 16) | (1 << 24));
	return FALSE;
}
int main(int argc, char** argv)
{
	if (argc != 2) return 2;
	HMODULE library = LoadLibraryW(L"SimpleGraphic.dll");
	if (!library) return 3;
	auto run = reinterpret_cast<int (*)(int, char**)>(GetProcAddress(library, "RunLuaFileAsWin"));
	if (!run) return 4;
	std::thread driver;
	if (std::string(argv[1]) == "input.lua") {
		driver = std::thread([] {
			for (int attempt = 0; attempt < 100; ++attempt) {
				Sleep(100);
				if (!EnumWindows(input, 0)) return;
			}
		});
	}
	int result = run(1, argv + 1);
	if (driver.joinable()) driver.join();
	return result;
}
