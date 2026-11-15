/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Receives the resource accesses that the model builder learns about: every read and write in call order, and where each frame ends. The
 * dependencies between frames are computed from them (see AccessEvent for what an access is). A sink is optional. It must not call back into
 * the builder that calls it.
 */
#ifndef APIDUMPANALYSIS_I_ACCESS_SINK_H
#define APIDUMPANALYSIS_I_ACCESS_SINK_H

#include "analysis/api_dump_analysis/types.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class IAccessSink
        {
        public:
            virtual ~IAccessSink() = default;

            /* One access to a resource. The accesses of a call arrive in the order the call makes them. For a draw, the reads arrive before
             * the writes.
             */
            virtual void on_access(const AccessEvent& in_event) = 0;

            /* A frame ended (at a present). The accesses since the previous on_frame_ended belong to this frame. Frames are numbered from 0.
             * The calls of the frame run from the first to the last call index, inclusive (the call that ends the frame is the last). The
             * indices are the call numbers that the builder was given.
             */
            virtual void on_frame_ended(const uint64_t& in_frame_index,
                                        const uint64_t& in_first_call_index,
                                        const uint64_t& in_last_call_index) = 0;
        };
    }
}

#endif /* APIDUMPANALYSIS_I_ACCESS_SINK_H */
