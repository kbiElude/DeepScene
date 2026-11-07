/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_geometry_sampler.h"

size_t Analysis::APIDumpAnalysis::Frontends::GlGeometrySampler::get_n_samples(const size_t& in_n_vertices)
{
    return (in_n_vertices < MAX_N_SAMPLES) ? in_n_vertices
                                           : MAX_N_SAMPLES;
}

size_t Analysis::APIDumpAnalysis::Frontends::GlGeometrySampler::get_sample_vertex(const size_t& in_n_sample,
                                                                                  const size_t& in_n_vertices)
{
    const size_t n_samples = get_n_samples(in_n_vertices);

    return in_n_sample * in_n_vertices / n_samples;
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::GlGeometrySampler::make_sample(const std::vector<std::array<float, 3> >& in_position_vec)
{
    const size_t n_positions = in_position_vec.size();
    const size_t n_samples   = get_n_samples(n_positions);
    auto         result_ptr  = std::make_shared<Analysis::APIDumpAnalysis::GeometrySample>();

    for (size_t n_sample = 0;
                n_sample < n_samples;
              ++n_sample)
    {
        const size_t vertex = get_sample_vertex(n_sample,
                                                n_positions);

        result_ptr->position_vec.push_back(in_position_vec.at(vertex) );
    }

    return result_ptr;
}
