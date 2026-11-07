/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/dependency_analyzer.h"
#include "utils/parallel/parallel_for.h"
#include <algorithm>

bool Analysis::APIDumpDependencies::operator==(const FrameUse& in_a,
                                               const FrameUse& in_b)
{
    return (in_a.carry            == in_b.carry            &&
            in_a.is_fully_written == in_b.is_fully_written &&
            in_a.is_read          == in_b.is_read          &&
            in_a.is_self_sampled  == in_b.is_self_sampled  &&
            in_a.is_written       == in_b.is_written);
}

Analysis::APIDumpDependencies::DependencyResult Analysis::APIDumpDependencies::DependencyAnalyzer::analyze(const AccessLog&                                        in_log,
                                                                                                           const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec,
                                                                                                           const uint32_t&                                         in_n_threads)
{
    const auto& event_vec           = in_log.get_event_vec                ();
    const auto& frame_end_count_vec = in_log.get_frame_end_event_count_vec();

    std::vector<uint32_t> event_index_vec(event_vec.size());
    std::vector<uint32_t> frame_index_vec(event_vec.size());
    std::vector<size_t>   first_event_vec(in_resource_vec.size() + 1,
                                          0);

    DependencyResult result;

    /* The frame of each access: the accesses up to the end of frame N are of frames up to N. */
    {
        size_t n_event = 0;

        for (size_t n_frame = 0;
                    n_frame < frame_end_count_vec.size();
                  ++n_frame)
        {
            for (;
                 n_event < frame_end_count_vec.at(n_frame);
               ++n_event)
            {
                frame_index_vec.at(n_event) = static_cast<uint32_t>(n_frame);
            }
        }

        /* What comes after the last end belongs to a frame which did not end. */
        for (;
             n_event < event_vec.size();
           ++n_event)
        {
            frame_index_vec.at(n_event) = static_cast<uint32_t>(frame_end_count_vec.size() );
        }

        const uint64_t n_events_in_ended_frames = (frame_end_count_vec.empty() ) ? 0
                                                                                 : frame_end_count_vec.back();

        result.n_frames = frame_end_count_vec.size();

        if (event_vec.size() > n_events_in_ended_frames)
        {
            result.n_frames++;
        }
    }

    /* The accesses by resource, each in the order it was made in (a counting sort, which is stable). */
    for (const auto& event : event_vec)
    {
        const uint32_t id_value = Analysis::APIDumpAnalysis::get_resource_id_value(event.resource_id);

        if (id_value >= 1                      &&
            id_value <= in_resource_vec.size() )
        {
            first_event_vec.at(id_value)++;
        }
    }

    for (size_t n_resource = 0;
                n_resource < in_resource_vec.size();
              ++n_resource)
    {
        first_event_vec.at(n_resource + 1) += first_event_vec.at(n_resource);
    }

    {
        std::vector<size_t> next_event_vec(first_event_vec.begin(),
                                           first_event_vec.end() - 1);

        for (size_t n_event = 0;
                    n_event < event_vec.size();
                  ++n_event)
        {
            const uint32_t id_value = Analysis::APIDumpAnalysis::get_resource_id_value(event_vec.at(n_event).resource_id);

            if (id_value >= 1                      &&
                id_value <= in_resource_vec.size() )
            {
                event_index_vec.at(next_event_vec.at(id_value - 1)++) = static_cast<uint32_t>(n_event);
            }
        }
    }

    result.resource_vec.resize(in_resource_vec.size() );

    const auto analyze_resources_func = [&](const uint64_t& in_first_resource,
                                            const uint64_t& in_end_resource)
    {
        for (uint64_t n_resource  = in_first_resource;
                      n_resource  < in_end_resource;
                    ++n_resource)
        {
            ResourceAccesses accesses;

            /* The range is of the indices of a vector, so it fits size_t on a 32-bit build too. */
            const size_t    resource_index = static_cast<size_t>(n_resource);

            accesses.event_index_ptr = event_index_vec.data() + first_event_vec.at(resource_index);
            accesses.n_events        = first_event_vec.at(resource_index + 1) - first_event_vec.at(resource_index);

            analyze_resource(in_log,
                             frame_index_vec,
                             accesses,
                             in_resource_vec.at(resource_index).description,
                            &result.resource_vec.at(resource_index) );
        }
    };

    Utils::Parallel::ParallelFor::run(in_resource_vec.size(),
                                      in_n_threads,
                                      analyze_resources_func);

    return result;
}

void Analysis::APIDumpDependencies::DependencyAnalyzer::analyze_resource(const AccessLog&                                      in_log,
                                                                         const std::vector<uint32_t>&                          in_frame_index_vec,
                                                                         const ResourceAccesses&                               in_accesses,
                                                                         const Analysis::APIDumpAnalysis::ResourceDescription& in_description,
                                                                         ResourceDependencies*                                 out_dependencies_ptr)
{
    const auto&      event_vec = in_log.get_event_vec();
    size_t           n_access  = 0;
    ResourceSummary& summary   = out_dependencies_ptr->summary;

    while (n_access < in_accesses.n_events)
    {
        const uint32_t frame_index       = in_frame_index_vec.at(in_accesses.event_index_ptr[n_access]);
        size_t         end_access        = n_access;
        FrameUse       use;
        bool           has_partial_write = false;
        bool           is_replaced       = false;
        uint64_t       call_index        = 0;
        bool           call_has_sampled  = false;
        bool           call_has_write    = false;

        /* The accesses of the resource in this frame. */
        while (end_access                                                     <  in_accesses.n_events &&
               in_frame_index_vec.at(in_accesses.event_index_ptr[end_access]) == frame_index)
        {
            const Analysis::APIDumpAnalysis::AccessEvent& event    = event_vec.at(in_accesses.event_index_ptr[end_access]);
            const bool                                    is_read  = (event.mode != Analysis::APIDumpAnalysis::AccessMode::WRITE);
            const bool                                    is_write = (event.mode != Analysis::APIDumpAnalysis::AccessMode::READ);

            if (end_access == n_access ||
                event.call_index != call_index)
            {
                call_index       = event.call_index;
                call_has_sampled = false;
                call_has_write   = false;
            }

            if (is_read)
            {
                use.is_read = true;

                if (is_replaced == false)
                {
                    use.carry = merge_carry(use.carry,
                                            (has_partial_write) ? CarryKind::POSSIBLE
                                                                : CarryKind::CERTAIN);
                }

                if (event.role == Analysis::APIDumpAnalysis::AccessRole::SAMPLED)
                {
                    call_has_sampled = true;
                }
            }

            if (is_write)
            {
                use.is_written = true;
                call_has_write = true;

                const bool is_full_write             = (event.mode     == Analysis::APIDumpAnalysis::AccessMode::WRITE &&
                                                        event.coverage == Analysis::APIDumpAnalysis::AccessCoverage::FULL);
                bool       is_whole_resource_written = false;

                if (is_full_write)
                {
                    is_whole_resource_written = covers_whole_resource(event.subresource_range,
                                                                      in_description);
                }

                if (is_whole_resource_written)
                {
                    is_replaced          = true;
                    use.is_fully_written = true;
                }
                else
                {
                    has_partial_write = true;
                }
            }

            /* What a call reads as a texture while it also writes is a use of what it is writing. */
            if (call_has_sampled &&
                call_has_write)
            {
                use.is_self_sampled = true;
            }

            ++end_access;
        }

        /* The frame uses the content of the version before it. */
        if (use.carry != CarryKind::NONE)
        {
            if (out_dependencies_ptr->version_vec.empty() )
            {
                /* What the application made it with. */
                out_dependencies_ptr->version_vec.emplace_back();
            }

            ResourceVersion& version = out_dependencies_ptr->version_vec.back();

            if (version.consumer_range_vec.empty()                     == false       &&
                version.consumer_range_vec.back().last_frame_index + 1 == frame_index)
            {
                version.consumer_range_vec.back().last_frame_index = frame_index;
            }
            else
            {
                FrameRange range;

                range.first_frame_index = frame_index;
                range.last_frame_index  = frame_index;

                version.consumer_range_vec.push_back(range);
            }

            version.n_consumer_frames++;

            if (use.carry == CarryKind::POSSIBLE)
            {
                version.n_possible_consumer_frames++;
            }

            summary.longest_carry_n_frames = std::max<uint64_t>(summary.longest_carry_n_frames,
                                                                (version.has_writer) ? frame_index - version.writer_frame_index
                                                                                     : frame_index + 1);
        }

        /* And what it writes is the next version. */
        if (use.is_written)
        {
            ResourceVersion version;

            version.has_writer         = true;
            version.keeps_earlier_content = (use.is_fully_written == false);
            version.writer_frame_index = frame_index;

            out_dependencies_ptr->version_vec.push_back(version);
        }

        if (out_dependencies_ptr->use_run_vec.empty()                     == false       &&
            out_dependencies_ptr->use_run_vec.back().last_frame_index + 1 == frame_index &&
            out_dependencies_ptr->use_run_vec.back().use                  == use)
        {
            out_dependencies_ptr->use_run_vec.back().last_frame_index = frame_index;
        }
        else
        {
            UseRun run;

            run.first_frame_index = frame_index;
            run.last_frame_index  = frame_index;
            run.use               = use;

            out_dependencies_ptr->use_run_vec.push_back(run);
        }

        if (summary.n_frames_used == 0)
        {
            summary.first_frame_index = frame_index;
        }

        summary.last_frame_index = frame_index;

        summary.n_frames_used++;

        if (use.is_read)
        {
            summary.n_frames_read++;
        }

        if (use.is_self_sampled)
        {
            summary.n_frames_self_sampled++;
        }

        if (use.is_written)
        {
            summary.n_frames_written++;
        }

        if (use.carry == CarryKind::CERTAIN)
        {
            summary.n_frames_using_earlier_content++;

            if (use.is_written)
            {
                summary.n_frames_carrying++;
            }
        }
        else
        if (use.carry == CarryKind::POSSIBLE)
        {
            summary.n_frames_using_earlier_content_possibly++;

            if (use.is_written)
            {
                summary.n_frames_carrying_possibly++;
            }
        }

        n_access = end_access;
    }

    if (summary.n_frames_used == 0)
    {
        summary.dependency_class = DependencyClass::UNUSED;
    }
    else
    if (summary.n_frames_carrying != 0)
    {
        summary.dependency_class = DependencyClass::CARRIED;
    }
    else
    if (summary.n_frames_carrying_possibly != 0)
    {
        summary.dependency_class = DependencyClass::CARRIED_POSSIBLY;
    }
    else
    if (summary.n_frames_using_earlier_content          != 0 ||
        summary.n_frames_using_earlier_content_possibly != 0)
    {
        summary.dependency_class = DependencyClass::STATIC;
    }
    else
    {
        summary.dependency_class = DependencyClass::FRAME_LOCAL;
    }
}

bool Analysis::APIDumpDependencies::DependencyAnalyzer::covers_whole_resource(const Analysis::APIDumpAnalysis::SubresourceRange&    in_range,
                                                                              const Analysis::APIDumpAnalysis::ResourceDescription& in_description)
{
    const uint32_t n_layers     = std::max<uint32_t>(in_description.depth_or_layer_count,
                                                     1);
    const uint32_t n_mip_levels = std::max<uint32_t>(in_description.mip_level_count,
                                                     1);

    /* A count of 0 is all of them from the first on. */
    const uint32_t last_layer     = (in_range.n_layers     == 0) ? n_layers
                                                                 : in_range.first_layer     + in_range.n_layers;
    const uint32_t last_mip_level = (in_range.n_mip_levels == 0) ? n_mip_levels
                                                                 : in_range.first_mip_level + in_range.n_mip_levels;

    return (in_range.first_layer     == 0            &&
            in_range.first_mip_level == 0            &&
            last_layer               >= n_layers     &&
            last_mip_level           >= n_mip_levels);
}

Analysis::APIDumpDependencies::CarryKind Analysis::APIDumpDependencies::DependencyAnalyzer::merge_carry(const CarryKind& in_a,
                                                                                                        const CarryKind& in_b)
{
    CarryKind result = CarryKind::NONE;

    if (in_a == CarryKind::CERTAIN ||
        in_b == CarryKind::CERTAIN)
    {
        result = CarryKind::CERTAIN;
    }
    else
    if (in_a == CarryKind::POSSIBLE ||
        in_b == CarryKind::POSSIBLE)
    {
        result = CarryKind::POSSIBLE;
    }

    return result;
}
