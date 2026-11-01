/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stop_observer.h"

Stereo::Replay::ConsistencyChecker::StopObserver::StopObserver(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                                                               const uint32_t&                                 in_last_frame)
    :m_controller_ptr(in_controller_ptr),
     m_last_frame    (in_last_frame)
{
    /* Stub */
}

void Stereo::Replay::ConsistencyChecker::StopObserver::on_error(const std::string& in_message)
{
    (void) in_message;
}

void Stereo::Replay::ConsistencyChecker::StopObserver::on_frame_presented(const uint32_t& in_n_frame)
{
    if (in_n_frame >= m_last_frame)
    {
        m_controller_ptr->pause();
    }
}

void Stereo::Replay::ConsistencyChecker::StopObserver::on_state_changed(const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                                                        const Stereo::Replay::EngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;
    (void) in_new_state;
}

void Stereo::Replay::ConsistencyChecker::StopObserver::on_window_closed()
{
    /* Stub */
}

void Stereo::Replay::ConsistencyChecker::StopObserver::on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement)
{
    (void) in_placement;
}
