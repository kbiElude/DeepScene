/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Decides which frames of a workload must be made so that a replay can jump to a target frame without making every frame before it. These are the
 * frames whose content the target frame, or any later frame, reads.
 *
 * The planner starts from the target and every frame after it. For each version of a resource that these frames use, it walks back through the
 * earlier versions. The frame that wrote a version is live. So is the frame that wrote the version before it, if the writer did not replace all of
 * the content (see CarryKind and ResourceVersion), and so on. A frame that replaces all of a resource ends the walk for that resource.
 *
 * A run of frames that together wrote the whole written extent of the resource (ResourceDependencies::written_extent) also ends the walk. For
 * example, a texture that frames fill one strip at a time is complete again after the strips of a few frames, and the writes before those no
 * longer show in it. The part of the resource outside the written extent holds what the last frame that replaced all of it (or the application)
 * left, so that frame is still needed.
 *
 * Every frame after the target is live, so that a replay that continues past the target comes out as it does when everything is replayed.
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
            /* True if a replay has to make the frame. Frames beyond the end of the slice, which the analysis has not seen, are reported live. */
            static bool is_live(const FrameSlice& in_slice,
                                const uint64_t&   in_frame_index);

            static FrameSlice plan(const DependencyFileContent& in_analysis,
                                   const uint64_t&              in_target_frame_index);

        private:
            /* Computes, for each version of a resource, how far back a consumer of that version has to look.
             *
             * out_end_version_vec_ptr receives the version where the walk back ends: the nearest earlier version whose writer replaced all of the
             * resource, or the first version if there is none.
             *
             * out_first_version_vec_ptr receives the oldest version that has to be made. It equals the end version, unless the later versions have
             * already written the whole written extent, which hides the older ones. The versions from that oldest one up to the consumed version
             * are needed, and so is the end version, because it holds the content that lies outside the written extent.
             */
            static void compute_windows(const ResourceDependencies& in_dependencies,
                                        std::vector<size_t>*        out_end_version_vec_ptr,
                                        std::vector<size_t>*        out_first_version_vec_ptr);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H */
