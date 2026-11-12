/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_PICTURE_READBACK_CACHE_H
#define STEREO_CORE_PICTURE_READBACK_CACHE_H

#include <array>
#include <functional>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Core
    {
        /* Serves the reads of one pixel that an application makes of the picture it renders, which is on the graphics card: each of them is a wait for the card, and an
         * application that samples the picture on a grid makes more than a thousand in a row (about 0.1 ms each, so that they are a frame of 130 ms). The first read of a run is
         * made as it is; at the second the whole picture is read once, and the rest of the run is given from it. A run ends with any call that is not such a read, since
         * what the call does may change the picture. API-neutral: how the picture is read is told by a function.
         */
        class PictureReadbackCache
        {
        public:
            /* Fills the vector with the picture (as many pixels as the width times the height, each R, G, B and A of 8 bits, rows from the bottom up); false if it cannot. */
            typedef std::function<bool(std::vector<uint8_t>*)> FillFunc;

            PictureReadbackCache();

            /* A call that may change the picture was made, or the picture is no longer the one that was read: what was read of it is not used again. */
            void invalidate();

            /* A read of the pixel, which the application names (it may name one that is outside of the picture: the nearest one in it is read). Returns true, and the pixel,
             * if it is given from the picture that was read; false if the read is to be made as it is.
             */
            bool read_pixel(const FillFunc&         in_fill_func,
                            const uint32_t&         in_picture_width,
                            const uint32_t&         in_picture_height,
                            const int64_t&          in_x,
                            const int64_t&          in_y,
                            std::array<uint8_t, 4>* out_rgba_u8vec4_ptr);

        private:
            bool                 m_has_failed;         /* the picture could not be read in this run, which is not tried again          */
            uint32_t             m_height;
            bool                 m_is_valid;           /* the picture that is kept is the one that the application sees               */
            uint32_t             m_n_reads;            /* of the run                                                                  */
            std::vector<uint8_t> m_rgba_u8_vec;
            uint32_t             m_width;
        };
    }
}

#endif /* STEREO_CORE_PICTURE_READBACK_CACHE_H */
