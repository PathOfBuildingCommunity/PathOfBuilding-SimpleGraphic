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
#include <OpenGL/gl.h>
#include <GL/glext.h>
#elif __linux__
#include <GL/gl.h>
#include <GL/glext.h>
#else
#define WINGDIAPI
#define APIENTRY __stdcall
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "core/core_image.h"

#include "r_texture.h"
#include "r_font.h"
#include "r_main.h"

