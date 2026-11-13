/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/geometry_sampler.h"
#include <algorithm>
#include <array>
#include <math.h>
#include <string.h>

size_t Analysis::APIDumpAnalysis::Frontends::GeometrySampler::get_position_n_bytes(const bool& in_is_pre_transformed)
{
    constexpr size_t N_POSITION_FLOATS        = 3;
    constexpr size_t N_PRE_TRANSFORMED_FLOATS = 4; /* x, y, z and the reciprocal of the depth */

    return ((in_is_pre_transformed) ? N_PRE_TRANSFORMED_FLOATS
                                    : N_POSITION_FLOATS) * sizeof(float);
}

bool Analysis::APIDumpAnalysis::Frontends::GeometrySampler::is_finite(const std::array<float, 3>& in_position)
{
    const bool is_x_finite = ::isfinite(in_position[0]);
    const bool is_y_finite = ::isfinite(in_position[1]);
    const bool is_z_finite = ::isfinite(in_position[2]);

    return (is_x_finite &&
            is_y_finite &&
            is_z_finite);
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(const GeometrySamplingRange& in_range)
{
    /* A few dozen vertices tell where a mesh is, and what the analysis makes of them is a percentile of the depth. */
    constexpr size_t MAX_N_POINTS           = 64;
    constexpr size_t RECIPROCAL_DEPTH_FLOAT = 3;

    const size_t n_position_bytes = get_position_n_bytes(in_range.is_pre_transformed);

    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;

    const bool has_vertices = (in_range.vertex_data_ptr != nullptr &&
                               in_range.vertex_count    >  0       &&
                               in_range.stride          >= n_position_bytes);

    if (has_vertices)
    {
        auto         sample_ptr = std::make_shared<Analysis::APIDumpAnalysis::GeometrySample>();
        const size_t n_points   = std::min<size_t>(in_range.vertex_count,
                                                   MAX_N_POINTS);

        sample_ptr->is_pre_transformed = in_range.is_pre_transformed;

        for (size_t n_point = 0;
                    n_point < n_points;
                  ++n_point)
        {
            /* Spread over the draw, so that a mesh whose parts are in different places is told by all of them. */
            const uint64_t slot         = static_cast<uint64_t>(n_point) * in_range.vertex_count / n_points;
            uint64_t       vertex_index = in_range.first + slot;
            bool           is_valid     = true;

            if (in_range.is_indexed)
            {
                const uint64_t offset = (in_range.first + slot) * in_range.index_stride;

                is_valid = (offset + in_range.index_stride <= in_range.index_n_bytes);

                if (is_valid)
                {
                    uint32_t index = 0;

                    ::memcpy(&index,
                             in_range.index_data_ptr + offset,
                             in_range.index_stride);

                    vertex_index = static_cast<uint64_t>(index) + in_range.base_vertex_index;
                }
            }

            const uint64_t position_offset = vertex_index * in_range.stride;

            is_valid = (is_valid && position_offset + n_position_bytes <= in_range.vertex_n_bytes);

            if (is_valid)
            {
                std::array<float, 4> read_floats = {};
                std::array<float, 3> position;

                ::memcpy(read_floats.data(),
                         in_range.vertex_data_ptr + position_offset,
                         n_position_bytes);

                position = {read_floats[0], read_floats[1], read_floats[2]};

                /* For vertices which are positioned in the picture, the third coordinate is the reciprocal of the depth, and not the depth buffer's. */
                if (in_range.is_pre_transformed)
                {
                    position[2] = read_floats[RECIPROCAL_DEPTH_FLOAT];
                }

                const bool is_position_finite = is_finite(position);

                if (is_position_finite)
                {
                    sample_ptr->position_vec.push_back(position);
                }
            }
        }

        if (sample_ptr->position_vec.empty() == false)
        {
            result_ptr = sample_ptr;
        }
    }

    return result_ptr;
}
