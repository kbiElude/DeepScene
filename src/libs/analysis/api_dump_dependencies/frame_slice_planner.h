/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Decides which frames a replay must make to jump to a target frame: those whose content the target or any later frame reads.
 *
 * From the target and every later frame, for each resource version they use, it walks back through earlier versions. The writer of a version
 * is live, and so is the writer of the version before it if the writer did not replace all content (see CarryKind and ResourceVersion), and so
 * on. A frame that replaces all of a resource ends the walk.
 *
 * A run of frames that together wrote the whole written extent (ResourceDependencies::written_extent) also ends the walk. The part outside
 * the extent holds what the last full replacement (or the application) left, so that frame is still needed.
 *
 * Every frame after the target is live, so a replay continuing past it matches a full replay.
 */
#ifndef APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H
#define APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H

#include "analysis/api_dump_dependencies/types.h"

namespace Analysis
{
    namespace APIDumpDependencies
    {
        class FrameSlicePlanner
        {
        public:
            /* True if a replay has to make the frame. Frames past the end of the slice are live. */
            static bool is_live(const FrameSlice& in_slice,
                                const uint64_t&   in_frame_index);

            static FrameSlice plan(const DependencyFileContent& in_analysis,
                                   const uint64_t&              in_target_frame_index);

        private:
            /* Computes, per version, how far back a consumer has to look.
             * out_end_version_vec_ptr: where the walk ends, the nearest earlier version whose writer replaced all content, else the first.
             * out_first_version_vec_ptr: the oldest version to make. It equals the end version unless later versions already wrote the whole
             * written extent. Versions from it up to the consumed one are needed, and so is the end version (it holds content outside the extent).
             */
            static void compute_windows(const ResourceDependencies& in_dependencies,
                                        std::vector<size_t>*        out_end_version_vec_ptr,
                                        std::vector<size_t>*        out_first_version_vec_ptr);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H */
