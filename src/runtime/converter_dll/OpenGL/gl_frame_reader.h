/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_FRAME_READER_H
#define STEREO_CONVERTERDLL_GL_FRAME_READER_H

#include "gl_eye_targets.h"
#include "types.h"
#include <array>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Reads the images of the eyes of a frame out of their framebuffers without making the application wait for the graphics card: the pixels are read into buffers
         * (which the card fills while the application goes on), and the frame is given a frame later, from the buffers of the frame before, which are mapped. There are two
         * sets of buffers; a set is not used again while what was given from it is held (see GlFrameImages::consumer_token). Without buffers the images are read at once.
         * Calls are made with the context of the application current, and the converter's own.
         */
        class GlFrameReader
        {
        public:
            GlFrameReader();

            /* Starts reading the frame that is ready in the framebuffers, and gives the one that was started the frame before (with the buffers mapped), or if the
             * buffers cannot be used the frame that is ready (read at once). False if there is none to give: the first frame, or a size that changed.
             */
            bool read(GlEyeTargets*  inout_targets_ptr,
                      GlFrameImages* out_images_ptr);

        private:
            static constexpr size_t N_EYES = 2;
            static constexpr size_t N_SETS = 2;

            /* What a frame is read into. */
            struct Set
            {
                std::array<uint32_t, N_EYES>             buffer_array       = {};
                std::weak_ptr<const void>                consumer;               /* held by what has been given of the set */
                bool                                     has_frame          = false; /* the pixels of a frame are on their way into the buffers */
                uint32_t                                 height             = 0;
                bool                                     is_mapped          = false;
                std::array<const uint8_t*, N_EYES>       mapped_ptr_array   = {};
                std::array<std::vector<uint8_t>, N_EYES> pixel_u8_vec_array;     /* without buffers */
                uint32_t                                 width              = 0;
            };

            /* Waits until what was given of the set is not held, and unmaps its buffers. */
            void finish_set(const GlEyeTargets& in_targets,
                            Set*                inout_set_ptr);

            /* Finishes the set, and gives its buffers back. */
            void release_set(const GlEyeTargets& in_targets,
                             Set*                inout_set_ptr);

            uint64_t                m_n_reads;
            std::array<Set, N_SETS> m_set_array;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_FRAME_READER_H */
