/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_FRAME_BUFFER_H
#define STEREO_REPLAY_ENGINEDLL_FRAME_BUFFER_H

#include "i_chunk_source.h"
#include <map>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Holds the calls of one frame and their data chunks. Both are copied out of the segments they were read from, because a
             * segment is released when the walker moves on. A frame can be executed again only from such a copy.
             */
            class FrameBuffer : public ::Replay::ReplayEngineDLL::IChunkSource
            {
            public:
                FrameBuffer();

                /* Copies the call, and the chunks it refers to that the buffer does not hold yet. */
                void add_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source);

                void clear();

                const std::vector<Capture::APIDumpLoader::DumpedAPICall>& get_call_vec       ()                                 const;
                const Capture::APIDumpLoader::DataChunk*                  get_data_chunk_ptr (const uint32_t& in_data_chunk_id) const override;
                uint32_t                                                  get_pointer_n_bytes()                                 const override;

                /* Copies a call. The call cannot be copied by plain assignment because it owns its returned value. */
                static Capture::APIDumpLoader::DumpedAPICall copy_call(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            private:
                std::vector<Capture::APIDumpLoader::DumpedAPICall>    m_call_vec;
                std::map<uint32_t, Capture::APIDumpLoader::DataChunk> m_chunk_map;
                uint32_t                                              m_pointer_n_bytes;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_FRAME_BUFFER_H */
