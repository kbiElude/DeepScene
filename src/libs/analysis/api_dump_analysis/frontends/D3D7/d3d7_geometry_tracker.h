/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Finds the vertices of a Direct3D 7 draw so that the analysis can measure the draw. It is used only when analyzing a workload, which
 * has time for this. A converter that plays an application live does not keep buffer contents.
 *
 * The vertices of a draw are in one of three places: the application's memory (the dumper stored them with the call), the arrays of a
 * strided draw (also stored with the call), or a vertex buffer, whose contents are the data of its unlocks. A vertex buffer is always
 * locked as a whole, so the data stored by an unlock starts at the first byte. The position is the first element of a vertex, for the
 * vertex formats that have a position in model space (XYZ, and XYZ with blend weights). Pre-transformed vertices (XYZRHW) are not sampled here.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D7_GEOMETRY_TRACKER_H
#define APIDUMPANALYSIS_FRONTENDS_D3D7_GEOMETRY_TRACKER_H

#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D7GeometryTracker
            {
            public:
                /* The vertices that a draw uses. The position is the first three floats of each vertex, or four if the vertex is pre-transformed. */
                struct DrawRange
                {
                    uint32_t        base_vertex_index = 0;       /* added to the indices                                     */
                    uint32_t        first             = 0;       /* the first index of an indexed draw, else the first vertex */
                    const uint8_t*  index_data_ptr    = nullptr; /* 16-bit indices; null if the draw is not indexed         */
                    size_t          index_n_bytes     = 0;
                    bool            is_pre_transformed = false;   /* the vertices are positioned in pixels, with the reciprocal of their depth (x, y, z, 1 / w) */
                    uint32_t        stride            = 0;
                    uint32_t        vertex_count      = 0;       /* indices, if the draw is indexed                          */
                    const uint8_t*  vertex_data_ptr   = nullptr;
                    size_t          vertex_n_bytes    = 0;
                };

                /* Forgets a vertex buffer that was destroyed. */
                void forget(const Analysis::APIDumpAnalysis::ResourceId& in_id);

                /* The contents of a vertex buffer; nullptr if none were recorded. */
                const std::vector<uint8_t>* get_vertex_buffer_bytes_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const;

                void on_create_vertex_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                             const uint64_t&                              in_n_bytes);

                /* Records what the application wrote to the buffer. Records nothing if the buffer was locked for reading. */
                void on_unlock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                               const uint8_t*                               in_data_ptr,
                               const uint64_t&                              in_n_bytes);

                /* A sample of the vertices of the draw, in model space; null if they cannot be determined. */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample(const DrawRange& in_range);

            private:
                std::map<Analysis::APIDumpAnalysis::ResourceId, std::vector<uint8_t> > m_vertex_buffer_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D7_GEOMETRY_TRACKER_H */
