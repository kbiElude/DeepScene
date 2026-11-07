/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_hurry.h"

Stereo::Replay::EngineDLL::FrameHurry::FrameHurry()
    :m_has_frame  (false),
     m_until_frame(0)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::FrameHurry::clear()
{
    m_has_frame = false;
}

bool Stereo::Replay::EngineDLL::FrameHurry::is_hurrying(const uint32_t& in_n_frame) const
{
    return (m_has_frame                 &&
            in_n_frame  < m_until_frame);
}

void Stereo::Replay::EngineDLL::FrameHurry::set_until_frame(const uint32_t& in_n_frame)
{
    /* The frame comes first: the other thread looks at the flag, and then at the frame. */
    m_until_frame = in_n_frame;
    m_has_frame   = true;
}
