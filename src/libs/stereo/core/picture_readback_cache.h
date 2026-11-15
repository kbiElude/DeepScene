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
        /* Serves the single-pixel reads that an application makes of the picture it renders. The picture is on the graphics card, so each
         * read waits for the card (about 0.1 ms). An application that samples the picture on a grid makes more than a thousand such reads in
         * a row, which adds up to a frame of 130 ms. The cache makes the first read of a run directly. At the second read it reads the
         * whole picture once and serves the rest of the run from that copy. Any call that is not such a read ends the run, because the call
         * may change the picture. API-neutral: a function supplied by the caller reads the picture.
         */
        class PictureReadbackCache
        {
        public:
            /* Fills the vector with the picture: width times height pixels, each stored as 8-bit R, G, B and A, with the rows ordered from the
             * bottom up. Returns false if the picture cannot be read.
             */
            typedef std::function<bool(std::vector<uint8_t>*)> FillFunc;

            PictureReadbackCache();

            /* Call this when the application made a call that may change the picture, or when the picture is no longer the one that was read.
             * The stored copy is not used again.
             */
            void invalidate();

            /* Handles a read of the pixel (in_x, in_y) that the application names. The pixel may lie outside the picture. In that case the
             * nearest pixel inside the picture is read. Returns true and the pixel if it is served from the stored copy of the picture.
             * Returns false if the caller has to make the read itself.
             */
            bool read_pixel(const FillFunc&         in_fill_func,
                            const uint32_t&         in_picture_width,
                            const uint32_t&         in_picture_height,
                            const int64_t&          in_x,
                            const int64_t&          in_y,
                            std::array<uint8_t, 4>* out_rgba_u8vec4_ptr);

        private:
            bool                 m_has_failed;         /* the picture could not be read in this run, and it is not tried again          */
            uint32_t             m_height;
            bool                 m_is_valid;           /* the stored picture is the one that the application sees                       */
            uint32_t             m_n_reads;            /* the number of reads in the current run                                        */
            std::vector<uint8_t> m_rgba_u8_vec;
            uint32_t             m_width;
        };
    }
}

#endif /* STEREO_CORE_PICTURE_READBACK_CACHE_H */
