/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says which frames of a workload have to be made, for a replay that goes to a frame without making all the frames before it: the ones whose content
 * the target frame or any frame after it reads. It starts from the target and every frame after it, and goes back through the versions of the resources
 * which they use: the frame which wrote a version is live, and so is the one which wrote the version before it if the writer did not replace all of the
 * content (CarryKind and ResourceVersion say), and so on. A frame which replaces all of a resource ends the way back for that resource.
 *
 * So does a run of frames which together wrote all of the part of the resource that any write reaches (ResourceDependencies::written_extent): a texture
 * that the frames fill a strip at a time is whole again after the strips of a few of them, and the writes before those do not show in it. What is outside
 * the part that the writes reach is what the frame that last replaced all of the resource (or the application) left, so that frame is still needed.
 *
 * The frames after the target are all live, so that a replay which goes on after it comes out as it does when everything is replayed.
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
            /* True if a replay has to make the content of the frame; also for a frame which the analysis has not seen. */
            static bool is_live(const FrameSlice& in_slice,
                                const uint64_t&   in_frame_index);

            static FrameSlice plan(const DependencyAnalysis& in_analysis,
                                   const uint64_t&           in_target_frame_index);

        private:
            /* For each version of a resource, from a consumer that uses it back: the version where the way back ends because a writer replaced all of the
             * resource (or there is none before it), and the oldest version that has to be made from the consumer's: the same, unless the versions after the
             * first have written all of the extent that anything writes, and the ones before them do not show. The versions from that one to the consumer's
             * are needed, and so is the first (which holds what the extent does not).
             */
            static void compute_windows(const ResourceDependencies& in_dependencies,
                                        std::vector<size_t>*        out_end_version_vec_ptr,
                                        std::vector<size_t>*        out_first_version_vec_ptr);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H */
