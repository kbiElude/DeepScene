/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_ZERO_FILLED_PIXELS_H
#define STEREO_CONVERTERDLL_GL_ZERO_FILLED_PIXELS_H

#include <stddef.h>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Pixels that are all zero. A program that makes a texture to render to, by copying from the framebuffer, often gives its storage with a
         * buffer of zeros instead of a null pointer. The pixels carry nothing, and the copy overwrites them, so the converter treats such an upload
         * as storage without pixels (see GLResolutionScaler). This class holds the parts of that which need no driver.
         */
        class GLZeroFilledPixels
        {
        public:
            /* The size of a buffer of zeros that an upload of the scaled size reads, if it has the pixel layout of an upload of in_n_source_bytes bytes
             * and in_n_source_pixels pixels: the bytes of a pixel rounded up, and each row padded to the largest alignment that the unpacking can have.
             * Returns 0 if the source has no pixels.
             */
            static size_t get_n_scaled_bytes(const uint32_t& in_n_source_bytes,
                                             const uint64_t& in_n_source_pixels,
                                             const uint32_t& in_scaled_width,
                                             const uint32_t& in_scaled_height);

            /* Whether all the bytes are zero. False if there are none. */
            static bool is_zero_filled(const void*     in_data_ptr,
                                       const uint32_t& in_n_bytes);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_ZERO_FILLED_PIXELS_H */
