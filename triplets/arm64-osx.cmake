set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

# GLFW loads EGL/GLES at runtime via dlopen("libEGL.dylib"), matching the
# libEGL.dll/libGLESv2.dll pair shipped on Windows; ANGLE must be shared.
if(PORT STREQUAL "angle")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
