/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says which frames of a workload have to be made, for a replay that goes to a frame without making all the frames before it: the ones whose content
 * the target frame or any frame after it reads. It starts from the target and every frame after it, and goes back through the versions of the resources
 * which they use: the frame which wrote a version is live, and so is the one which wrote the version before it if the writer did not replace all of the
 * content (CarryKind and ResourceVersion say), and so on. A frame which replaces all of a resource ends the way back for that resource.
 *
 * The frames after the target are all live, so that a replay which goes on after it comes out as it does when everything is replayed.
 */
#ifndef APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H
#define APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H

#include "Analysis/APIDumpDependencies/types.h"

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
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_FRAME_SLICE_PLANNER_H */
