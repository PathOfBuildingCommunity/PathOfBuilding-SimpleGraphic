# Apple Silicon smoke app

`SimpleGraphicSmoke.app` is the first native Apple Silicon SimpleGraphic
artifact. It uses the same LuaJIT host, Lua modules, GLFW Cocoa window, and
ANGLE Metal renderer intended for Path of Building integration.

## Build

The build requires Xcode command-line tools, CMake, Git, and an Apple Silicon
Mac. Dependencies are built by the pinned vcpkg submodule.

```sh
git submodule update --init --recursive

MACOSX_DEPLOYMENT_TARGET=13.0 cmake -S . -B build-macos13 \
  -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_OVERLAY_TRIPLETS="$PWD/vcpkg-ports/triplets" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0

cmake --build build-macos13 --target SimpleGraphicSmoke --parallel
```

The artifact is written to `build-macos13/SimpleGraphicSmoke.app`. Run its
executable from a terminal to retain the smoke log:

```sh
build-macos13/SimpleGraphicSmoke.app/Contents/MacOS/SimpleGraphicSmoke
```

A passing run reports LuaJIT and native module loading, compression, paths,
localhost sockets, ANGLE rendering, and the host restart loop before exiting.

## Verify

```sh
macos/verify-bundle.sh build-macos13/SimpleGraphicSmoke.app
```

The verifier rejects non-arm64 Mach-O files, deployment targets newer than
macOS 13, absolute non-system dependencies, and invalid ad-hoc signatures.

## Current scope

The smoke app proves the native host and packaging seam. It does not yet prove
bitmap fonts, PNG/WebP and DDS/BC fixture rendering, screenshots, clipboard,
URL opening, HTTPS trust, or full Path of Building startup. It also still needs
a run on physical Apple Silicon macOS 13 and a green Windows CI build before it
is ready to hand off to Path of Building integration.
