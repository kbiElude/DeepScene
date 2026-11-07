/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_geometry_tracker.h"
#include "analysis/api_dump_analysis/frontends/shared/position_checker.h"
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

bool Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::is_position_fvf(const uint32_t& in_fvf)
{
    /* The position mask of an FVF code: 2 is XYZ, 4 is XYZRHW (in the space of the screen), and 6 to 14 are XYZ and blend weights. */
    constexpr uint32_t POSITION_MASK = 0x00E;
    constexpr uint32_t XYZ           = 0x002;
    constexpr uint32_t XYZB1         = 0x006;

    const uint32_t position = (in_fvf & POSITION_MASK);

    return (position == XYZ ||
            position >= XYZB1);
}

void Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::on_create_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                                                                       const uint32_t&                              in_n_bytes,
                                                                                       const uint32_t&                              in_format)
{
    constexpr uint32_t FORMAT_INDEX32 = 102; /* D3DFMT_INDEX32; D3DFMT_INDEX16 is 101 */
    constexpr uint32_t MAX_N_BYTES    = 256 * 1024 * 1024;

    if (in_n_bytes <= MAX_N_BYTES)
    {
        Buffer buffer;

        buffer.byte_vec.assign(in_n_bytes,
                               0);

        buffer.index_stride = (in_format == FORMAT_INDEX32) ? sizeof(uint32_t)
                                                            : sizeof(uint16_t);

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

    /* The size is not needed: the data that comes with the unlock says how much was locked. */
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
    /* A few dozen vertices tell where a mesh is, and what the analysis makes of them is a percentile of the depth; the cache is bounded
     * since a buffer that is written to each frame makes a sample of its own each time.
     */
    constexpr size_t MAX_N_POINTS  = 64;
    constexpr size_t MAX_N_SAMPLES = 4096;
    constexpr size_t N_POSITION_BYTES = 3 * sizeof(float);

    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;
    const auto                                                       vertex_iterator = m_buffer_map.find(in_range.vertex_buffer_id);
    const auto                                                       index_iterator  = m_buffer_map.find(in_range.index_buffer_id);

    const bool has_vertices = (vertex_iterator != m_buffer_map.end() &&
                               in_range.vertex_count > 0             &&
                               in_range.stride       >= N_POSITION_BYTES);

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
                    auto sample_ptr = std::make_shared<Analysis::APIDumpAnalysis::GeometrySample>();

                    const size_t n_points = std::min<size_t>(in_range.vertex_count,
                                                             MAX_N_POINTS);

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
                            const auto&    index_buffer = index_iterator->second;
                            const uint64_t offset       = (in_range.first + slot) * index_buffer.index_stride;

                            is_valid = (offset + index_buffer.index_stride <= index_buffer.byte_vec.size() );

                            if (is_valid)
                            {
                                uint32_t index = 0;

                                ::memcpy(&index,
                                         index_buffer.byte_vec.data() + offset,
                                         index_buffer.index_stride);

                                vertex_index = static_cast<uint64_t>(index) + in_range.base_vertex_index;
                            }
                        }

                        const uint64_t position_offset = vertex_index * in_range.stride;

                        is_valid = (is_valid && position_offset + N_POSITION_BYTES <= vertex_buffer.byte_vec.size() );

                        if (is_valid)
                        {
                            std::array<float, 3> position;

                            ::memcpy(position.data(),
                                     vertex_buffer.byte_vec.data() + position_offset,
                                     N_POSITION_BYTES);

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
