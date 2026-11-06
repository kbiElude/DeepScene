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
            /* Tells the calls of a Direct3D 8 application which depend on the size of what it renders to in pixels, so that it cannot be
             * made to render larger than it asks without it noticing (see ResolutionHazard). It is used where a workload is analyzed, and
             * where an application is made to render larger, so that both say the same.
             */
            class D3D8ResolutionHazards
            {
            public:
                /* Returns true, and which hazard it is, if the call is one. The arguments are those the call was made with. */
                static bool classify(const APIInterceptor::APIFunction&         in_function,
                                     const uint32_t&                            in_n_arguments,
                                     const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                     ResolutionHazard*                          out_hazard_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_RESOLUTION_HAZARDS_H */
