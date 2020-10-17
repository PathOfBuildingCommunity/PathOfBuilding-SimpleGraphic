## Instructions to build a DLL

1. Check out `git` to a new folder
2. Open Visual Studio 2019
3. Choose the "Open a folder" option and choose the git checkout
4. In the configurations dropdown, choose "Manage configurations..."
5. Add a new "Release|x86" configuration and save the `CMakeSettings.json` file
6. Choose this new "Release|x86" configuration in the configurations dropdown
7. Go to "Build" > "Build All" and wait for the build to complete without errors
8. Find the new DLL in `out\build\x86-Release\SimpleGraphic.dll`

To use this DLL, simply copy it to your Path Of Building install location and
restart Path Of Building.
