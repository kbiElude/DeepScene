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
            /* Says which frames a seek hurries through: those before the frame it goes to. Nothing of them is waited for, shown or presented; what
             * they do to the state of the device is done as it is, since the frames after them are made of it. Set by any thread, read by the replay
             * thread.
             */
            class FrameHurry
            {
            public:
                FrameHurry();

                /* The frames are played as they are again. */
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
