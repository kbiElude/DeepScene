/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_CLEAR_OPERATION_FACTORY_H
#define APIDUMPANALYSIS_FRONTENDS_CLEAR_OPERATION_FACTORY_H

#include "analysis/api_dump_analysis/types.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class ClearOperationFactory
            {
            public:
                /* The clear of the APIs whose clear call has flags for what to clear, a color in ARGB (a byte each), a depth and a stencil. */
                static Analysis::APIDumpAnalysis::ClearOperation create(const bool&     in_clears_color,
                                                                        const bool&     in_clears_depth,
                                                                        const bool&     in_clears_stencil,
                                                                        const uint32_t& in_color_argb,
                                                                        const float&    in_depth,
                                                                        const uint32_t& in_stencil);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_CLEAR_OPERATION_FACTORY_H */
