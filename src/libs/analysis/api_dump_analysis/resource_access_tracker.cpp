/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include <algorithm>

Analysis::APIDumpAnalysis::ResourceAccessTracker::ResourceAccessTracker()
{
    /* Stub */
}

Analysis::APIDumpAnalysis::ResourceAccessClass Analysis::APIDumpAnalysis::ResourceAccessTracker::classify(const ResourceAccess& in_access)
{
    ResourceAccessClass result = ResourceAccessClass::FRAME_LOCAL;

    if (in_access.n_frames_loop_carried > 0)
    {
        result = ResourceAccessClass::LOOP_CARRIED;
    }
    else
    if (in_access.n_frames_read    == 0 &&
        in_access.n_frames_written == 0)
    {
        result = ResourceAccessClass::UNUSED;
    }
    else
    if (in_access.n_frames_read == 0)
    {
        result = ResourceAccessClass::UNREAD;
    }
    else
    if (in_access.n_frames_written       == 0                                ||
        in_access.last_write_frame_index <= in_access.first_read_frame_index)
    {
        result = ResourceAccessClass::STATIC;
    }

    return result;
}

std::map<Analysis::APIDumpAnalysis::ResourceAccessClass, uint64_t> Analysis::APIDumpAnalysis::ResourceAccessTracker::count_by_class(const std::vector<ResourceAccess>& in_access_vec)
{
    std::map<ResourceAccessClass, uint64_t> result_map =
    {
        {ResourceAccessClass::FRAME_LOCAL,  0},
        {ResourceAccessClass::LOOP_CARRIED, 0},
        {ResourceAccessClass::STATIC,       0},
        {ResourceAccessClass::UNREAD,       0},
        {ResourceAccessClass::UNUSED,       0}
    };

    for (const auto& access : in_access_vec)
    {
        result_map.at(classify(access) )++;
    }

    return result_map;
}

void Analysis::APIDumpAnalysis::ResourceAccessTracker::end_frame(const uint64_t&              in_frame_index,
                                                                 std::vector<ResourceAccess>* inout_access_vec_ptr,
                                                                 std::vector<ResourceId>*     out_loop_carried_id_vec_ptr)
{
    out_loop_carried_id_vec_ptr->clear();

    /* The order of the touches is the order of the first access; the IDs are listed in their own. */
    std::sort(m_touched_vec.begin(),
              m_touched_vec.end() );

    for (const auto& index : m_touched_vec)
    {
        const FrameAccess frame_access = m_frame_access_vec.at   (index);
        ResourceAccess&   access       = inout_access_vec_ptr->at(index);

        if (frame_access.has_read)
        {
            if (access.n_frames_read == 0)
            {
                access.first_read_frame_index = in_frame_index;
            }

            access.n_frames_read++;
        }

        if (frame_access.has_write)
        {
            if (access.n_frames_written == 0)
            {
                access.first_write_frame_index = in_frame_index;
            }

            access.last_write_frame_index = in_frame_index;

            access.n_frames_written++;
        }

        if (frame_access.has_read_before_write &&
            frame_access.has_write)
        {
            access.n_frames_loop_carried++;

            out_loop_carried_id_vec_ptr->push_back(make_resource_id(index + 1) );
        }

        m_frame_access_vec.at(index) = FrameAccess();
    }

    m_touched_vec.clear();
}

Analysis::APIDumpAnalysis::ResourceAccessTracker::FrameAccess* Analysis::APIDumpAnalysis::ResourceAccessTracker::get_frame_access_ptr(const ResourceId& in_id)
{
    const uint32_t id_value   = get_resource_id_value(in_id);
    FrameAccess*   result_ptr = nullptr;

    if (id_value != 0)
    {
        if (m_frame_access_vec.size() < id_value)
        {
            m_frame_access_vec.resize(id_value);
        }

        result_ptr = &m_frame_access_vec.at(id_value - 1);
    }

    return result_ptr;
}

void Analysis::APIDumpAnalysis::ResourceAccessTracker::on_read(const ResourceId& in_id)
{
    FrameAccess* access_ptr = get_frame_access_ptr(in_id);

    if (access_ptr != nullptr)
    {
        if (access_ptr->has_read  == false &&
            access_ptr->has_write == false)
        {
            m_touched_vec.push_back(get_resource_id_value(in_id) - 1);
        }

        if (access_ptr->has_write == false)
        {
            access_ptr->has_read_before_write = true;
        }

        access_ptr->has_read = true;
    }
}

void Analysis::APIDumpAnalysis::ResourceAccessTracker::on_write(const ResourceId& in_id)
{
    FrameAccess* access_ptr = get_frame_access_ptr(in_id);

    if (access_ptr != nullptr)
    {
        if (access_ptr->has_read  == false &&
            access_ptr->has_write == false)
        {
            m_touched_vec.push_back(get_resource_id_value(in_id) - 1);
        }

        access_ptr->has_write = true;
    }
}
