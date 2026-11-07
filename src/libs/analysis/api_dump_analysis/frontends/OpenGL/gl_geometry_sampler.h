/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Which of the vertices of a draw are a sample of its geometry: a few, spread evenly over the draw, so that the sample says what the draw does
 * from its first vertex to its last.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_GEOMETRY_SAMPLER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_GEOMETRY_SAMPLER_H

#include "analysis/api_dump_analysis/types.h"
#include <array>
#include <memory>
#include <stddef.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GlGeometrySampler
            {
            public:
                /* The number of vertices of a draw that are sampled (all of them if they are not many). */
                static size_t get_n_samples(const size_t& in_n_vertices);

                /* The vertex of the draw that the sample of the given number is. */
                static size_t get_sample_vertex(const size_t& in_n_sample,
                                                const size_t& in_n_vertices);

                /* The sample of a draw whose positions are all known. */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> make_sample(const std::vector<std::array<float, 3> >& in_position_vec);

            private:
                static constexpr size_t MAX_N_SAMPLES = 64;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_GEOMETRY_SAMPLER_H */
