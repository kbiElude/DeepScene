/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYPLAYER_FRAME_RANGE_OBSERVER_H
#define STEREOREPLAYPLAYER_FRAME_RANGE_OBSERVER_H

#include "Replay/StereoReplayEngineDLL/i_playback_controller.h"
#include "Replay/StereoReplayEngineDLL/i_replay_observer.h"

namespace Replay
{
    namespace StereoReplayPlayer
    {
        /* Makes the replay go unpaced up to the first frame of a range, play the range at the recorded pace, and pause after
         * its last frame. Playing on afterwards is not stopped again.
         */
        class FrameRangeObserver : public Replay::StereoReplayEngineDLL::IReplayObserver
        {
        public:
            FrameRangeObserver(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                                     const uint32_t&                               in_first_frame,
                                     const uint32_t&                               in_last_frame);

            /* Must be called before the replay is played. */
            void start();

            void on_error          (const std::string&                              in_message)   override;
            void on_frame_presented(const uint32_t&                                 in_n_frame)   override;
            void on_state_changed  (const Replay::StereoReplayEngineDLL::PlaybackState&   in_old_state,
                                          const Replay::StereoReplayEngineDLL::PlaybackState&   in_new_state) override;
            void on_window_closed  ()                                                             override;
            void on_window_placed  (const Replay::StereoReplayEngineDLL::WindowPlacement& in_placement) override;

        private:
            Replay::StereoReplayEngineDLL::IPlaybackController* m_controller_ptr;
            uint32_t                                      m_first_frame;
            uint32_t                                      m_last_frame;
        };
    }
}

#endif /* STEREOREPLAYPLAYER_FRAME_RANGE_OBSERVER_H */
