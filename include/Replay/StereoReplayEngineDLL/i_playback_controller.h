/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_I_PLAYBACK_CONTROLLER_H
#define STEREOREPLAYENGINEDLL_I_PLAYBACK_CONTROLLER_H

#include "Replay/StereoReplayEngineDLL/types.h"

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Controls a replay from any thread. The functions return at once; a request takes effect at the next frame
         * boundary of the replay (at once, if the replay is not running). Observers tell when it did.
         */
        class IPlaybackController
        {
        public:
            virtual ~IPlaybackController() = default;

            virtual PlaybackStatus get_status() const = 0;

            /* Stops at the next frame boundary. */
            virtual void pause() = 0;

            /* Starts, or resumes. Does nothing once the replay has finished or failed. */
            virtual void play() = 0;

            /* Goes back to the start of the workload, replays its first frame, and pauses there until play(). The frame is
             * rendered, so it can be seen. Takes effect at the next frame boundary if the replay is running.
             */
            virtual void reset() = 0;

            /* The pace of the replay: 1 is the pace at which the dumped process presented its frames, 2 is twice as fast, 0 is as
             * fast as the machine can go. Takes effect at once.
             */
            virtual void set_speed(const double& in_speed) = 0;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_I_PLAYBACK_CONTROLLER_H */
