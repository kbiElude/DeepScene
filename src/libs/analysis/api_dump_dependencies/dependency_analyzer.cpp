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

    /* Assigns each access to its frame: the accesses from the end count of frame N - 1 up to the end count of frame N belong to frame N. */
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

        /* The accesses after the last frame end belong to a frame that did not end. */
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

    /* Groups the accesses by resource with a counting sort. The sort is stable, so each resource keeps its accesses in the order they were made. */
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

            /* The range holds vector indices, so the value fits in size_t even on a 32-bit build. */
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

    compute_inputs(in_log,
                   in_resource_vec,
                   in_n_threads,
                  &result);

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

    out_dependencies_ptr->written_extent = compute_written_extent(in_log,
                                                                  in_accesses,
                                                                  in_description);

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

        std::vector<Analysis::APIDumpAnalysis::AccessRectangle> written_rectangle_vec; /* rectangles the frame wrote; set only if the written extent is known */

        /* The accesses of the resource in this frame. */
        while (end_access                                                     <  in_accesses.n_events &&
               in_frame_index_vec.at(in_accesses.event_index_ptr[end_access]) == frame_index)
        {
            const Analysis::APIDumpAnalysis::AccessEvent& event    = event_vec.at(in_accesses.event_index_ptr[end_access]);
            const bool                                    is_read  = (event.mode != Analysis::APIDumpAnalysis::AccessMode::WRITE);
            const bool                                    is_write = (event.mode != Analysis::APIDumpAnalysis::AccessMode::READ);

            if (end_access       == n_access   ||
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

                    if (out_dependencies_ptr->written_extent.is_known &&
                        event.rectangle.is_known)
                    {
                        written_rectangle_vec.push_back(event.rectangle);
                    }
                }
            }

            /* A call that samples the resource as a texture and also writes it reads its own output, so the frame uses what it is writing. */
            if (call_has_sampled &&
                call_has_write)
            {
                use.is_self_sampled = true;
            }

            ++end_access;
        }

        /* The frame uses the latest version, which holds the content that earlier frames left. */
        if (use.carry != CarryKind::NONE)
        {
            if (out_dependencies_ptr->version_vec.empty() )
            {
                /* A version without a writer holds the content the application created the resource with. */
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

        /* What the frame writes becomes the next version. */
        if (use.is_written)
        {
            ResourceVersion version;

            version.has_writer            = true;
            version.keeps_earlier_content = (use.is_fully_written == false);
            version.writer_frame_index    = frame_index;

            /* A write that replaced the whole resource has no useful rectangle, so none is stored. */
            if (use.is_fully_written == false)
            {
                version.written_rectangle_vec = written_rectangle_vec;
            }

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

void Analysis::APIDumpDependencies::DependencyAnalyzer::compute_frame_inputs(const std::vector<Analysis::APIDumpAnalysis::AccessEvent>& in_event_vec,
                                                                             const size_t&                                              in_first_event,
                                                                             const size_t&                                              in_end_event,
                                                                             const std::vector<Analysis::APIDumpAnalysis::Resource>&    in_resource_vec,
                                                                             const std::vector<uint8_t>&                                in_is_tracked_vec,
                                                                             InputWorkspace*                                            inout_workspace_ptr,
                                                                             std::vector<FrameInputs>*                                  out_frame_inputs_vec_ptr)
{
    InputWorkspace&       workspace = *inout_workspace_ptr;
    std::vector<uint32_t> input_vec;
    size_t                n_event   = in_first_event;

    const auto add_sorted_func = [](std::vector<uint32_t>* inout_vec_ptr,
                                    const uint32_t&        in_number)
    {
        const auto position = std::lower_bound(inout_vec_ptr->begin(),
                                               inout_vec_ptr->end  (),
                                               in_number);

        if (position == inout_vec_ptr->end() ||
            *position != in_number)
        {
            inout_vec_ptr->insert(position,
                                  in_number);
        }
    };

    while (n_event < in_end_event)
    {
        const uint64_t call_index = in_event_vec.at(n_event).call_index;
        size_t         end_call   = n_event;

        while (end_call                             <  in_end_event &&
               in_event_vec.at(end_call).call_index == call_index)
        {
            ++end_call;
        }

        input_vec.clear();

        /* The call gives what it reads to what it writes. For each resource the call reads, the inputs are the resources that already flowed into it
         * in this frame, or the resource itself if nothing has been written to it yet.
         */
        for (size_t n_call_event = n_event;
                    n_call_event < end_call;
                  ++n_call_event)
        {
            const auto&    event  = in_event_vec.at(n_call_event);
            const uint32_t number = Analysis::APIDumpAnalysis::get_resource_id_value(event.resource_id);

            if (event.mode != Analysis::APIDumpAnalysis::AccessMode::WRITE &&
                number     >= 1                                            &&
                number     <= in_resource_vec.size() )
            {
                if (workspace.has_dependencies_vec.at(number) )
                {
                    for (const auto dependency : workspace.dependency_vec_vec.at(number) )
                    {
                        add_sorted_func(&input_vec,
                                        dependency);
                    }
                }
                else
                {
                    add_sorted_func(&input_vec,
                                    number);
                }
            }
        }

        for (size_t n_call_event = n_event;
                    n_call_event < end_call;
                  ++n_call_event)
        {
            const auto&    event  = in_event_vec.at(n_call_event);
            const uint32_t number = Analysis::APIDumpAnalysis::get_resource_id_value(event.resource_id);

            if (event.mode != Analysis::APIDumpAnalysis::AccessMode::READ &&
                number     >= 1                                           &&
                number     <= in_resource_vec.size() )
            {
                const bool is_whole_resource = covers_whole_resource(event.subresource_range,
                                                                     in_resource_vec.at(number - 1).description);
                const bool is_replace        = (event.mode     == Analysis::APIDumpAnalysis::AccessMode::WRITE    &&
                                                event.coverage == Analysis::APIDumpAnalysis::AccessCoverage::FULL &&
                                                is_whole_resource);
                const bool is_fed_back       = std::binary_search             (input_vec.begin(),
                                                                               input_vec.end  (),
                                                                               number);
                auto&      dependency_vec    = workspace.dependency_vec_vec.at(number);

                if (is_fed_back)
                {
                    workspace.has_feedback_vec.at(number) = 1;
                }

                /* At the first write in the frame, the resource still holds its content from before the frame. A write that replaces only part of
                 * it keeps that content, so the resource then depends on itself.
                 */
                if (workspace.has_dependencies_vec.at(number) == 0)
                {
                    workspace.has_dependencies_vec.at(number) = 1;

                    workspace.touched_vec.push_back(number);

                    dependency_vec.clear();

                    if (is_replace == false)
                    {
                        dependency_vec.push_back(number);
                    }
                }
                else
                if (is_replace)
                {
                    dependency_vec.clear();
                }

                for (const auto input : input_vec)
                {
                    add_sorted_func(&dependency_vec,
                                    input);
                }

                workspace.is_written_vec.at(number) = 1;
            }
        }

        n_event = end_call;
    }

    for (const uint32_t number : workspace.touched_vec)
    {
        auto& dependency_vec = workspace.dependency_vec_vec.at(number);

        if (workspace.is_written_vec.at(number) != 0 &&
            in_is_tracked_vec.at       (number) != 0)
        {
            FrameInputs frame_inputs;

            frame_inputs.has_feedback    = (workspace.has_feedback_vec.at(number) != 0);
            frame_inputs.resource_number = number;

            for (const auto dependency : dependency_vec)
            {
                if (dependency != number)
                {
                    frame_inputs.input_resource_vec.push_back(dependency);
                }
            }

            out_frame_inputs_vec_ptr->push_back(frame_inputs);
        }

        dependency_vec.clear();

        workspace.has_dependencies_vec.at(number) = 0;
        workspace.has_feedback_vec.at    (number) = 0;
        workspace.is_written_vec.at      (number) = 0;
    }

    workspace.touched_vec.clear();
}

void Analysis::APIDumpDependencies::DependencyAnalyzer::compute_inputs(const AccessLog&                                        in_log,
                                                                       const std::vector<Analysis::APIDumpAnalysis::Resource>& in_resource_vec,
                                                                       const uint32_t&                                         in_n_threads,
                                                                       DependencyResult*                                       inout_result_ptr)
{
    const auto&          event_vec           = in_log.get_event_vec                ();
    const auto&          frame_end_count_vec = in_log.get_frame_end_event_count_vec();
    bool                 has_tracked         = false;
    std::vector<uint8_t> is_tracked_vec(in_resource_vec.size() + 1,
                                        0);

    /* Only resources with a known written extent are tracked. For the others, the writer is assumed to need everything the frame uses. */
    for (size_t n_resource = 0;
                n_resource < in_resource_vec.size();
              ++n_resource)
    {
        if (inout_result_ptr->resource_vec.at(n_resource).written_extent.is_known)
        {
            is_tracked_vec.at(n_resource + 1) = 1;

            has_tracked = true;
        }
    }

    if (has_tracked)
    {
        const size_t                           n_frames = static_cast<size_t>(inout_result_ptr->n_frames);
        std::vector<std::vector<FrameInputs> > frame_inputs_vec_vec(n_frames);

        const auto compute_frames_func = [&](const uint64_t& in_first_frame,
                                             const uint64_t& in_end_frame)
        {
            InputWorkspace workspace;

            workspace.dependency_vec_vec.resize  (in_resource_vec.size() + 1);
            workspace.has_dependencies_vec.assign(in_resource_vec.size() + 1,
                                                  0);
            workspace.has_feedback_vec.assign    (in_resource_vec.size() + 1,
                                                  0);
            workspace.is_written_vec.assign      (in_resource_vec.size() + 1,
                                                  0);

            for (uint64_t n_frame  = in_first_frame;
                          n_frame  < in_end_frame;
                        ++n_frame)
            {
                const size_t frame_index = static_cast<size_t>(n_frame);
                const size_t first_event = (frame_index == 0)                           ? 0
                                                                                        : static_cast<size_t>(frame_end_count_vec.at(frame_index - 1) );
                const size_t end_event   = (frame_index <  frame_end_count_vec.size() ) ? static_cast<size_t>(frame_end_count_vec.at(frame_index) )
                                                                                        : event_vec.size();

                compute_frame_inputs(event_vec,
                                     first_event,
                                     end_event,
                                     in_resource_vec,
                                     is_tracked_vec,
                                    &workspace,
                                    &frame_inputs_vec_vec.at(frame_index) );
            }
        };

        Utils::Parallel::ParallelFor::run(n_frames,
                                          in_n_threads,
                                          compute_frames_func);

        /* Finds the version that the frame wrote: the version whose writer is this frame. The versions are ordered by writer frame, so a binary
         * search finds it.
         */
        for (size_t n_frame = 0;
                    n_frame < n_frames;
                  ++n_frame)
        {
            for (const auto& frame_inputs : frame_inputs_vec_vec.at(n_frame) )
            {
                auto&  version_vec = inout_result_ptr->resource_vec.at(frame_inputs.resource_number - 1).version_vec;
                size_t first       = (version_vec.empty() == false && version_vec.front().has_writer == false) ? 1
                                                                                                               : 0;
                size_t end         = version_vec.size();

                while (first < end)
                {
                    const size_t middle = first + (end - first) / 2;

                    if (version_vec.at(middle).writer_frame_index < n_frame)
                    {
                        first = middle + 1;
                    }
                    else
                    {
                        end = middle;
                    }
                }

                if (first < version_vec.size() )
                {
                    ResourceVersion& version = version_vec.at(first);

                    if (version.has_writer                   &&
                        version.writer_frame_index == n_frame)
                    {
                        version.has_inputs = (frame_inputs.has_feedback == false);

                        if (version.has_inputs)
                        {
                            version.input_resource_vec = frame_inputs.input_resource_vec;
                        }
                    }
                }
            }
        }
    }
}

Analysis::APIDumpAnalysis::AccessRectangle Analysis::APIDumpDependencies::DependencyAnalyzer::compute_written_extent(const AccessLog&                                      in_log,
                                                                                                                     const ResourceAccesses&                               in_accesses,
                                                                                                                     const Analysis::APIDumpAnalysis::ResourceDescription& in_description)
{
    const auto&                                event_vec     = in_log.get_event_vec();
    bool                                       has_rectangle = false;
    bool                                       is_bounded    = true;
    uint64_t                                   max_x         = 0;
    uint64_t                                   max_y         = 0;
    uint32_t                                   min_x         = 0;
    uint32_t                                   min_y         = 0;
    Analysis::APIDumpAnalysis::AccessRectangle result;

    for (size_t n_access = 0;
                n_access < in_accesses.n_events && is_bounded;
              ++n_access)
    {
        const Analysis::APIDumpAnalysis::AccessEvent& event = event_vec.at(in_accesses.event_index_ptr[n_access]);

        if (event.mode != Analysis::APIDumpAnalysis::AccessMode::READ)
        {
            const bool is_whole_resource = covers_whole_resource(event.subresource_range,
                                                                 in_description);
            const bool is_first_level    = is_first_level_range (event.subresource_range);
            const bool is_replace        = (event.mode     == Analysis::APIDumpAnalysis::AccessMode::WRITE &&
                                            event.coverage == Analysis::APIDumpAnalysis::AccessCoverage::FULL);

            /* A write that replaces the whole resource leaves none of the earlier content, so it does not contribute to the written extent. */
            if (is_replace        &&
                is_whole_resource)
            {
                continue;
            }

            if (event.rectangle.is_known == false ||
                is_first_level           == false)
            {
                is_bounded = false;

                continue;
            }

            /* A rectangle with no area reaches nothing, so it is ignored. */
            if (event.rectangle.width  != 0 &&
                event.rectangle.height != 0)
            {
                const uint64_t right = static_cast<uint64_t>(event.rectangle.x) + event.rectangle.width;
                const uint64_t top   = static_cast<uint64_t>(event.rectangle.y) + event.rectangle.height;

                if (has_rectangle == false)
                {
                    min_x         = event.rectangle.x;
                    min_y         = event.rectangle.y;
                    max_x         = right;
                    max_y         = top;
                    has_rectangle = true;
                }
                else
                {
                    min_x = std::min<uint32_t>(min_x,
                                               event.rectangle.x);
                    min_y = std::min<uint32_t>(min_y,
                                               event.rectangle.y);
                    max_x = std::max<uint64_t>(max_x,
                                               right);
                    max_y = std::max<uint64_t>(max_y,
                                               top);
                }
            }
        }
    }

    if (is_bounded &&
        has_rectangle)
    {
        result.height   = static_cast<uint32_t>(max_y - min_y);
        result.is_known = true;
        result.width    = static_cast<uint32_t>(max_x - min_x);
        result.x        = min_x;
        result.y        = min_y;
    }

    return result;
}

bool Analysis::APIDumpDependencies::DependencyAnalyzer::covers_whole_resource(const Analysis::APIDumpAnalysis::SubresourceRange&    in_range,
                                                                              const Analysis::APIDumpAnalysis::ResourceDescription& in_description)
{
    const uint32_t n_layers     = std::max<uint32_t>(in_description.depth_or_layer_count,
                                                     1);
    const uint32_t n_mip_levels = std::max<uint32_t>(in_description.mip_level_count,
                                                     1);

    /* A count of 0 means all layers or mip levels from the first one on. */
    const uint32_t last_layer     = (in_range.n_layers     == 0) ? n_layers
                                                                 : in_range.first_layer     + in_range.n_layers;
    const uint32_t last_mip_level = (in_range.n_mip_levels == 0) ? n_mip_levels
                                                                 : in_range.first_mip_level + in_range.n_mip_levels;

    return (in_range.first_layer     == 0            &&
            in_range.first_mip_level == 0            &&
            last_layer               >= n_layers     &&
            last_mip_level           >= n_mip_levels);
}

bool Analysis::APIDumpDependencies::DependencyAnalyzer::is_first_level_range(const Analysis::APIDumpAnalysis::SubresourceRange& in_range)
{
    /* A layer count of 0 means all layers. For a one-layer resource, a count of 0 and a count of 1 mean the same, so both are accepted. */
    return (in_range.first_layer     == 0 &&
            in_range.first_mip_level == 0 &&
            in_range.n_layers        <= 1 &&
            in_range.n_mip_levels    == 1);
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
