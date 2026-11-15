/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_geometry_tracker.h"
#include "analysis/api_dump_analysis/frontends/shared/geometry_sampler.h"
#include <algorithm>
#include <string.h>

Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::D3D8GeometryTracker()
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::forget(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_buffer_map.erase(in_id);
}

uint32_t Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::get_index_stride(const uint32_t& in_format)
{
    constexpr uint32_t FORMAT_INDEX32 = 102; /* D3DFMT_INDEX32; D3DFMT_INDEX16 is 101 */

    return (in_format == FORMAT_INDEX32) ? sizeof(uint32_t)
                                         : sizeof(uint16_t);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::is_position_fvf(const uint32_t& in_fvf)
{
    /* The position mask of an FVF code: 2 is XYZ, 4 is XYZRHW (in pixels), and 6 to 14 are XYZ and blend weights. */
    constexpr uint32_t POSITION_MASK = 0x00E;
    constexpr uint32_t XYZ           = 0x002;
    constexpr uint32_t XYZB1         = 0x006;
    constexpr uint32_t XYZRHW        = 0x004;

    const uint32_t position = (in_fvf & POSITION_MASK);

    return (position == XYZ    ||
            position == XYZRHW ||
            position >= XYZB1);
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::is_pre_transformed_fvf(const uint32_t& in_fvf)
{
    constexpr uint32_t POSITION_MASK = 0x00E;
    constexpr uint32_t XYZRHW        = 0x004;

    return ((in_fvf & POSITION_MASK) == XYZRHW);
}

void Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::on_create_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                                       const uint32_t&                              in_n_bytes,
                                                                                       const uint32_t&                              in_format)
{
    constexpr uint32_t MAX_N_BYTES = 256 * 1024 * 1024;

    if (in_n_bytes <= MAX_N_BYTES)
    {
        Buffer buffer;

        buffer.byte_vec.assign(in_n_bytes,
                               0);

        buffer.index_stride = get_index_stride(in_format);

        m_buffer_map[in_id] = std::move(buffer);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::on_create_vertex_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                                        const uint32_t&                              in_n_bytes)
{
    constexpr uint32_t MAX_N_BYTES = 256 * 1024 * 1024;

    if (in_n_bytes <= MAX_N_BYTES)
    {
        Buffer buffer;

        buffer.byte_vec.assign(in_n_bytes,
                               0);

        m_buffer_map[in_id] = std::move(buffer);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::on_lock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                        const uint32_t&                              in_offset,
                                                                        const uint32_t&                              in_n_bytes)
{
    const auto buffer_iterator = m_buffer_map.find(in_id);

    /* The size is not needed: the data stored with the unlock says how much was locked. */
    (void) in_n_bytes;

    if (buffer_iterator != m_buffer_map.end() )
    {
        buffer_iterator->second.is_locked   = true;
        buffer_iterator->second.lock_offset = in_offset;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::on_unlock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                          const uint8_t*                               in_data_ptr,
                                                                          const uint64_t&                              in_n_bytes)
{
    const auto buffer_iterator = m_buffer_map.find(in_id);

    if (buffer_iterator != m_buffer_map.end() )
    {
        auto& buffer = buffer_iterator->second;

        if (buffer.is_locked           &&
            in_data_ptr != nullptr     &&
            buffer.lock_offset < buffer.byte_vec.size() )
        {
            const uint64_t n_bytes = std::min<uint64_t>(in_n_bytes,
                                                        buffer.byte_vec.size() - buffer.lock_offset);

            ::memcpy(buffer.byte_vec.data() + buffer.lock_offset,
                     in_data_ptr,
                     static_cast<size_t>(n_bytes) );

            buffer.version++;
        }

        buffer.is_locked = false;
    }
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::sample(const DrawRange& in_range)
{
    /* The cache is bounded because a buffer that is written every frame produces a new sample every time. */
    constexpr size_t MAX_N_SAMPLES = 4096;

    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;
    const auto                                                       index_iterator     = m_buffer_map.find(in_range.index_buffer_id);
    const bool                                                       is_pre_transformed = is_pre_transformed_fvf(in_range.fvf);
    const size_t                                                     n_position_bytes   = GeometrySampler::get_position_n_bytes(is_pre_transformed);
    const auto                                                       vertex_iterator    = m_buffer_map.find(in_range.vertex_buffer_id);

    const bool has_vertices = (vertex_iterator != m_buffer_map.end() &&
                               in_range.vertex_count > 0             &&
                               in_range.stride       >= n_position_bytes);

    if (has_vertices)
    {
        const bool has_format = is_position_fvf(in_range.fvf);

        if (has_format)
        {
            const bool has_indices = (in_range.is_indexed == false ||
                                      index_iterator != m_buffer_map.end() );

            if (has_indices)
            {
                const auto& vertex_buffer = vertex_iterator->second;

                const SampleKey key = {Analysis::APIDumpAnalysis::get_resource_id_value(in_range.vertex_buffer_id),
                                       vertex_buffer.version,
                                       (in_range.is_indexed) ? Analysis::APIDumpAnalysis::get_resource_id_value(in_range.index_buffer_id) : 0,
                                       (in_range.is_indexed) ? index_iterator->second.version : 0,
                                       in_range.base_vertex_index,
                                       in_range.first,
                                       in_range.vertex_count,
                                       in_range.stride,
                                       in_range.fvf};

                const auto sample_iterator = m_sample_map.find(key);

                if (sample_iterator != m_sample_map.end() )
                {
                    result_ptr = sample_iterator->second;
                }
                else
                {
                    GeometrySamplingRange range;

                    range.base_vertex_index  = in_range.base_vertex_index;
                    range.first              = in_range.first;
                    range.index_data_ptr     = (in_range.is_indexed) ? index_iterator->second.byte_vec.data() : nullptr;
                    range.index_n_bytes      = (in_range.is_indexed) ? index_iterator->second.byte_vec.size() : 0;
                    range.index_stride       = (in_range.is_indexed) ? index_iterator->second.index_stride    : 0;
                    range.is_indexed         = in_range.is_indexed;
                    range.is_pre_transformed = is_pre_transformed;
                    range.stride             = in_range.stride;
                    range.vertex_count       = in_range.vertex_count;
                    range.vertex_data_ptr    = vertex_buffer.byte_vec.data();
                    range.vertex_n_bytes     = vertex_buffer.byte_vec.size();

                    result_ptr = GeometrySampler::sample(range);

                    if (m_sample_map.size() >= MAX_N_SAMPLES)
                    {
                        m_sample_map.clear();
                    }

                    m_sample_map[key] = result_ptr;
                }
            }
        }
    }

    return result_ptr;
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::sample_memory(const MemoryRange& in_range)
{
    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;
    const bool                                                       has_format         = is_position_fvf(in_range.fvf);
    const bool                                                       is_pre_transformed = is_pre_transformed_fvf(in_range.fvf);
    const size_t                                                     n_position_bytes   = GeometrySampler::get_position_n_bytes(is_pre_transformed);

    const bool has_vertices = (has_format                         &&
                               in_range.vertex_count > 0          &&
                               in_range.stride       >= n_position_bytes);

    if (has_vertices)
    {
        GeometrySamplingRange range;

        range.index_data_ptr     = in_range.index_data_ptr;
        range.index_n_bytes      = in_range.index_n_bytes;
        range.index_stride       = in_range.index_stride;
        range.is_indexed         = in_range.is_indexed;
        range.is_pre_transformed = is_pre_transformed;
        range.stride             = in_range.stride;
        range.vertex_count       = in_range.vertex_count;
        range.vertex_data_ptr    = in_range.vertex_data_ptr;
        range.vertex_n_bytes     = in_range.vertex_n_bytes;

        result_ptr = GeometrySampler::sample(range);
    }

    return result_ptr;
}
