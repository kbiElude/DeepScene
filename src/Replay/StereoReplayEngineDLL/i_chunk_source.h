/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_I_CHUNK_SOURCE_H
#define STEREOREPLAYENGINEDLL_I_CHUNK_SOURCE_H

#include "Capture/APIDumpLoader/APIDumpLoader.h"
#include <stdint.h>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Where the data chunks of the calls being replayed are kept: the walker, or a buffer holding the calls of a frame. */
        class IChunkSource
        {
        public:
            virtual ~IChunkSource() = default;

            /* Returns nullptr for a chunk which the source does not have. */
            virtual const Capture::APIDumpLoader::DataChunk* get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const = 0;

            /* Of the process which was dumped. */
            virtual uint32_t get_pointer_n_bytes() const = 0;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_I_CHUNK_SOURCE_H */
