/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_FRAME_RANGE_OBSERVER_H
#define STEREO_REPLAY_PLAYER_FRAME_RANGE_OBSERVER_H

#include "tools/stereo_replay/engine_dll/i_playback_controller.h"
#include "tools/stereo_replay/engine_dll/null_replay_observer.h"

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* Runs the replay as fast as possible up to the first frame of a range, plays the range at the recorded pace, and pauses
             * after the last frame of the range. If playing continues afterwards, the observer does not pause it again.
             */
            class FrameRangeObserver : public Stereo::Replay::EngineDLL::NullReplayObserver
            {
            public:
                FrameRangeObserver(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                                   const uint32_t&                                 in_first_frame,
                                   const uint32_t&                                 in_last_frame);

                void on_frame_presented(const uint32_t& in_n_frame) override;

                /* Must be called before the replay is played. */
                void start();

            private:
                Stereo::Replay::EngineDLL::IPlaybackController* m_controller_ptr;
                uint32_t                                        m_first_frame;
                uint32_t                                        m_last_frame;
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_FRAME_RANGE_OBSERVER_H */
