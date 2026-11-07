/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D7/d3d7_geometry_tracker.h"
#include "analysis/api_dump_analysis/frontends/shared/position_checker.h"
#include <algorithm>
#include <array>
#include <string.h>

void Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::forget(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_vertex_buffer_map.erase(in_id);
}

const std::vector<uint8_t>* Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::get_vertex_buffer_bytes_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const
{
    const auto buffer_iterator = m_vertex_buffer_map.find(in_id);

    return (buffer_iterator != m_vertex_buffer_map.end() ) ? &buffer_iterator->second
                                                           : nullptr;
}

void Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::on_create_vertex_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                                        const uint64_t&                              in_n_bytes)
{
    m_vertex_buffer_map[in_id].assign(static_cast<size_t>(in_n_bytes),
                                      0);
}

void Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::on_unlock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                          const uint8_t*                               in_data_ptr,
                                                                          const uint64_t&                              in_n_bytes)
{
    const auto buffer_iterator = m_vertex_buffer_map.find(in_id);

    if (buffer_iterator != m_vertex_buffer_map.end() &&
        in_data_ptr     != nullptr)
    {
        auto& byte_vec = buffer_iterator->second;

        if (byte_vec.size() < in_n_bytes)
        {
            byte_vec.resize(static_cast<size_t>(in_n_bytes),
                            0);
        }

        ::memcpy(byte_vec.data(),
                 in_data_ptr,
                 static_cast<size_t>(in_n_bytes) );
    }
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(const DrawRange& in_range)
{
    /* A few dozen vertices tell where a mesh is, and what the analysis makes of them is a percentile of the depth. */
    constexpr size_t MAX_N_POINTS              = 64;
    constexpr size_t N_PRE_TRANSFORMED_FLOATS  = 4; /* x, y, z and the reciprocal of the depth */
    constexpr size_t N_POSITION_FLOATS         = 3;
    constexpr size_t RECIPROCAL_DEPTH_FLOAT    = 3;

    /* What is read of a vertex: its position, which for one that is pre-transformed goes on to the reciprocal of its depth. */
    const size_t n_position_bytes = ((in_range.is_pre_transformed) ? N_PRE_TRANSFORMED_FLOATS
                                                                   : N_POSITION_FLOATS) * sizeof(float);

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

            if (in_range.index_data_ptr != nullptr)
            {
                const uint64_t offset = (in_range.first + slot) * sizeof(uint16_t);

                is_valid = (offset + sizeof(uint16_t) <= in_range.index_n_bytes);

                if (is_valid)
                {
                    uint16_t index = 0;

                    ::memcpy(&index,
                             in_range.index_data_ptr + offset,
                             sizeof(index) );

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

                const bool is_position_finite = PositionChecker::is_finite(position);

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
