# Dependencies for SimpleGraphics

There are several 3rd party (open source) dependencies for building the SimpleGraphics.dll library used by Path of Building. To build the library yourself, you will need to acquire these libraries.

## List of Libraries

The following 3rd party libraries are required to build SimpleGraphics.dll. Usually, the latest version will work, but the most recent version used by the PathOfBuildingCommunity team are as follows:
* LuaJIT 2.0.5-3
* zlib 1.2.11-6
* libjpeg-turbo 2.0.4
* libpng 1.6.37-9
* giflib 5.1.4-6
* tiff 4.0.10-9
* liblzma 5.2.4-5

These libraries are also listed in vcpkg.txt for our GitHub actions build to reference

## Using vcpgk to install dependencies on Windows for building with Visual Studio

vcpgk is an open-source tool by Microsoft which can easily download and build all of the required dependencies. This is the recommended method for retrieving the dependencies when building for Visual Studio.
vcpkg is included as a submodule of this project, so installing it should be relatively simple

1) Run 'git submodule update --init'
2) 'cd vcpkg'
3) Type the following commands:
   1) `bootstrap-vcpkg.sh` or `bootstrap-vcpkg.bat`
   2) `./vcpkg.exe install @../vcpkg.txt` or `vcpkg install @../vcpkg.txt`
4) The SimpleGraphics solution and vcproj are already configured to look in this location for the dependencies, so it should now build as-is.