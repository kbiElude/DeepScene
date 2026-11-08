/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Where the model builder tells what happens to the textures, as the front ends learn of it: an upload, a copy that rendering made, the levels that the driver
 * generates. A sink is optional, and must not call the builder which calls it.
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

            /* The event, with the call and the frame it is in (the builder fills them), and no blob (the sink makes it). An upload that has data comes with the bytes of
             * it, which are valid for the call only, and with the data chunk of the workload that they are in; with none the data pointer is null.
             */
            virtual void on_texture_event(const TextureEvent& in_event,
                                          const uint8_t*      in_data_ptr,
                                          const size_t&       in_n_data_bytes,
                                          const uint32_t&     in_data_chunk_id) = 0;
        };
    }
}

#endif /* APIDUMPTEXTURES_I_TEXTURE_SINK_H */
