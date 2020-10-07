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

## Using vcpgk to install dependencies on Windows for building with Visual Studio

vcpgk is an open-source tool by Microsoft which can easily download and build all of the required dependencies. This is the recommended method for retrieving the dependencies when building for Visual Studio.

1) Clone the vcpgk git repository somewhere on your drive: https://github.com/microsoft/vcpkg.git
2) Open a console window and change directory to the new vcpkg checkout  
   * `cd C:\vcpkg`
3) Type the following commands:
   1) `bootstrap-vcpkg.bat`
   2) `vcpkg install giflib:x86-windows-static libjpeg-turbo:x86-windows-static liblzma:x86-windows-static libpng:x86-windows-static luajit:x86-windows-static tiff:x86-windows-static zlib:x86-windows-static`
4) Copy all folders in the `packages` folder into the `dependencies` folder within the SimpleGraphics checkout.
5) The SimpleGraphics solution and vcproj are already configured to look in this location for the dependencies, so it should now build as-is.