/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_EYE_IMAGE_BUFFER_H
#define STEREO_REPLAY_PLAYER_EYE_IMAGE_BUFFER_H

#include "tools/stereo_replay/engine_dll/types.h"
#include "types.h"
#include <array>
#include <mutex>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* Hands the images of the eyes from the replay thread to the thread that shows them. Both eyes of a frame come out
             * together: the buffer never returns the left image of one frame with the right image of the previous frame. Images are
             * swapped between the threads instead of being copied under the lock, so neither thread waits for the other to copy a frame.
             */
            class EyeImageBuffer
            {
            public:
                enum
                {
                    EYE_LEFT,
                    EYE_RIGHT,
                    N_EYES
                };

                struct Image
                {
                    uint32_t             height = 0;
                    std::vector<uint8_t> pixel_u8_vec; /* 8-bit B, G, R and A per pixel, rows stored one after the other */
                    uint32_t             width  = 0;
                };

                explicit EyeImageBuffer(const EyeMode& in_eye_mode);

                /* Call this when waking up the user failed, so that the next submit() asks for a wake-up again. (The user is woken when
                 * submit() returns true, and then calls take().)
                 */
                void on_post_failed();

                /* Stores the image of one execution of a frame (execution 0 is the left eye if both eyes are shown). Returns true if the
                 * caller must wake up the user: images are waiting to be taken and the user has not been told yet. Replay thread only.
                 */
                bool submit(const uint32_t&                              in_n_execution,
                            const Stereo::Replay::EngineDLL::FrameImage& in_image);

                /* Swaps the images that have not been taken yet into the arrays. Eyes without a new image are left alone, and
                 * out_is_new_array_ptr says which eyes got a new image. Returns false if no eye had a new image.
                 */
                bool take(std::array<Image, N_EYES>* inout_image_array_ptr,
                          std::array<bool,  N_EYES>* out_is_new_array_ptr);

            private:
                /* Lock order: no other lock is taken while it is held. */
                struct Pending
                {
                    std::array<bool, N_EYES>  is_new_array = {};
                    bool                      is_woken     = false;
                    std::array<Image, N_EYES> image_array;
                    std::mutex                mutex;
                };

                EyeMode                   m_eye_mode;
                Pending                   m_pending;
                std::array<Image, N_EYES> m_staging_image_array; /* replay thread only */
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_EYE_IMAGE_BUFFER_H */
