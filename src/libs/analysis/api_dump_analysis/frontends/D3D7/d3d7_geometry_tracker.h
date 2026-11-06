/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Where the vertices of a Direct3D 7 draw are, so that the draw can be told by them. Only for the analysis of a workload, which has
 * the time: a converter that plays an application as it runs does not keep the contents of buffers.
 *
 * The vertices of a draw are in the application's memory (the dumper stored them with the call), in the arrays of a strided draw
 * (stored with it too), or in a vertex buffer, whose contents are the data of its unlocks. A vertex buffer is locked whole, so what an
 * unlock stores starts at the first byte. The position of a vertex is the first thing in it, for the vertex formats that have one in the
 * space of the model (XYZ, and XYZ with blend weights); pre-transformed vertices (XYZRHW) are not sampled.
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
                /* The vertices that a draw uses. The positions are the first three floats of each vertex. */
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

                /* The vertex buffer is gone. */
                void forget(const Analysis::APIDumpAnalysis::ResourceId& in_id);

                /* The contents of a vertex buffer; nullptr if it was never told. */
                const std::vector<uint8_t>* get_vertex_buffer_bytes_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const;

                void on_create_vertex_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                             const uint64_t&                              in_n_bytes);

                /* What the application wrote to the buffer; nothing, if it was locked for reading. */
                void on_unlock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                               const uint8_t*                               in_data_ptr,
                               const uint64_t&                              in_n_bytes);

                /* Some of the vertices of the draw, in the space of the model; null if they cannot be told. */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample(const DrawRange& in_range);

            private:
                std::map<Analysis::APIDumpAnalysis::ResourceId, std::vector<uint8_t> > m_vertex_buffer_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D7_GEOMETRY_TRACKER_H */
