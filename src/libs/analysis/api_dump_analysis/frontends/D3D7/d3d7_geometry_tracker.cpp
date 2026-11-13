/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D7/d3d7_geometry_tracker.h"
#include "analysis/api_dump_analysis/frontends/shared/geometry_sampler.h"
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
    GeometrySamplingRange range;

    range.base_vertex_index  = in_range.base_vertex_index;
    range.first              = in_range.first;
    range.index_data_ptr     = in_range.index_data_ptr;
    range.index_n_bytes      = in_range.index_n_bytes;
    range.index_stride       = sizeof(uint16_t);
    range.is_indexed         = (in_range.index_data_ptr != nullptr);
    range.is_pre_transformed = in_range.is_pre_transformed;
    range.stride             = in_range.stride;
    range.vertex_count       = in_range.vertex_count;
    range.vertex_data_ptr    = in_range.vertex_data_ptr;
    range.vertex_n_bytes     = in_range.vertex_n_bytes;

    return GeometrySampler::sample(range);
}
