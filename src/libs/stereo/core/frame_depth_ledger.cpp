/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/frame_depth_ledger.h"
#include <algorithm>

Stereo::Core::FrameDepthLedger::FrameDepthLedger()
    :m_frame             (0),
     m_oldest_kept_frame (0)
{
    /* Stub */
}

void Stereo::Core::FrameDepthLedger::add_pending(const uint32_t& in_slot,
                                                 const float&    in_x_scale,
                                                 const float&    in_z_scale,
                                                 const float&    in_z_shift)
{
    PendingDepthRead read;

    read.frame   = m_frame;
    read.slot    = in_slot;
    read.x_scale = in_x_scale;
    read.z_scale = in_z_scale;
    read.z_shift = in_z_shift;

    m_pending_read_deque.push_back(read);
}

void Stereo::Core::FrameDepthLedger::add_result(const PendingDepthRead& in_read,
                                                const DepthRange&       in_range)
{
    /* Frames up to the one that was handed out are gone, so a result for one of them is dropped. */
    if (in_read.frame >= m_oldest_kept_frame)
    {
        add_to_frame(in_read.frame,
                     in_range,
                     in_read.x_scale);
    }
}

void Stereo::Core::FrameDepthLedger::add_result_now(const DepthRange& in_range,
                                                    const float&      in_x_scale)
{
    add_to_frame(m_frame,
                 in_range,
                 in_x_scale);
}

void Stereo::Core::FrameDepthLedger::add_to_frame(const uint64_t&   in_frame,
                                                  const DepthRange& in_range,
                                                  const float&      in_x_scale)
{
    FrameDepth& depth = m_frame_map[in_frame];

    if (depth.has_depth == false)
    {
        depth.has_depth = true;
        depth.range     = in_range;
    }
    else
    {
        depth.range.far_depth  = (std::max)(depth.range.far_depth,
                                            in_range.far_depth);
        depth.range.near_depth = (std::min)(depth.range.near_depth,
                                            in_range.near_depth);
    }

    depth.median_vec.push_back(in_range.median_depth);

    depth.x_scale = in_x_scale;
}

void Stereo::Core::FrameDepthLedger::end_frame(const uint32_t& in_lag)
{
    /* A ready depth that was not taken is dropped. */
    m_ready = FrameDepth();

    if (m_frame >= in_lag)
    {
        const uint64_t ready_frame = m_frame - in_lag;
        const auto     ready_it    = m_frame_map.find(ready_frame);
        const auto     done_end_it = m_frame_map.upper_bound(ready_frame);

        if (ready_it != m_frame_map.end() )
        {
            m_ready = std::move(ready_it->second);
        }

        /* Frames up to and including the ready frame are no longer needed. */
        m_frame_map.erase(m_frame_map.begin(),
                          done_end_it);

        m_oldest_kept_frame = ready_frame + 1;
    }

    m_frame++;
}

void Stereo::Core::FrameDepthLedger::forget_pending()
{
    m_pending_read_deque.clear();
}

size_t Stereo::Core::FrameDepthLedger::get_n_pending() const
{
    return m_pending_read_deque.size();
}

bool Stereo::Core::FrameDepthLedger::pop_pending(const bool&       in_only_older_frames,
                                                 PendingDepthRead* out_read_ptr)
{
    bool result = false;

    if (m_pending_read_deque.empty() == false)
    {
        const PendingDepthRead& oldest = m_pending_read_deque.front();

        if (in_only_older_frames == false ||
            oldest.frame         <  m_frame)
        {
            *out_read_ptr = oldest;

            m_pending_read_deque.pop_front();

            result = true;
        }
    }

    return result;
}

bool Stereo::Core::FrameDepthLedger::take_ready(DepthRange* out_range_ptr,
                                                float*      out_x_scale_ptr)
{
    const bool result = m_ready.has_depth;

    if (result)
    {
        *out_range_ptr = m_ready.range;

        /* The median of the passes' medians. */
        std::sort(m_ready.median_vec.begin(),
                  m_ready.median_vec.end  () );

        out_range_ptr->median_depth = m_ready.median_vec.at(m_ready.median_vec.size() / 2);
        *out_x_scale_ptr            = m_ready.x_scale;

        m_ready = FrameDepth();
    }

    return result;
}
