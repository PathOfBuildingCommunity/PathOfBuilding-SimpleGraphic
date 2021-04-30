project(SimpleGraphic C CXX)
cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake)

set(SIMPLEGRAPHIC_SOURCES
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
    # "engine/system/win/sys_local.h"
    # "engine/system/win/sys_main.cpp"
    # "engine/system/win/sys_opengl.cpp"
    # "engine/system/win/sys_video.cpp"
    "engine/system/sys_console.h"
    "engine/system/sys_main.h"
    "engine/system/sys_opengl.h"
    "engine/system/sys_video.h"
    # "win/entry.cpp"
)

add_library(SimpleGraphic SHARED
    ${SIMPLEGRAPHIC_SOURCES}
)

target_compile_definitions(SimpleGraphic
    PRIVATE
    "GL_SILENCE_DEPRECATION"
)

target_include_directories(SimpleGraphic
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine"    
)

find_package(fmt CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(libpng CONFIG REQUIRED)

target_include_directories(SimpleGraphic
    PRIVATE
    ${JPEG_INCLUDE_DIR}
)

target_link_libraries(SimpleGraphic
    PRIVATE
    fmt::fmt
    ${JPEG_LIBRARIES}
    OpenGL::GL
    png_static
)