/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What is in the vertex and index buffers of a Direct3D 8 device, so that a draw can be told where its vertices are. Only for the
 * analysis of a workload, which has the time: a converter that plays an application as it runs does not keep the contents of buffers.
 *
 * A buffer is filled by the data of its unlocks, which are stored with them, at the offset that its lock said. The position of a
 * vertex is the first thing in it, for the vertex formats that have one in the space of the model (XYZ, and XYZ with blend weights);
 * the others (pre-transformed vertices, and the vertices of a vertex shader, whose declaration is not read) are not sampled.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_GEOMETRY_TRACKER_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_GEOMETRY_TRACKER_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <array>
#include <map>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D8GeometryTracker
            {
            public:
                /* The vertices that a draw uses. */
                struct DrawRange
                {
                    uint32_t                              base_vertex_index = 0; /* added to the indices                                     */
                    uint32_t                              first             = 0; /* the first index of an indexed draw, else the first vertex */
                    uint32_t                              fvf               = 0; /* 0 if the vertices are those of a vertex shader           */
                    Analysis::APIDumpAnalysis::ResourceId index_buffer_id   = Analysis::APIDumpAnalysis::ResourceId::NONE;
                    bool                                  is_indexed        = false;
                    uint32_t                              stride            = 0;
                    uint32_t                              vertex_count      = 0; /* indices, if the draw is indexed                          */
                    Analysis::APIDumpAnalysis::ResourceId vertex_buffer_id  = Analysis::APIDumpAnalysis::ResourceId::NONE;
                };

                D3D8GeometryTracker();

                /* The buffer is gone. */
                void forget(const Analysis::APIDumpAnalysis::ResourceId& in_id);

                /* The format is that of D3DFMT_INDEX16 or D3DFMT_INDEX32. */
                void on_create_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                            const uint32_t&                              in_n_bytes,
                                            const uint32_t&                              in_format);

                void on_create_vertex_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                             const uint32_t&                              in_n_bytes);

                /* A lock of a buffer: where, and how much (0: all that is left). The data comes with the unlock. */
                void on_lock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                             const uint32_t&                              in_offset,
                             const uint32_t&                              in_n_bytes);

                /* What the application wrote to the locked part of the buffer; none, if it was locked for reading. */
                void on_unlock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                               const uint8_t*                               in_data_ptr,
                               const uint64_t&                              in_n_bytes);

                /* Some of the vertices of the draw, in the space of the model; null if they cannot be told. */
                std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample(const DrawRange& in_range);

            private:
                struct Buffer
                {
                    std::vector<uint8_t> byte_vec;
                    uint32_t             index_stride = 0;     /* 2 or 4, for an index buffer */
                    bool                 is_locked    = false;
                    uint32_t             lock_offset  = 0;
                    uint64_t             version      = 0;     /* changes with each write     */
                };

                /* What a sample is of: the buffers (and the version of what is in them), and the part of them that is drawn. */
                typedef std::array<uint64_t, 9> SampleKey;


                static bool is_position_fvf(const uint32_t& in_fvf);


                std::map<Analysis::APIDumpAnalysis::ResourceId, Buffer>                                   m_buffer_map;
                std::map<SampleKey, std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> > m_sample_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_GEOMETRY_TRACKER_H */
