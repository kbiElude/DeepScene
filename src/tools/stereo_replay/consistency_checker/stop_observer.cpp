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

void Stereo::Replay::ConsistencyChecker::StopObserver::on_frame_presented(const uint32_t& in_n_frame)
{
    if (in_n_frame >= m_last_frame)
    {
        m_controller_ptr->pause();
    }
}

