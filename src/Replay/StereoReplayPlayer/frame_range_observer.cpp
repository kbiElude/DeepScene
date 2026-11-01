/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_range_observer.h"

Replay::StereoReplayPlayer::FrameRangeObserver::FrameRangeObserver(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                                                                         const uint32_t&                               in_first_frame,
                                                                         const uint32_t&                               in_last_frame)
    :m_controller_ptr(in_controller_ptr),
     m_first_frame   (in_first_frame),
     m_last_frame    (in_last_frame)
{
    /* Stub */
}

void Replay::StereoReplayPlayer::FrameRangeObserver::on_error(const std::string& in_message)
{
    (void) in_message;
}

void Replay::StereoReplayPlayer::FrameRangeObserver::on_frame_presented(const uint32_t& in_n_frame)
{
    /* The frame before the range was the last one to hurry through. */
    if (m_first_frame  != 0             &&
        in_n_frame + 1 == m_first_frame)
    {
        m_controller_ptr->set_speed(1.0);
    }

    if (in_n_frame == m_last_frame)
    {
        m_controller_ptr->pause();
    }
}

void Replay::StereoReplayPlayer::FrameRangeObserver::on_state_changed(const Replay::StereoReplayEngineDLL::PlaybackState& in_old_state,
                                                                            const Replay::StereoReplayEngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;
    (void) in_new_state;
}

void Replay::StereoReplayPlayer::FrameRangeObserver::on_window_closed()
{
    /* Stub */
}

void Replay::StereoReplayPlayer::FrameRangeObserver::on_window_placed(const Replay::StereoReplayEngineDLL::WindowPlacement& in_placement)
{
    (void) in_placement;
}

void Replay::StereoReplayPlayer::FrameRangeObserver::start()
{
    if (m_first_frame != 0)
    {
        m_controller_ptr->set_speed(0.0);
    }
}
