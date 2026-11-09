/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_REPEATABILITY_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_REPEATABILITY_H

#include "analysis/api_dump_dependencies/types.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* Whether a frame can be played again and again from one and the same state, for the report of overlays that plays it once for each draw that it leaves out:
             * only if each play begins with what the frame before left, and not with what the plays before it left. What the dependencies of the workload say tells
             * it: the frame makes and releases no resource, and each resource that it writes is either replaced by it before it is read (nothing of what it held
             * is used), or is one that a written extent is known of and the frame writes all of it (see ResourceDependencies::written_extent) before it is read, with
             * nothing that was in the resource flowing into what is written.
             */
            class FrameRepeatability
            {
            public:
                /* False if the frame cannot be played again from the state it leaves; the reason is then said (for people, and for a test). */
                static bool is_repeatable(const Analysis::APIDumpDependencies::DependencyAnalysis& in_analysis,
                                          const uint32_t&                                          in_frame_index,
                                          std::string*                                             out_reason_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_FRAME_REPEATABILITY_H */
