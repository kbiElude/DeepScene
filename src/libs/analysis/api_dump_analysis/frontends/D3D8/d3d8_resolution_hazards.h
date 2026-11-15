/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_RESOLUTION_HAZARDS_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_RESOLUTION_HAZARDS_H

#include "analysis/api_dump_analysis/types.h"
#include "common/api_function_argument.h"
#include "common/callbacks.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* Identifies the calls of a Direct3D 8 application that depend on the pixel size of its render target. If the application renders
             * larger than it asked for, these calls would reveal it (see ResolutionHazard). The workload analysis and the live upscaling both
             * use this class, so they agree on which calls are hazards.
             */
            class D3D8ResolutionHazards
            {
            public:
                /* Returns true and sets the hazard if the call is one. The arguments are the ones the call was made with. */
                static bool classify(const APIInterceptor::APIFunction&         in_function,
                                     const uint32_t&                            in_n_arguments,
                                     const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                     ResolutionHazard*                          out_hazard_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_RESOLUTION_HAZARDS_H */
