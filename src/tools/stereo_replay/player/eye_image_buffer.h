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
            /* Hands the images of the eyes from the replay thread to the thread which shows them. Both eyes of a frame come out together,
             * never the left one of a frame with the right one of the frame before. Images are swapped between the threads, not copied
             * under the lock, so a thread never waits for the other one to copy a frame.
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
                    std::vector<uint8_t> pixel_u8_vec; /* B, G, R and A of 8 bits, rows one after the other */
                    uint32_t             width  = 0;
                };

                explicit EyeImageBuffer(const EyeMode& in_eye_mode);

                /* The user of the buffer wakes up when a submit() says so, and calls take() then. */
                void on_post_failed();

                /* The image of an execution of a frame (the first is the left eye's, if both eyes are shown). Returns true if the user has to
                 * be woken up: images wait to be taken, and the user has not been told yet. Replay thread only.
                 */
                bool submit(const uint32_t&                              in_n_execution,
                            const Stereo::Replay::EngineDLL::FrameImage& in_image);

                /* Swaps the images which have not been taken into the arrays (the eyes with no new image are left alone), and says which.
                 * Returns false if there was none.
                 */
                bool take(std::array<Image, N_EYES>* inout_image_array_ptr,
                          std::array<bool, N_EYES>*  out_is_new_array_ptr);

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
