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
            /* What every API with vertex buffers does to know where a draw is: a few dozen of its vertices (see GeometrySample), whatever the API that made the draw. */
            class GeometrySampler
            {
            public:
                /* How many bytes of a vertex are read for its position: three floats, and a fourth, the reciprocal of the depth, for a vertex that is positioned in the picture. */
                static size_t get_position_n_bytes(const bool& in_is_pre_transformed);

                /* Some of the vertices of the draw, in the space of the model, spread over the draw; null if none can be told (the range has no room for a position, or each of the
                 * vertices is outside the data, or is not a number).
                 */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample(const GeometrySamplingRange& in_range);

            private:
                static bool is_finite(const std::array<float, 3>& in_position);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_GEOMETRY_SAMPLER_H */
