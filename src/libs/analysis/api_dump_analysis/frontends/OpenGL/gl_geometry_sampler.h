/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds the sample of the geometry of a draw whose positions OpenGL has read: the vertices are those that GeometrySampler chooses.
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
            class GLGeometrySampler
            {
            public:
                /* Builds the sample of a draw whose positions are all known. */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> make_sample(const std::vector<std::array<float, 3> >& in_position_vec);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_GEOMETRY_SAMPLER_H */
