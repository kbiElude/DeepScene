/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What a flexible vertex format says of the vertices, for every API that has one.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_VERTEX_FORMAT_H
#define APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_VERTEX_FORMAT_H

#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class FixedFunctionVertexFormat
            {
            public:
                /* Whether the positions are in pixels (D3DFVF_XYZRHW). */
                static bool is_pre_transformed(const uint32_t& in_format);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_VERTEX_FORMAT_H */
