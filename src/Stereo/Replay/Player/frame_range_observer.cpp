/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_range_observer.h"

Stereo::Replay::Player::FrameRangeObserver::FrameRangeObserver(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                                                               const uint32_t&                                 in_first_frame,
                                                               const uint32_t&                                 in_last_frame)
    :m_controller_ptr(in_controller_ptr),
     m_first_frame   (in_first_frame),
     m_last_frame    (in_last_frame)
{
    /* Stub */
}

void Stereo::Replay::Player::FrameRangeObserver::on_error(const std::string& in_message)
{
    (void) in_message;
}

void Stereo::Replay::Player::FrameRangeObserver::on_frame_presented(const uint32_t& in_n_frame)
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

void Stereo::Replay::Player::FrameRangeObserver::on_state_changed(const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                                                  const Stereo::Replay::EngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;
    (void) in_new_state;
}

void Stereo::Replay::Player::FrameRangeObserver::on_window_closed()
{
    /* Stub */
}

void Stereo::Replay::Player::FrameRangeObserver::on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement)
{
    (void) in_placement;
}

void Stereo::Replay::Player::FrameRangeObserver::start()
{
    if (m_first_frame != 0)
    {
        m_controller_ptr->set_speed(0.0);
    }
}
