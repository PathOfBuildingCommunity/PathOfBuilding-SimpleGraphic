project(SimpleGraphic C CXX)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

if (APPLE)
    enable_language(OBJCXX)
endif ()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(${PROJECT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

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
    "GL_SILENCE_DEPRECATION"
    "SIMPLEGRAPHIC_EXPORTS"
)

target_include_directories(SimpleGraphic
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"
)

find_package(fmt CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(LuaJit REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB REQUIRED)

target_include_directories(SimpleGraphic
    PRIVATE
    ${JPEG_INCLUDE_DIR}
    ${LUAJIT_INCLUDE_DIR}
)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_options(SimpleGraphic
        PRIVATE
        "-export-dynamic"
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
    fmt::fmt
    glfw
    ${LUAJIT_LIBRARIES}
    OpenGL::GL
    ZLIB::ZLIB
)