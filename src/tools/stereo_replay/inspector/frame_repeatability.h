/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_INSPECTOR_FRAME_REPEATABILITY_H
#define STEREO_REPLAY_INSPECTOR_FRAME_REPEATABILITY_H

#include "analysis/api_dump_dependencies/types.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            /* Whether a frame can be played again and again from one and the same state. The overlay report needs this because it plays the frame once
             * for each draw that it leaves out. A repeat is correct only if each play begins with what the frame before left, not with what the earlier
             * plays left. The dependencies of the workload decide this. The frame must make and release no resource. Each resource that it writes must
             * either be replaced before it is read (nothing that it held is used), or have a known written extent that the frame writes completely
             * (see ResourceDependencies::written_extent) before it is read, with nothing from the old content flowing into what is written.
             */
            class FrameRepeatability
            {
            public:
                /* False if the frame cannot be played again from the state it leaves. The reason is then set for people (and for a test). */
                static bool is_repeatable(const Analysis::APIDumpDependencies::DependencyFileContent& in_analysis,
                                          const uint32_t&                                             in_frame_index,
                                          std::string*                                                out_reason_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_FRAME_REPEATABILITY_H */
