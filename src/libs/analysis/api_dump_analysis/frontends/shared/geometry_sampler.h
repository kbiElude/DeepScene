/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_GEOMETRY_SAMPLER_H
#define APIDUMPANALYSIS_FRONTENDS_GEOMETRY_SAMPLER_H

#include "analysis/api_dump_analysis/frontends/shared/types.h"
#include "analysis/api_dump_analysis/types.h"
#include <array>
#include <memory>
#include <stddef.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* Finds where a draw is for every API that has vertex buffers: it takes a few dozen of the vertices of the draw (see GeometrySample),
             * whichever API made the draw. */
            class GeometrySampler
            {
            public:
                /* The number of bytes that are read for the position of a vertex: three floats, plus a fourth float (the reciprocal of the depth)
                 * for a vertex that is already positioned in the picture. */
                static size_t get_position_n_bytes(const bool& in_is_pre_transformed);

                /* Some of the vertices of the draw, in model space, spread over the draw. Returns null if no sample can be taken: the range has
                 * no room for a position, or every vertex is outside the data or is not a finite number.
                 */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample(const GeometrySamplingRange& in_range);

            private:
                static bool is_finite(const std::array<float, 3>& in_position);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_GEOMETRY_SAMPLER_H */
