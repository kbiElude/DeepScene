/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include "analysis/api_dump_dependencies/rectangle_coverage.h"
#include <algorithm>
#include <deque>
#include <numeric>
#include <utility>

bool Analysis::APIDumpDependencies::FrameSlicePlanner::is_live(const FrameSlice& in_slice,
                                                               const uint64_t&   in_frame_index)
{
    return (in_frame_index >= in_slice.is_live_vec.size() ||
            in_slice.is_live_vec.at(static_cast<size_t>(in_frame_index) ) );
}

Analysis::APIDumpDependencies::FrameSlice Analysis::APIDumpDependencies::FrameSlicePlanner::plan(const DependencyFileContent& in_analysis,
                                                                                                 const uint64_t&              in_target_frame_index)
{
    /* A resource index and the index of one of its versions. */
    typedef std::pair<size_t, size_t> VersionRef;

    /* How much of a frame has to be made: none of it (dead), only what the needed versions that the frame wrote depend on (partly live), or all
     * of it (fully live). A replay makes a frame as a whole or leaves it out, so a partly live frame is still replayed.
     */
    enum FrameState
    {
        FRAME_DEAD,
        FRAME_PARTLY_LIVE,
        FRAME_FULLY_LIVE
    };

    /* A pending piece of work. Either the frame is fully live and everything it uses must be made (is_whole_frame), or only one version that
     * the frame wrote is needed and only the inputs of that version must be made.
     */
    struct Task
    {
        uint64_t frame_index;
        bool     is_whole_frame;
        size_t   resource_index;
        size_t   version_index;
    };

    /* The result of compute_windows() for one resource. It is computed the first time it is needed. */
    struct Windows
    {
        std::vector<size_t> end_version_vec;
        std::vector<size_t> first_version_vec;
        bool                is_computed = false;
    };

    const auto&                        resource_vec = in_analysis.result.resource_vec;
    const uint64_t                     n_frames     = in_analysis.result.n_frames;
    FrameSlice                         result;
    std::deque<Task>                   task_queue;

    std::vector<std::vector<VersionRef> > consumed_vec_vec(static_cast<size_t>(n_frames) ); /* by frame: the versions it uses, sorted by resource           */
    std::vector<std::vector<size_t> >     parent_vec_vec  (resource_vec.size  ());          /* by resource: union-find over version positions, see below    */
    std::vector<FrameState>               state_vec       (static_cast<size_t>(n_frames),
                                                           FRAME_DEAD);
    std::vector<Windows>                  windows_vec     (resource_vec.size  ());

    result.is_live_vec.assign(static_cast<size_t>(n_frames),
                              false);

    result.target_frame_index = in_target_frame_index;

    /* Records the versions that each frame consumes. The version a frame consumes is the one that was the latest when the frame began. */
    for (size_t n_resource = 0;
                n_resource < resource_vec.size();
              ++n_resource)
    {
        const auto& version_vec = resource_vec.at(n_resource).version_vec;
        auto&       parent_vec  = parent_vec_vec.at(n_resource);

        /* Positions are version indices plus 1, so position 0 means that there is no version. Initially each position is its own parent, which
         * marks its version as not needed yet. When a version becomes needed, its position is pointed at the position below it.
         */
        parent_vec.resize(version_vec.size() + 1);

        std::iota(parent_vec.begin(),
                  parent_vec.end  (),
                  0);

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

    /* Makes a frame fully live and queues it, so that everything it uses is made as well. The writers of those versions are made in full too,
     * unless a version records its inputs (see make_part_of_frame_func).
     */
    const auto make_whole_frame_func = [&](const uint64_t& in_frame_index)
    {
        if (in_frame_index < n_frames)
        {
            const size_t frame_index = static_cast<size_t>(in_frame_index);

            if (state_vec.at(frame_index) != FRAME_FULLY_LIVE)
            {
                if (state_vec.at(frame_index) == FRAME_DEAD)
                {
                    result.is_live_vec.at(frame_index) = true;
                    result.n_live_frames++;
                }

                state_vec.at(frame_index) = FRAME_FULLY_LIVE;

                Task task;

                task.frame_index    = in_frame_index;
                task.is_whole_frame = true;
                task.resource_index = 0;
                task.version_index  = 0;

                task_queue.push_back(task);
            }
        }
    };

    /* Makes a frame partly live. The frame is replayed as a whole, but only what its needed version depends on has to be made for it. */
    const auto make_part_of_frame_func = [&](const uint64_t& in_frame_index)
    {
        if (in_frame_index < n_frames)
        {
            const size_t frame_index = static_cast<size_t>(in_frame_index);

            if (state_vec.at(frame_index) == FRAME_DEAD)
            {
                state_vec.at(frame_index)          = FRAME_PARTLY_LIVE;
                result.is_live_vec.at(frame_index) = true;

                result.n_live_frames++;
            }
        }
    };

    /* Returns the position (version index plus 1) of the nearest version at or below in_position that is not needed yet, or 0 if there is none.
     * It compresses the paths it follows.
     */
    const auto find_unneeded_func = [](std::vector<size_t>* inout_parent_vec_ptr,
                                       const size_t&        in_position)
    {
        size_t root     = in_position;
        size_t position = in_position;

        while (inout_parent_vec_ptr->at(root) != root)
        {
            root = inout_parent_vec_ptr->at(root);
        }

        while (inout_parent_vec_ptr->at(position) != root)
        {
            const size_t next_position = inout_parent_vec_ptr->at(position);

            inout_parent_vec_ptr->at(position) = root;
            position                           = next_position;
        }

        return root;
    };

    /* Marks the versions from in_first_version to in_last_version (inclusive) of a resource as needed. Each version is processed once. Its
     * writer is made in full, unless the version records its inputs (has_inputs). Then only those inputs are made for the writer.
     */
    const auto need_versions_func = [&](const size_t& in_resource,
                                        const size_t& in_first_version,
                                        const size_t& in_last_version)
    {
        auto&       parent_vec  = parent_vec_vec.at(in_resource);
        const auto& version_vec = resource_vec.at(in_resource).version_vec;
        size_t      position    = find_unneeded_func(&parent_vec,
                                                     in_last_version + 1);

        while (position > in_first_version)
        {
            const auto& version = version_vec.at(position - 1);

            if (version.has_writer)
            {
                if (version.has_inputs)
                {
                    Task task;

                    make_part_of_frame_func(version.writer_frame_index);

                    task.frame_index    = version.writer_frame_index;
                    task.is_whole_frame = false;
                    task.resource_index = in_resource;
                    task.version_index  = position - 1;

                    task_queue.push_back(task);
                }
                else
                {
                    make_whole_frame_func(version.writer_frame_index);
                }
            }

            parent_vec.at(position) = position - 1;

            position = find_unneeded_func(&parent_vec,
                                          position - 1);
        }
    };

    /* A live frame consumes a version of a resource. This marks that version, and the earlier versions it depends on, as needed, as far back
     * as the walk goes (see compute_windows()).
     */
    const auto need_consumed_func = [&](const size_t& in_resource,
                                        const size_t& in_version)
    {
        auto& windows = windows_vec.at(in_resource);

        if (windows.is_computed == false)
        {
            compute_windows(resource_vec.at(in_resource),
                           &windows.end_version_vec,
                           &windows.first_version_vec);

            windows.is_computed = true;
        }

        const size_t end_version   = windows.end_version_vec.at  (in_version);
        const size_t first_version = windows.first_version_vec.at(in_version);

        /* The consumed version is needed, and so are the older versions down to first_version. A writer that replaced only part of the resource
         * leaves part of the version before it visible.
         */
        need_versions_func(in_resource,
                           first_version,
                           in_version);

        /* If the walk skipped versions, the version where it ends is still needed. It holds the content that the skipped writes did not reach. */
        if (first_version > end_version)
        {
            need_versions_func(in_resource,
                               end_version,
                               end_version);
        }
    };

    /* The target frame and every frame after it are live, and so is everything they use. */
    for (uint64_t n_frame = in_target_frame_index;
                  n_frame < n_frames;
                ++n_frame)
    {
        make_whole_frame_func(n_frame);
    }

    while (task_queue.empty() == false)
    {
        const Task task = task_queue.front();

        task_queue.pop_front();

        const auto& consumed_vec = consumed_vec_vec.at(static_cast<size_t>(task.frame_index) );

        if (task.is_whole_frame)
        {
            for (const auto& consumed : consumed_vec)
            {
                need_consumed_func(consumed.first,
                                   consumed.second);
            }
        }
        else
        {
            /* A frame that is needed only for a version it wrote needs only the inputs of that version, not everything the frame uses. The earlier
             * content of the written resource itself is covered by the walk back from that version (see need_versions_func).
             */
            const auto& version = resource_vec.at(task.resource_index).version_vec.at(task.version_index);

            for (const uint32_t input_number : version.input_resource_vec)
            {
                const size_t input_resource = static_cast<size_t>(input_number) - 1;
                const auto   consumed_begin = std::lower_bound(consumed_vec.begin(),
                                                               consumed_vec.end  (),
                                                               VersionRef(input_resource,
                                                                          0) );

                for (auto consumed_iterator  = consumed_begin;
                          consumed_iterator != consumed_vec.end() && consumed_iterator->first == input_resource;
                        ++consumed_iterator)
                {
                    need_consumed_func(consumed_iterator->first,
                                       consumed_iterator->second);
                }
            }
        }
    }

    return result;
}

void Analysis::APIDumpDependencies::FrameSlicePlanner::compute_windows(const ResourceDependencies& in_dependencies,
                                                                       std::vector<size_t>*        out_end_version_vec_ptr,
                                                                       std::vector<size_t>*        out_first_version_vec_ptr)
{
    /* The maximum number of versions that are examined together. A texture that needs more versions than this to become complete is treated as
     * one that never becomes complete.
     */
    constexpr size_t MAX_N_VERSIONS_IN_WINDOW = 64;

    const auto&  version_vec = in_dependencies.version_vec;
    const size_t n_versions  = version_vec.size();
    size_t       first       = 0;

    out_end_version_vec_ptr->assign  (n_versions,
                                      0);
    out_first_version_vec_ptr->assign(n_versions,
                                      0);

    for (size_t n_version = 0;
                n_version < n_versions;
              ++n_version)
    {
        const auto& version = version_vec.at(n_version);

        /* The walk back continues while the writer left some of the earlier content in place. It ends at a version without a writer, a version
         * whose writer replaced everything, or the first version.
         */
        const bool ends_here = (version.has_writer            == false ||
                                version.keeps_earlier_content == false ||
                                n_version                     == 0);
        const size_t end_version = (ends_here) ? n_version
                                               : out_end_version_vec_ptr->at(n_version - 1);

        out_end_version_vec_ptr->at(n_version)   = end_version;
        out_first_version_vec_ptr->at(n_version) = end_version;

        if (ends_here)
        {
            first = end_version + 1;
        }
        else
        if (in_dependencies.written_extent.is_known)
        {
            /* Looks for the shortest run of versions that ends with this version and has written the whole extent. The start of that run never
             * moves back as n_version grows, so the search resumes from the start found so far.
             */
            const size_t lowest_first = std::max<size_t>(first,
                                                         (n_version + 1 > MAX_N_VERSIONS_IN_WINDOW) ? n_version + 1 - MAX_N_VERSIONS_IN_WINDOW
                                                                                                    : 0);

            /* A run can hide the older versions only if every version in it records its inputs. A writer that is given everything it uses needs the
             * version before it, whichever part of that version the writer reads.
             */
            const auto covers_func = [&](const size_t& in_first_version)
            {
                std::vector<Analysis::APIDumpAnalysis::AccessRectangle> rectangle_vec;
                bool                                                    is_each_known = true;

                for (size_t n_in_run = in_first_version;
                            n_in_run <= n_version;
                          ++n_in_run)
                {
                    const auto& run_version = version_vec.at(n_in_run);

                    if (run_version.has_inputs == false)
                    {
                        is_each_known = false;
                    }

                    rectangle_vec.insert(rectangle_vec.end(),
                                         run_version.written_rectangle_vec.begin(),
                                         run_version.written_rectangle_vec.end  () );
                }

                const bool is_covering = RectangleCoverage::covers(rectangle_vec,
                                                                   in_dependencies.written_extent);

                return (is_each_known &&
                        is_covering);
            };

            if (lowest_first > end_version &&
                lowest_first <= n_version)
            {
                const bool is_covered = covers_func(lowest_first);

                if (is_covered)
                {
                    first = lowest_first;

                    while (first < n_version)
                    {
                        const bool is_still_covered = covers_func(first + 1);

                        if (is_still_covered == false)
                        {
                            break;
                        }

                        ++first;
                    }

                    out_first_version_vec_ptr->at(n_version) = first;
                }
            }
        }
    }
}
