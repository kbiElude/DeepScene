/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What is told of the use of resources, as the model builder learns of it: every read and write, in the order of the calls, and where the frames
 * end. The dependencies between frames are made of it (see AccessEvent for what an access is). A sink is optional, and must not call the
 * builder which calls it.
 */
#ifndef APIDUMPANALYSIS_ACCESS_SINK_H
#define APIDUMPANALYSIS_ACCESS_SINK_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class IAccessSink
        {
        public:
            virtual ~IAccessSink() = default;

            /* A use of a resource. The accesses of one call come in the order the call makes them; those of a draw come with the reads before
             * the writes.
             */
            virtual void on_access(const AccessEvent& in_event) = 0;

            /* The frame ended (a present); the accesses which followed the previous one are its own. The frames are numbered from 0. */
            virtual void on_frame_ended(const uint64_t& in_frame_index) = 0;
        };
    }
}

#endif /* APIDUMPANALYSIS_ACCESS_SINK_H */
