/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What a flexible vertex format (FVF) says of the vertices, the same for every API that has one: Direct3D 7 and 8 use the same bits.
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
                /* Whether the positions of the vertices are in pixels (x, y, z and the reciprocal of the depth, which no matrix moves): the format has D3DFVF_XYZRHW. */
                static bool is_pre_transformed(const uint32_t& in_format);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_VERTEX_FORMAT_H */
