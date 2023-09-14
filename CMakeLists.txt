cmake_minimum_required(VERSION 3.15)
project(SimpleGraphic C CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if (APPLE)
    enable_language(OBJCXX)
endif ()

if (MSVC)
	add_compile_options("/Zi")
	add_link_options("/DEBUG")
endif ()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(${PROJECT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)

set(SIMPLEGRAPHIC_SOURCES
    "config.h"
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
    "stb_image.h"
    "stb_image_write.h"
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
find_package(LuaJit REQUIRED)
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
)

target_link_libraries(imgui PUBLIC
    unofficial::angle::libGLESv2
    glfw
)

target_include_directories(SimpleGraphic
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dep/glad/include
    ${JPEG_INCLUDE_DIR}
    ${LUAJIT_INCLUDE_DIR}
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_options(SimpleGraphic
        PRIVATE
        "-export-dynamic"
    )
endif ()

if (WIN32)
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
    imgui
    ${LUAJIT_LIBRARIES}
    re2::re2
    Threads::Threads
    ZLIB::ZLIB
)

install(FILES $<TARGET_RUNTIME_DLLS:SimpleGraphic> DESTINATION ".")
install(TARGETS SimpleGraphic RUNTIME DESTINATION ".")

if (WIN32)
	if (DEFINED ENV{DEPS_DIR})
		set(DEPS_BIN_DIR ${DEPS_DIR}/bin)
	endif ()
    find_file(LUAJIT_DLL NAMES "lua51.dll" PATHS "${CMAKE_BINARY_DIR}" ${DEPS_BIN_DIR} REQUIRED)
    find_file(ZLIB_DLL NAMES "zlib1.dll" PATHS "${CMAKE_BINARY_DIR}" ${DEPS_BIN_DIR} REQUIRED)
    install(FILES ${LUAJIT_DLL} ${ZLIB_DLL} DESTINATION ".")
endif ()


# lcurl module

set(LCURL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/Lua-cURLv3)
file(GLOB LCURL_SOURCES ${LCURL_SOURCE_DIR}/src/**.c)
add_library(lcurl SHARED ${LCURL_SOURCES})

target_include_directories(lcurl
    PRIVATE
    ${LCURL_SOURCE_DIR}/src
    ${LUAJIT_INCLUDE_DIR}
)

target_link_libraries(lcurl
    PRIVATE
    CURL::libcurl
    ${LUAJIT_LIBRARIES}
)

install(TARGETS lcurl RUNTIME DESTINATION ".")
install(FILES $<TARGET_RUNTIME_DLLS:lcurl> DESTINATION ".")


# lzip module

add_library(lzip SHARED libs/LZip/lzip.cpp)

target_include_directories(lzip
    PRIVATE
    ${LUAJIT_INCLUDE_DIR}
)

target_link_libraries(lzip
    PRIVATE
    ${LUAJIT_LIBRARIES}
    ZLIB::ZLIB
)

install(TARGETS lzip RUNTIME DESTINATION ".")
install(FILES $<TARGET_RUNTIME_DLLS:lzip> DESTINATION ".")
