### Instructions to build a DLL
1. Check out git to a new folder
2. Open VS2019
3. Choose the "Open a folder" option and choose the git checkout
4. In the configurations dropdown, choose "Manage configurations..."
5. Add a new x86-Release configuration and save the CMakeSettings.json file
6. Choose this new x86-Release configuration in the configurations dropdown
7. Go to Build->Build All and wait for the build to complete with no errors
8. Find the new dll in out\build\x86-Release\SimpleGraphic.dll

To use this DLL, simply copy it to your Path Of Building install location and restart Path Of Building