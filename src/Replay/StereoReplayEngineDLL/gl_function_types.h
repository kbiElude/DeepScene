/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The types of the GL functions (PFNGL...PROC), as the wrappers of the interceptor have them: the core ones, the
 * compatibility ones, and those of the extensions.
 */
#ifndef STEREOREPLAYENGINEDLL_GL_FUNCTION_TYPES_H
#define STEREOREPLAYENGINEDLL_GL_FUNCTION_TYPES_H

#include <Windows.h>
#include "Khronos/GL/glcorearb.h"
#undef GL_VERSION_1_3  /* allow glext.h to define its compat-profile 1.3 typedefs */
#undef GL_VERSION_1_4  /* allow glext.h to define its compat-profile 1.4 typedefs */
#include "Khronos/GL/glext.h"

#define AI_APIENTRY KHRONOS_APIENTRY

#include "Interception/Hooks/OpenGL/types_typedefs.h"

#endif /* STEREOREPLAYENGINEDLL_GL_FUNCTION_TYPES_H */
