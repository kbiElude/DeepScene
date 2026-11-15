/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tracks the contents of the vertex and index buffers of a Direct3D 8 device, so that the analysis can find the vertices of a draw. It is
 * used only when analyzing a workload, which has time for this. A converter that plays an application live does not keep buffer contents.
 *
 * A buffer is filled with the data of its unlocks, which the dumper stores with the call, at the offset given by the lock. The position
 * is the first element of a vertex, for the vertex formats that have one: in model space (XYZ, and XYZ with blend weights), or in pixels
 * with the reciprocal of the depth (pre-transformed, XYZRHW). The vertices of a vertex shader are not sampled, because its declaration is
 * not read. The vertices of a draw from application memory (DrawPrimitiveUP and DrawIndexedPrimitiveUP) are stored with the call and are
 * sampled the same way.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_GEOMETRY_TRACKER_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_GEOMETRY_TRACKER_H

#include "analysis/api_dump_analysis/types.h"
#include <array>
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

                /* The vertices that a draw from application memory uses (the data is stored with the call). */
                struct MemoryRange
                {
                    uint32_t       fvf             = 0;       /* 0 if the vertices are those of a vertex shader */
                    const uint8_t* index_data_ptr  = nullptr;
                    size_t         index_n_bytes   = 0;
                    uint32_t       index_stride    = 0;       /* 2 or 4 (see get_index_stride)                  */
                    bool           is_indexed      = false;
                    uint32_t       stride          = 0;
                    uint32_t       vertex_count    = 0;       /* indices, if the draw is indexed                */
                    const uint8_t* vertex_data_ptr = nullptr;
                    size_t         vertex_n_bytes  = 0;
                };

                D3D8GeometryTracker();

                /* Forgets a buffer that was destroyed. */
                void forget(const Analysis::APIDumpAnalysis::ResourceId& in_id);

                /* The format is D3DFMT_INDEX16 or D3DFMT_INDEX32. */
                void on_create_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                            const uint32_t&                              in_n_bytes,
                                            const uint32_t&                              in_format);

                void on_create_vertex_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                             const uint32_t&                              in_n_bytes);

                /* Records a lock of a buffer: the offset and the size (0: the rest of the buffer). The data is stored with the unlock. */
                void on_lock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                             const uint32_t&                              in_offset,
                             const uint32_t&                              in_n_bytes);

                /* Records what the application wrote to the locked part of the buffer. Records nothing if the buffer was locked for reading. */
                void on_unlock(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                               const uint8_t*                               in_data_ptr,
                               const uint64_t&                              in_n_bytes);

                /* A sample of the vertices of the draw, in model space; null if they cannot be determined. */
                std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample(const DrawRange& in_range);

                /* The bytes of an index: 2 for D3DFMT_INDEX16, 4 for D3DFMT_INDEX32. */
                static uint32_t get_index_stride(const uint32_t& in_format);

                /* Returns true if the vertex format has a position that is sampled, either in model space or in pixels (see is_pre_transformed_fvf). */
                static bool is_position_fvf(const uint32_t& in_fvf);

                /* Returns true if the positions are in pixels with the reciprocal of the depth (XYZRHW). The matrices do not transform such vertices. */
                static bool is_pre_transformed_fvf(const uint32_t& in_fvf);

                /* A sample of the vertices of a draw from application memory, like sample() does for a draw from buffers; null if they cannot be determined. */
                static std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample_memory(const MemoryRange& in_range);

            private:
                struct Buffer
                {
                    std::vector<uint8_t> byte_vec;
                    uint32_t             index_stride = 0;     /* 2 or 4, for an index buffer */
                    bool                 is_locked    = false;
                    uint32_t             lock_offset  = 0;
                    uint64_t             version      = 0;     /* changes with each write     */
                };

                /* The key of a sample: the buffers, the version of their contents, and the part of them that is drawn. */
                typedef std::array<uint64_t, 9> SampleKey;


                std::map<Analysis::APIDumpAnalysis::ResourceId, Buffer>                                   m_buffer_map;
                std::map<SampleKey, std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> > m_sample_map;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_GEOMETRY_TRACKER_H */
