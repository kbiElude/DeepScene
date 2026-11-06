/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Picks the frames that a contact sheet shows.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_SAMPLER_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_SAMPLER_H

#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            class FrameSampler
            {
            public:
                /* The frames, in order, spread evenly from the first to the last (both are in it, unless only one is asked for: then it is the first). With
                 * fewer frames in the range than asked for, all of them. None if none is asked for, or the first is after the last.
                 */
                static std::vector<uint32_t> sample(const uint32_t& in_first_frame,
                                                    const uint32_t& in_last_frame,
                                                    const uint32_t& in_n_frames);
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_SAMPLER_H */
