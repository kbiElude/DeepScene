/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_FRAME_BUFFER_H
#define STEREOREPLAYENGINEDLL_FRAME_BUFFER_H

#include "i_chunk_source.h"
#include <map>
#include <vector>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* The calls of one frame and their data chunks, copied out of the segments they were read from: a segment is gone once
         * the walker moves on, so a frame can be executed again only from a copy.
         */
        class FrameBuffer : public IChunkSource
        {
        public:
            FrameBuffer();

            /* Copies the call, and the chunks that it refers to and the buffer does not have yet. */
            void add_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                          const IChunkSource&                          in_chunk_source);

            void clear();

            const std::vector<Capture::APIDumpLoader::DumpedAPICall>& get_call_vec       ()                                 const;
            const Capture::APIDumpLoader::DataChunk*                  get_data_chunk_ptr (const uint32_t& in_data_chunk_id) const override;
            uint32_t                                                  get_pointer_n_bytes()                                 const override;

            /* A call can not be copied as it is: it owns its returned value. */
            static Capture::APIDumpLoader::DumpedAPICall copy_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

        private:
            std::vector<Capture::APIDumpLoader::DumpedAPICall>    m_call_vec;
            std::map<uint32_t, Capture::APIDumpLoader::DataChunk> m_chunk_map;
            uint32_t                                              m_pointer_n_bytes;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_FRAME_BUFFER_H */
