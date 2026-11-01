/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYCONSISTENCYCHECKER_STOP_OBSERVER_H
#define STEREOREPLAYCONSISTENCYCHECKER_STOP_OBSERVER_H

#include "Replay/StereoReplayEngineDLL/i_playback_controller.h"
#include "Replay/StereoReplayEngineDLL/i_replay_observer.h"

namespace Replay
{
    namespace StereoReplayConsistencyChecker
    {
        /* Pauses the replay after the last frame of the check. */
        class StopObserver : public Replay::StereoReplayEngineDLL::IReplayObserver
        {
        public:
            StopObserver(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                               const uint32_t&                               in_last_frame);

            void on_error          (const std::string&                              in_message)   override;
            void on_frame_presented(const uint32_t&                                 in_n_frame)   override;
            void on_state_changed  (const Replay::StereoReplayEngineDLL::PlaybackState&   in_old_state,
                                          const Replay::StereoReplayEngineDLL::PlaybackState&   in_new_state) override;
            void on_window_closed  ()                                                             override;
            void on_window_placed  (const Replay::StereoReplayEngineDLL::WindowPlacement& in_placement) override;

        private:
            Replay::StereoReplayEngineDLL::IPlaybackController* m_controller_ptr;
            uint32_t                                      m_last_frame;
        };
    }
}

#endif /* STEREOREPLAYCONSISTENCYCHECKER_STOP_OBSERVER_H */
