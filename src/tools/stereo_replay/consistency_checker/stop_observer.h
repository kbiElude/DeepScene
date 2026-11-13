/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_STOP_OBSERVER_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_STOP_OBSERVER_H

#include "tools/stereo_replay/engine_dll/i_playback_controller.h"
#include "tools/stereo_replay/engine_dll/null_replay_observer.h"

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Pauses the replay after the last frame of the check. */
            class StopObserver : public Stereo::Replay::EngineDLL::NullReplayObserver
            {
            public:
                StopObserver(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                             const uint32_t&                                 in_last_frame);

                void on_frame_presented(const uint32_t& in_n_frame) override;

            private:
                Stereo::Replay::EngineDLL::IPlaybackController* m_controller_ptr;
                uint32_t                                        m_last_frame;
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_STOP_OBSERVER_H */
