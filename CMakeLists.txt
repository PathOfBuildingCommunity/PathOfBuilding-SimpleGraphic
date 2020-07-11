cmake_minimum_required(VERSION 3.0)
project(sg)

set(PROJ "SimpleGraphic")

set(sources
	engine/common/common.cpp
	engine/common/console.cpp
	engine/common/console.h
	engine/common/keylist.h
	engine/common/memtrak3.cpp
	engine/common/memtrak3.h
	engine/common/streams.cpp
	engine/common/streams.h
	engine/common.h
	engine/core/core_config.cpp
	engine/core/core_config.h
	engine/core/core_image.cpp
	engine/core/core_image.h
	engine/core/core_main.cpp
	engine/core/core_main.h
	engine/core/core_video.cpp
	engine/core/core_video.h
	engine/render/r_font.cpp
	engine/render/r_font.h
	engine/render/r_local.h
	engine/render/r_main.cpp
	engine/render/r_main.h
	engine/render/r_texture.cpp
	engine/render/r_texture.h
	engine/render.h
	engine/system/sys_console.h
	engine/system/sys_main.h
	engine/system/sys_opengl.h
	engine/system/sys_video.h
	engine/system/win/sys_console.cpp
	engine/system/win/sys_local.h
	engine/system/win/sys_main.cpp
	engine/system/win/sys_opengl.cpp
	engine/system/win/sys_video.cpp
	engine/system.h
	win/entry.cpp
	config.h
	engine/common/common.cpp
	engine/common/console.cpp
	engine/common/console.h
	engine/common/keylist.h
	engine/common/memtrak3.cpp
	engine/common/memtrak3.h
	engine/common/streams.cpp
	engine/common/streams.h
	engine/common.h
	engine/core/core_config.cpp
	engine/core/core_config.h
	engine/core/core_image.cpp
	engine/core/core_image.h
	engine/core/core_main.cpp
	engine/core/core_main.h
	engine/core/core_video.cpp
	engine/core/core_video.h
	engine/render/r_font.cpp
	engine/render/r_font.h
	engine/render/r_local.h
	engine/render/r_main.cpp
	engine/render/r_main.h
	engine/render/r_texture.cpp
	engine/render/r_texture.h
	engine/render.h
	engine/system/sys_console.h
	engine/system/sys_main.h
	engine/system/sys_opengl.h
	engine/system/sys_video.h
	engine/system/win/sys_console.cpp
	engine/system/win/sys_local.h
	engine/system/win/sys_main.cpp
	engine/system/win/sys_opengl.cpp
	engine/system/win/sys_video.cpp
	engine/system.h
	ui.h
	ui_api.cpp
	ui_console.cpp
	ui_console.h
	ui_debug.cpp
	ui_debug.h
	ui_local.h
	ui_main.cpp
	ui_main.h
	ui_subscript.cpp
	ui_subscript.h
	win/entry.cpp
)

include_directories(engine .)

link_directories(_lib)
link_libraries(jpeg-static libpng16_static tiff zlibstatic giflib5 lua51 opengl32 winmm)

add_library(${PROJ} SHARED ${sources})

set_target_properties(${PROJ} PROPERTIES
	PREFIX ""
	LINK_SEARCH_START_STATIC 1
	LINK_SEARCH_END_STATIC 1
)


