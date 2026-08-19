set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_OSX_DEPLOYMENT_TARGET 13.0)

if(PORT STREQUAL "angle")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()

# New SDKs expose pipe2 even when targeting systems that cannot run it.
if(PORT STREQUAL "curl")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DHAVE_PIPE2=OFF")
endif()
