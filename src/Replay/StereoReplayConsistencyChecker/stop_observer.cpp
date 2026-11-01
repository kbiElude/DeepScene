/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stop_observer.h"

Replay::StereoReplayConsistencyChecker::StopObserver::StopObserver(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                                                                         const uint32_t&                               in_last_frame)
    :m_controller_ptr(in_controller_ptr),
     m_last_frame    (in_last_frame)
{
    /* Stub */
}

void Replay::StereoReplayConsistencyChecker::StopObserver::on_error(const std::string& in_message)
{
    (void) in_message;
}

void Replay::StereoReplayConsistencyChecker::StopObserver::on_frame_presented(const uint32_t& in_n_frame)
{
    if (in_n_frame >= m_last_frame)
    {
        m_controller_ptr->pause();
    }
}

void Replay::StereoReplayConsistencyChecker::StopObserver::on_state_changed(const Replay::StereoReplayEngineDLL::PlaybackState& in_old_state,
                                                                                  const Replay::StereoReplayEngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;
    (void) in_new_state;
}

void Replay::StereoReplayConsistencyChecker::StopObserver::on_window_closed()
{
    /* Stub */
}

void Replay::StereoReplayConsistencyChecker::StopObserver::on_window_placed(const Replay::StereoReplayEngineDLL::WindowPlacement& in_placement)
{
    (void) in_placement;
}
