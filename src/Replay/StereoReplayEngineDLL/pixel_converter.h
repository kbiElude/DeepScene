/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_PIXEL_CONVERTER_H
#define STEREOREPLAYENGINEDLL_PIXEL_CONVERTER_H

#include "Interception/Hooks/D3D8/types.h"
#include <stdint.h>
#include <vector>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        class PixelConverter
        {
        public:
            /* Converts an image of a Direct3D 8 format (D3DFORMAT) to B, G, R and A (always 255) of 8 bits for each pixel, rows one
             * after the other with no padding. Returns false, and leaves the output empty, for a format that is not converted.
             */
            static bool convert_to_bgra8(const uint32_t&       in_format,
                                         const uint32_t&       in_width,
                                         const uint32_t&       in_height,
                                         const uint32_t&       in_pitch,
                                         const uint8_t*        in_pixel_ptr,
                                         std::vector<uint8_t>* out_pixel_u8_vec_ptr);

            /* The size of a pixel of a format that is converted; 0 for any other. */
            static uint32_t get_bytes_per_pixel(const uint32_t& in_format);

        private:
            /* Where a color channel is in a pixel. */
            struct Channel
            {
                uint32_t n_bits;
                uint32_t shift;
            };

            struct PixelLayout
            {
                Channel                              blue;
                Interception::Hooks::D3D8::D3DFORMAT format;
                Channel                              green;
                uint32_t                             n_bytes;
                Channel                              red;
            };


            static const PixelLayout*              find_layout_ptr(const uint32_t& in_format);
            static const std::vector<PixelLayout>& get_layout_vec ();

            /* The pixel of the output for every value of a 16-bit pixel. */
            static const std::vector<uint32_t>& get_lookup_vec(const PixelLayout& in_layout);
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_PIXEL_CONVERTER_H */
