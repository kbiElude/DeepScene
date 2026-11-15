/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The interface through which the model builder reports what happens to textures, as the front ends learn of it: an upload, a copy made by
 * rendering, the levels that the driver generates. A sink is optional. It must not call back into the builder that calls it.
 */
#ifndef APIDUMPTEXTURES_I_TEXTURE_SINK_H
#define APIDUMPTEXTURES_I_TEXTURE_SINK_H

#include "analysis/api_dump_textures/types.h"
#include <stddef.h>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class ITextureSink
        {
        public:
            virtual ~ITextureSink() = default;

            /* Receives one event. The builder fills in the call and the frame of the event, and leaves the blob for the sink to set. For an upload
             * that carries data, in_data_ptr points to the bytes, which are valid only during the call, and in_data_chunk_id is the data chunk of the
             * workload that holds them. Otherwise in_data_ptr is null.
             */
            virtual void on_texture_event(const TextureEvent& in_event,
                                          const uint8_t*      in_data_ptr,
                                          const size_t&       in_n_data_bytes,
                                          const uint32_t&     in_data_chunk_id) = 0;
        };
    }
}

#endif /* APIDUMPTEXTURES_I_TEXTURE_SINK_H */
