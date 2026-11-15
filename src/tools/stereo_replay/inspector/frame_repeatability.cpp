/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_repeatability.h"
#include "analysis/api_dump_dependencies/rectangle_coverage.h"
#include <algorithm>

bool Stereo::Replay::Inspector::FrameRepeatability::is_repeatable(const Analysis::APIDumpDependencies::DependencyAnalysis& in_analysis,
                                                                  const uint32_t&                                          in_frame_index,
                                                                  std::string*                                             out_reason_ptr)
{
    const auto& call_range_vec = in_analysis.frame_call_range_vec;
    const auto& resource_vec   = in_analysis.result.resource_vec;
    bool        result         = true;

    /* Nothing is known about a frame whose end the analysis has not seen. */
    if (in_frame_index >= call_range_vec.size() )
    {
        *out_reason_ptr = "the frame is not one that the dependencies know";

        result = false;
    }

    for (size_t n_resource = 0;
                n_resource < resource_vec.size() && result;
              ++n_resource)
    {
        const auto&        dependencies = resource_vec.at(n_resource);
        const std::string  name         = "resource " + std::to_string(n_resource + 1);

        /* A frame that creates or releases a resource would create or release it again every time it is played. */
        if (n_resource < in_analysis.resource_vec.size() )
        {
            const auto& resource = in_analysis.resource_vec.at(n_resource);
            const auto& range    = call_range_vec.at(in_frame_index);

            const bool is_created   = (resource.created_at_call_index >= range.first_call_index &&
                                       resource.created_at_call_index <= range.last_call_index);
            const bool is_destroyed = (resource.is_destroyed                                     &&
                                       resource.destroyed_at_call_index >= range.first_call_index &&
                                       resource.destroyed_at_call_index <= range.last_call_index);

            if (is_created ||
                is_destroyed)
            {
                *out_reason_ptr = "the frame makes or releases " + name;

                result = false;

                break;
            }
        }

        /* Find the use run (a stretch of consecutive frames that use the resource) that contains this frame. A frame outside a run does not use the
         * resource. */
        const auto run_end_iterator = std::upper_bound(dependencies.use_run_vec.begin(),
                                                       dependencies.use_run_vec.end  (),
                                                       static_cast<uint64_t>(in_frame_index),
                                                       [](const uint64_t&                          in_frame,
                                                          const Analysis::APIDumpDependencies::UseRun& in_run)
                                                       {
                                                           return (in_frame < in_run.first_frame_index);
                                                       });

        if (run_end_iterator == dependencies.use_run_vec.begin() )
        {
            continue;
        }

        const auto& run = *(run_end_iterator - 1);

        if (in_frame_index > run.last_frame_index)
        {
            continue;
        }

        /* A frame that writes the resource and uses what it held before gives a different result when it is played again, unless the frame
         * overwrites the whole written extent first. */
        if (run.use.is_written &&
            run.use.carry      != Analysis::APIDumpDependencies::CarryKind::NONE)
        {
            bool is_filled = false;

            if (dependencies.written_extent.is_known)
            {
                for (const auto& version : dependencies.version_vec)
                {
                    if (version.has_writer                          &&
                        version.writer_frame_index == in_frame_index &&
                        version.has_inputs)
                    {
                        is_filled = Analysis::APIDumpDependencies::RectangleCoverage::covers(version.written_rectangle_vec,
                                                                                            dependencies.written_extent);
                    }
                }
            }

            if (is_filled == false)
            {
                *out_reason_ptr = "the frame writes " + name + " and uses what was in it";

                result = false;
            }
        }
    }

    return result;
}
