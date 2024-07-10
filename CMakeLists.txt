cmake_minimum_required(VERSION 3.15)
project(SimpleGraphic C CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if (APPLE)
    enable_language(OBJCXX)
endif ()

if (MSVC)
    add_compile_options("/Zi")
    add_link_options("/DEBUG:FULL")
endif ()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(${PROJECT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)
add_subdirectory(dep/glm)

set(SIMPLEGRAPHIC_SOURCES
    "config.h"
    "dep/stb/stb_image.h"
    "dep/stb/stb_image_resize.h"
    "dep/stb/stb_image_write.h"
    "engine/common.h"
    "engine/common/base64.c"
    "engine/common/base64.h"
    "engine/common/common.cpp"
    "engine/common/console.cpp"
    "engine/common/console.h"
    "engine/common/keylist.h"
    "engine/common/memtrak3.cpp"
    "engine/common/memtrak3.h"
    "engine/common/streams.cpp"
    "engine/common/streams.h"
    "engine/core/core_config.cpp"
    "engine/core/core_config.h"
    "engine/core/core_image.cpp"
    "engine/core/core_image.h"
    "engine/core/core_main.cpp"
    "engine/core/core_main.h"
    "engine/core/core_video.cpp"
    "engine/core/core_video.h"
    "engine/render/r_font.cpp"
    "engine/render/r_font.h"
    "engine/render/r_main.cpp"
    "engine/render/r_main.h"
    "engine/render/r_texture.cpp"
    "engine/render/r_texture.h"
    # "engine/system/win/sys_console.cpp"
    "engine/system/win/sys_console_unix.cpp"
    "engine/system/win/sys_local.h"
    "engine/system/win/sys_main.cpp"
    "engine/system/win/sys_opengl.cpp"
    "engine/system/win/sys_video.cpp"
    "engine/system/sys_console.h"
    "engine/system/sys_main.h"
    "engine/system/sys_opengl.h"
    "engine/system/sys_video.h"
    "win/entry.cpp"
    "ui.h"
    "ui_api.cpp"
    "ui_console.cpp"
    "ui_console.h"
    "ui_debug.cpp"
    "ui_debug.h"
    "ui_local.h"
    "ui_main.cpp"
    "ui_main.h"
    "ui_subscript.cpp"
    "ui_subscript.h"
)

set (SIMPLEGRAPHIC_PLATFORM_SOURCES)
if (APPLE)
    set (SIMPLEGRAPHIC_PLATFORM_SOURCES
        "engine/system/win/sys_macos.mm"
    )
endif()

add_library(SimpleGraphic SHARED
    ${SIMPLEGRAPHIC_SOURCES}
    ${SIMPLEGRAPHIC_PLATFORM_SOURCES}
)
target_compile_definitions(SimpleGraphic
    PRIVATE
    "UNICODE"
    "_CRT_SECURE_NO_DEPRECATE"
    "_CRT_SECURE_NO_WARNINGS"
    "_SCL_SECURE_NO_DEPRECATE"
    "_SCL_SECURE_NO_WARNINGS"
    "GLFW_INCLUDE_NONE"
    "GL_SILENCE_DEPRECATION"
    "SIMPLEGRAPHIC_EXPORTS"
)

target_include_directories(SimpleGraphic
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
)

find_package(unofficial-angle CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(LuaJIT REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

add_library(imgui STATIC
    dep/imgui/imconfig.h
    dep/imgui/imgui.cpp
    dep/imgui/imgui.h
    dep/imgui/imgui_demo.cpp
    dep/imgui/imgui_draw.cpp
    dep/imgui/imgui_internal.h
    dep/imgui/imgui_tables.cpp
    dep/imgui/imgui_widgets.cpp
    dep/imgui/imstb_rectpack.h
    dep/imgui/imstb_textedit.h
    dep/imgui/imstb_truetype.h
    dep/imgui/backends/imgui_impl_glfw.cpp
    dep/imgui/backends/imgui_impl_glfw.h
    dep/imgui/backends/imgui_impl_opengl3.cpp
    dep/imgui/backends/imgui_impl_opengl3.h
    dep/imgui/backends/imgui_impl_opengl3_loader.h
    dep/imgui/misc/cpp/imgui_stdlib.cpp
    dep/imgui/misc/cpp/imgui_stdlib.h
)

target_compile_definitions(imgui PUBLIC
    "IMGUI_IMPL_OPENGL_ES2"
)

target_include_directories(imgui PUBLIC
    dep/imgui
    dep/imgui/backends
    dep/imgui/misc/cpp
)

target_link_libraries(imgui PUBLIC
    unofficial::angle::libGLESv2
    glfw
)

target_include_directories(SimpleGraphic
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/stb
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_options(SimpleGraphic
        PRIVATE
        "-export-dynamic"
    )
endif ()

if (WIN32)
    target_compile_options(SimpleGraphic
        PRIVATE
        "/EHa"
    )
    target_compile_definitions(SimpleGraphic
        PRIVATE
        "_CRT_SECURE_NO_DEPRECATE=1"
        "_CRT_SECURE_NO_WARNINGS=1"
        "_SCL_SECURE_NO_DEPRECATE=1"
        "_SCL_SECURE_NO_WARNINGS=1"
    )
    target_link_libraries(SimpleGraphic
        PRIVATE
        "winmm.lib"
    )
endif ()

if (APPLE)
    find_library(CORE_FOUNDATION_LIBRARY CoreFoundation)
    find_library(APPLICATION_SERVICES_LIBRARY ApplicationServices)
    target_link_libraries(SimpleGraphic
        PRIVATE
        ${CORE_FOUNDATION_LIBRARY}
        ${APPLICATION_SERVICES_LIBRARY}
    )
endif ()

target_link_libraries(SimpleGraphic
    PRIVATE
    unofficial::angle::libEGL
    unofficial::angle::libGLESv2
    fmt::fmt
    glfw
    glm::glm
    imgui
    LuaJIT::LuaJIT
    re2::re2
    Threads::Threads
    ZLIB::ZLIB
)

install(FILES $<TARGET_RUNTIME_DLLS:SimpleGraphic> DESTINATION ".")
install(TARGETS SimpleGraphic RUNTIME DESTINATION ".")

if (WIN32)
    set(DEPS_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

    if (NOT VCPKG_TARGET_TRIPLET MATCHES "-release")
        find_file(LuaJIT_DLL_DEBUG NAMES "lua51.dll"
            PATHS "${DEPS_DIR}" PATH_SUFFIXES "debug/bin" REQUIRED NO_DEFAULT_PATH)
        find_file(ZLIB_DLL_DEBUG NAMES "zlibd1.dll"
            PATHS "${DEPS_DIR}" PATH_SUFFIXES "debug/bin" REQUIRED NO_DEFAULT_PATH)
             
        install(FILES
            ${LuaJIT_DLL_DEBUG}
            ${ZLIB_DLL_DEBUG}
            DESTINATION "."
            CONFIGURATIONS Debug
        )
    endif ()

    find_file(LuaJIT_DLL_RELEASE NAMES "lua51.dll"
        PATHS "${DEPS_DIR}" PATH_SUFFIXES "bin" REQUIRED NO_DEFAULT_PATH)
    find_file(ZLIB_DLL_RELEASE NAMES "zlib1.dll"
        PATHS "${DEPS_DIR}" PATH_SUFFIXES "bin" REQUIRED NO_DEFAULT_PATH
    )

    install(FILES
        ${LuaJIT_DLL_RELEASE}
        ${ZLIB_DLL_RELEASE}
        DESTINATION "."
        CONFIGURATIONS Release MinSizeRel RelWithDebInfo
    )
endif ()


# lcurl module

set(LCURL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/Lua-cURLv3)
file(GLOB LCURL_SOURCES ${LCURL_SOURCE_DIR}/src/**.c)
add_library(lcurl SHARED ${LCURL_SOURCES})

target_include_directories(lcurl
    PRIVATE
    ${LCURL_SOURCE_DIR}/src
)

target_link_libraries(lcurl
    PRIVATE
    CURL::libcurl
    LuaJIT::LuaJIT
)

install(TARGETS lcurl RUNTIME DESTINATION ".")
install(FILES $<TARGET_RUNTIME_DLLS:lcurl> DESTINATION ".")


# lzip module

add_library(lzip SHARED libs/LZip/lzip.cpp)

target_include_directories(lzip
    PRIVATE
)

target_link_libraries(lzip
    PRIVATE
    LuaJIT::LuaJIT
    ZLIB::ZLIB
)

install(TARGETS lzip RUNTIME DESTINATION ".")
install(FILES $<TARGET_RUNTIME_DLLS:lzip> DESTINATION ".")
