/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "hurried_frame_selector.h"

Stereo::Replay::EngineDLL::HurriedFrameSelector::HurriedFrameSelector()
    :m_has_frame  (false),
     m_until_frame(0)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::HurriedFrameSelector::clear()
{
    m_has_frame = false;
}

bool Stereo::Replay::EngineDLL::HurriedFrameSelector::is_hurrying(const uint32_t& in_n_frame) const
{
    return (m_has_frame                 &&
            in_n_frame  < m_until_frame);
}

void Stereo::Replay::EngineDLL::HurriedFrameSelector::set_until_frame(const uint32_t& in_n_frame)
{
    /* Store the frame first: the replay thread reads the flag, then the frame. */
    m_until_frame = in_n_frame;
    m_has_frame   = true;
}
