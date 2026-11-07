/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/access_log.h"

Analysis::APIDumpDependencies::AccessLog::AccessLog()
{
    /* Stub */
}

const std::vector<Analysis::APIDumpAnalysis::AccessEvent>& Analysis::APIDumpDependencies::AccessLog::get_event_vec() const
{
    return m_event_vec;
}

const std::vector<Analysis::APIDumpDependencies::FrameCallRange>& Analysis::APIDumpDependencies::AccessLog::get_frame_call_range_vec() const
{
    return m_frame_call_range_vec;
}

const std::vector<uint64_t>& Analysis::APIDumpDependencies::AccessLog::get_frame_end_event_count_vec() const
{
    return m_frame_end_event_count_vec;
}

void Analysis::APIDumpDependencies::AccessLog::on_access(const Analysis::APIDumpAnalysis::AccessEvent& in_event)
{
    m_event_vec.push_back(in_event);
}

void Analysis::APIDumpDependencies::AccessLog::on_frame_ended(const uint64_t& in_frame_index,
                                                              const uint64_t& in_first_call_index,
                                                              const uint64_t& in_last_call_index)
{
    /* The frames are told in order, from 0, so that the position in the vector is the number of the frame. */
    if (in_frame_index == m_frame_end_event_count_vec.size() )
    {
        FrameCallRange call_range;

        call_range.first_call_index = in_first_call_index;
        call_range.last_call_index  = in_last_call_index;

        m_frame_call_range_vec.push_back    (call_range);
        m_frame_end_event_count_vec.push_back(m_event_vec.size() );
    }
}
