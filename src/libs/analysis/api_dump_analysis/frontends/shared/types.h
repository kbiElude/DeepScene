/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_TYPES_H
#define APIDUMPANALYSIS_FRONTENDS_TYPES_H

#include <stddef.h>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* The vertices that a draw uses, as the memory of an API holds them (see GeometrySampler). The positions are the first three floats of each vertex, or four for a vertex that is
             * pre-transformed.
             */
            struct GeometrySamplingRange
            {
                uint32_t       base_vertex_index  = 0;       /* added to the indices                                      */
                uint32_t       first              = 0;       /* the first index of an indexed draw, else the first vertex */
                const uint8_t* index_data_ptr     = nullptr; /* of the indices of an indexed draw                          */
                size_t         index_n_bytes      = 0;
                uint32_t       index_stride       = 0;       /* 2 or 4: the bytes of an index                              */
                bool           is_indexed         = false;
                bool           is_pre_transformed = false;   /* the vertices are positioned in pixels, with the reciprocal of their depth (x, y, z, 1 / w) */
                uint32_t       stride             = 0;
                uint32_t       vertex_count       = 0;       /* indices, if the draw is indexed                            */
                const uint8_t* vertex_data_ptr    = nullptr;
                size_t         vertex_n_bytes     = 0;
            };

            /* Where an analysis session is, for the progress it reports. */
            struct SessionProgress
            {
                double   demo_seconds           = 0.0; /* from the first call to the last one read */
                uint64_t n_calls                = 0;
                uint64_t n_frame_shapes         = 0;
                uint64_t n_frames               = 0;
                uint64_t n_pass_structures      = 0;
                uint64_t n_segments             = 0;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_TYPES_H */
