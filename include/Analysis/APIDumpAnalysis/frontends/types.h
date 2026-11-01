/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_TYPES_H
#define APIDUMPANALYSIS_FRONTENDS_TYPES_H

#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* Where an analysis session is, for the progress it reports. */
            struct SessionProgress
            {
                double   demo_seconds           = 0.0; /* from the first call to the last one read */
                uint64_t n_calls                = 0;
                uint64_t n_frame_shapes         = 0;
                uint64_t n_frames               = 0;
                uint64_t n_pass_structures      = 0;
                uint64_t n_segments             = 0;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_TYPES_H */
