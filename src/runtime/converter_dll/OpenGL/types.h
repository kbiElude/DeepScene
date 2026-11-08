/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_OPENGL_TYPES_H
#define STEREO_CONVERTERDLL_OPENGL_TYPES_H

#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The images of the eyes of a frame of an OpenGL application, as OpenGL reads them: rows from the bottom up, 8 bits each of R, G, B and A. */
        struct GlFrameImages
        {
            uint32_t       height         = 0;
            const uint8_t* left_rgba_ptr  = nullptr;
            const uint8_t* right_rgba_ptr = nullptr;
            uint32_t       width          = 0;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_OPENGL_TYPES_H */
