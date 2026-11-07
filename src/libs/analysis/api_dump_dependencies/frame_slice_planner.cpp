/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include <deque>
#include <utility>

bool Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(const FrameSlice& in_slice,
                                                               const uint64_t&   in_frame_index)
{
    return (in_frame_index >= in_slice.is_live_vec.size() ||
            in_slice.is_live_vec.at(static_cast<size_t>(in_frame_index) ) );
}

Analysis::APIDumpDependencies::FrameSlice Analysis::APIDumpDependencies::FrameSlicePlanner::plan(const DependencyAnalysis& in_analysis,
                                                                                                 const uint64_t&           in_target_frame_index)
{
    /* A resource, and the number of one of its versions. */
    typedef std::pair<size_t, size_t> VersionRef;

    const auto&                        resource_vec = in_analysis.result.resource_vec;
    const uint64_t                     n_frames     = in_analysis.result.n_frames;
    std::deque<uint64_t>               frame_queue;
    FrameSlice                         result;

    std::vector<std::vector<VersionRef> > consumed_vec_vec (static_cast<size_t>(n_frames) ); /* by frame: what it uses of the frames before it */
    std::vector<std::vector<bool> >       is_needed_vec_vec(resource_vec.size  ());

    result.is_live_vec.assign(static_cast<size_t>(n_frames),
                              false);

    result.target_frame_index = in_target_frame_index;

    /* What each frame uses of earlier content: the version a frame consumes is the one which was the latest when the frame began. */
    for (size_t n_resource = 0;
                n_resource < resource_vec.size();
              ++n_resource)
    {
        const auto& version_vec = resource_vec.at(n_resource).version_vec;

        is_needed_vec_vec.at(n_resource).assign(version_vec.size(),
                                                false);

        for (size_t n_version = 0;
                    n_version < version_vec.size();
                  ++n_version)
        {
            for (const auto& range : version_vec.at(n_version).consumer_range_vec)
            {
                for (uint64_t n_frame  = range.first_frame_index;
                              n_frame <= range.last_frame_index && n_frame < n_frames;
                            ++n_frame)
                {
                    consumed_vec_vec.at(static_cast<size_t>(n_frame) ).push_back(VersionRef(n_resource,
                                                                                            n_version) );
                }
            }
        }
    }

    const auto make_live_func = [&](const uint64_t& in_frame_index)
    {
        if (in_frame_index                                              <  n_frames &&
            result.is_live_vec.at(static_cast<size_t>(in_frame_index) ) == false)
        {
            result.is_live_vec.at(static_cast<size_t>(in_frame_index) ) = true;
            result.n_live_frames++;

            frame_queue.push_back(in_frame_index);
        }
    };

    /* The target and everything after it is made, and so is whatever it uses. */
    for (uint64_t n_frame = in_target_frame_index;
                  n_frame < n_frames;
                ++n_frame)
    {
        make_live_func(n_frame);
    }

    while (frame_queue.empty() == false)
    {
        const uint64_t frame_index = frame_queue.front();

        frame_queue.pop_front();

        for (const auto& consumed : consumed_vec_vec.at(static_cast<size_t>(frame_index) ) )
        {
            size_t n_version = consumed.second;

            /* The writer of the version is live; if it left some of the version before it, that is used too. */
            while (is_needed_vec_vec.at(consumed.first).at(n_version) == false)
            {
                const auto& version = resource_vec.at(consumed.first).version_vec.at(n_version);

                is_needed_vec_vec.at(consumed.first).at(n_version) = true;

                if (version.has_writer)
                {
                    make_live_func(version.writer_frame_index);
                }

                if (version.has_writer             == false ||
                    version.keeps_earlier_content  == false ||
                    n_version                      == 0)
                {
                    break;
                }

                --n_version;
            }
        }
    }

    return result;
}
