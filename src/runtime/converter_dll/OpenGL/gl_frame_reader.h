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
        /* Reads the eye images of a frame from their framebuffers without making the application wait for the graphics card. The pixels are
         * read into buffers, which the card fills while the application goes on. The frame is handed out one frame later, from the buffers of the
         * previous frame, which are mapped for reading. There are two sets of buffers. A set is not reused while something still holds the frame
         * that was handed out from it (see GLFrameImages::consumer_token). If buffers are unavailable, the images are read immediately. The reader
         * makes its OpenGL calls with the application's context current, and they are the converter's own calls.
         */
        class GLFrameReader
        {
        public:
            GLFrameReader();

            /* Starts reading the frame that is ready in the framebuffers. Returns the frame whose read started one frame earlier, with its buffers
             * mapped. If buffers cannot be used, it returns the frame that is ready, read immediately. Returns false if there is no frame to
             * return: on the first frame, or after the size changed.
             */
            bool read(GLEyeTargets*  inout_targets_ptr,
                      GLFrameImages* out_images_ptr);

        private:
            static constexpr size_t N_EYES = 2;
            static constexpr size_t N_SETS = 2;

            static const GLenum PIXEL_PACK_BUFFER;

            /* The buffers that one frame is read into. */
            struct Set
            {
                std::array<uint32_t, N_EYES>             buffer_array       = {};
                std::weak_ptr<const void>                consumer;               /* held by whatever was handed out from this set */
                bool                                     has_frame          = false; /* a frame is being read into the buffers */
                uint32_t                                 height             = 0;
                bool                                     is_mapped          = false;
                std::array<const uint8_t*, N_EYES>       mapped_ptr_array   = {};
                std::array<std::vector<uint8_t>, N_EYES> pixel_u8_vec_array;     /* used when there are no buffers */
                uint32_t                                 width              = 0;
            };

            /* Waits until nothing holds what was handed out from the set, then unmaps its buffers. */
            void finish_set(const GLEyeTargets& in_targets,
                            Set*                inout_set_ptr);

            /* Finishes the set, then deletes its buffers. */
            void release_set(const GLEyeTargets& in_targets,
                             Set*                inout_set_ptr);

            uint64_t                m_n_reads;
            std::array<Set, N_SETS> m_set_array;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_FRAME_READER_H */
