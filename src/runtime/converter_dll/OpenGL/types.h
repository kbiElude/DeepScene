/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_OPENGL_TYPES_H
#define STEREO_CONVERTERDLL_OPENGL_TYPES_H

#include <memory>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The images of the eyes of a frame of an OpenGL application, as OpenGL reads them: rows from the bottom up, 8 bits each of B, G, R and A (the order of the
         * images of the channel). They are in memory that is mapped from the buffers that the pixels were read into, and stay there while anything holds the token: the
         * one that passes them on keeps a copy of the struct until it is done with them.
         */
        struct GLFrameImages
        {
            std::shared_ptr<const void> consumer_token;
            uint32_t                    height          = 0;
            const uint8_t*              left_bgra_ptr   = nullptr;
            const uint8_t*              right_bgra_ptr  = nullptr;
            uint32_t                    width           = 0;
        };

        /* A glReadPixels of the application that reads one pixel of the colors, in bytes, to memory of its own: the only read that the converter makes itself (see GLResolutionScaler). */
        struct SinglePixelRead
        {
            uint32_t format     = 0;
            void*    pixels_ptr = nullptr;
            uint32_t type       = 0;
            int64_t  x          = 0;
            int64_t  y          = 0;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_OPENGL_TYPES_H */
