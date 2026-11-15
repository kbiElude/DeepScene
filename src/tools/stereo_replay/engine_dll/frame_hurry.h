/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_FRAME_HURRY_H
#define STEREO_REPLAY_ENGINEDLL_FRAME_HURRY_H

#include <atomic>
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Tells which frames a seek hurries through: the frames before the frame the seek goes to. A hurried frame is not waited
             * for, shown or presented. Its calls still change the device state, because the later frames depend on that state.
             * Set from any thread, read by the replay thread.
             */
            class FrameHurry
            {
            public:
                FrameHurry();

                /* Stops hurrying: all frames play normally again. */
                void clear();

                /* The frames before this one (counted from 0) are hurried through. */
                void set_until_frame(const uint32_t& in_n_frame);

                bool is_hurrying(const uint32_t& in_n_frame) const;

            private:
                std::atomic<bool>     m_has_frame;
                std::atomic<uint32_t> m_until_frame;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_FRAME_HURRY_H */
