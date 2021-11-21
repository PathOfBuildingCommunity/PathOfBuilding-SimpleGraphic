# Path of Building Community SimpleGraphic.dll

[中文简介](README_zh-rCN.md)
## Introduction

`SimpleGraphic.dll` is the host environment for Lua.
It contains the API used by the application's Lua logic, as well as a 2D OpenGL
renderer, window management, input handling, and a debug console.
It exports one symbol, `RunLuaFileAsWin`, which is passed a
C-style argc/argv argument list, with the script path as `argv[0]`.

The Windows-specific code is contained in 5 files:
- `win\entry.cpp`: Contains the DLL export
It just creates the system main module, and runs it
- `engine\system\win\sys_main.cpp`: The system main module.
It initialises the application, and contains generic OS interface functions,
such as input and clipboard handling
- `engine\system\win\sys_console.cpp`: Manages the debug console window that
appears during the program's initialisation
- `engine\system\win\sys_video.cpp`: Creates and manages the main program window
- `engine\system\win\sys_opengl.cpp`: Initialises OpenGL

## Building

`SimpleGraphic.dll` is currently built using Visual Studio 2019.

The DLL depends on a number of 3rd-party libraries. Details and instructions
for how to obtain them are in [DEPENDENCIES.md](DEPENDENCIES.md).

Once the libraries are obtained, follow these instructions to build the DLL:
1) Open `SimpleGraphic.sln` in Visual Studio 2019
2) Choose the "Release|x86" configuration
3) Build the solution
4) The resulting `SimpleGraphic.dll` can be found in the `Release` directory
5) Copy `SimpleGraphic.dll` to the Path of Building install directory,
replacing the existing file.

## Debugging

Since SimpleGraphic.dll is dynamically loaded by `PathOfBuilding.exe`,
to debug it, run `PathOfBuilding.exe` and then attach to that process using the
"Debug" > "Attach to Process..." menu option in Visual Studio.

If debugging of the initialization is desired, then adding a `MessageBox`
or some other pause at the start will give you time to attach and continue.

## Licence

[MIT](https://opensource.org/licenses/MIT)

For 3rd-party licences, see [LICENSE](LICENSE).
The licencing information is considered to be part of the documentation.
