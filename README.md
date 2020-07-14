## Path of Building Design Overview from @Openarl

It is split into 2 main components: the program, and the runtime.

The program consists of the Lua scripts that comprise the application logic, as well as 
the data files that the application uses. These are the files that are in the GitHub 
repository. When performing a normal install of the application (non-standalone) these 
files end up in %ProgramData%\Path of Building. In theory, this part of the application 
should be fully portable. The application's entry module is Launch.lua, which contains error
handling and update management code. The API is defined in SimpleGraphic\ui_api.cpp. It's
not very well documented at the moment, but there's a summary of the functions at the top of
that file.

The runtime consists of the platform-specific binaries, including the Lua interpreter and 
the host environment, as well as a few libraries, and the data files for the host environment.
In a normal install, these files go in Program Files\Path of Building. In the GitHub 
repository, the runtime files are packed into a zip file (runtime-win32.zip). 

The runtime contains the following binaries:

### Path of Building.exe
I haven't included the source for this, because franky it's very boring,
and is almost entirely Windows-centric code. It takes a Lua script as an argument (or looks 
for 'Launch.lua' if one isn't provided) and checks the first line for a host environment 
specification. This design enables the .exe to function as a generic Lua host, but for 
Path of Building the only host actually used is SimpleGraphic.dll, so all it really does is
load SimpleGraphic.dll and pass the path of the script to the "RunLuaFileAsWin" export.

### SimpleGraphic.dll
This is the Lua host environment. It contains the API used by the
application's Lua logic, as well as a 2D OpenGL renderer, window management, input handling,
and a debug console. It exports one symbol, RunLuaFileAsWin, which is passed a C-style argc/argv
argument list, with the script path as argv[0]. The Windows-specific code is contained in 5 files:
- win\entry.cpp: Contains the DLL export. It just creates the system main module, and runs it!
- engine\system\win\sys_main.cpp: The system main module; it initialises the application, and
contains generic OS interface functions, such as input and clipboard handling.
- engine\system\win\sys_console.cpp: Manages the debug console window that appears during the 
program's initialisation.
- engine\system\win\sys_video.cpp: Creates and manages the main program window. There's a lot of
cruft relating to fullscreen mode which could be stripped out as Path of Building only operates
in windowed mode.
- engine\system\win\sys_opengl.cpp: Initialises OpenGL.

The engine depends on a number of libraries, which are all fully portable:
 - LuaJIT 2.0.4: I use the third-party JIT for Lua: http://luajit.org/
 - ZLib 1.2.8
 - libjpeg 9a: http://ijg.org/
 - libpng 1.6.12: http://libpng.org/pub/png/libpng.html
 - GIFLIB 5.1.1: http://giflib.sourceforge.net/
 - LibTiFF: http://www.libtiff.org/ (Path of Building doesn't use TIFF images so you could 
   cut this out if you wanted to)

### lua51.dll
This is the aforementioned LuaJIT interpreter.

### libcurl.dll
This is of course cURL, which the application uses for network interaction.
The specific version used is 7.54.0, if that matters.

### lcurl.dll
A Lua binding for libcurl: https://github.com/Lua-cURL/Lua-cURLv3

### lzip.dll
A simple library I created to allow Lua code to extract from .zip files. This is used
by the updates system. The code is a bit odd as it's a stripped-down version of a ZIP-based
virtual file system, but it should be portable as it is. It depends on LuaJIT and ZLib.

### Update.exe
This is used by the updates system to update the runtime binaries. It runs with 
administrator priviliges to comply with Windows' UAC; that shouldn't be required on other OSs. 
It contains a Lua interpreter which runs the script passed as the first argument. In practice, the
only script it will run is UpdateApply.lua. To allow the LuaJIT binary to be updated, it uses a 
standard Lua interpreter (not the JIT) which is embedded in the executable. The script's 
environment consists of the standard Lua libaries, plus an extra API function 'SpawnProcess' that
starts the given process and returns without waiting for the process to finish. It also lowers the
execution level so that the new process doesn't run with administrator priviliges. UpdateApply.lua
uses this to restart the main application after the runtime files have been updated.