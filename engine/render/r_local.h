// SimpleGraphic Engine
// (c) David Gowor, 2014
//
// Render Local Header
//

#include "common.h"
#include "system.h"

#include "render.h"

#if __APPLE__ && __MACH__
#define GL_GLEXT_PROTOTYPES
#ifndef APIENTRY
#define APIENTRY
#endif
#define GL_GLEXT_LEGACY
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#elif __linux__
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#define WINGDIAPI
#define APIENTRY __stdcall
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include "core/core_image.h"

#include "r_texture.h"
#include "r_font.h"
#include "r_main.h"

