/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_OVERLAY_CHUNK_SOURCE_H
#define STEREO_REPLAY_ENGINEDLL_OVERLAY_CHUNK_SOURCE_H

#include "i_chunk_source.h"
#include <map>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* A chunk source that holds chunks of its own on top of another chunk source (the base). A chunk in the overlay hides the
             * base chunk with the same ID. The base must outlive the overlay.
             */
            class OverlayChunkSource : public ::Replay::ReplayEngineDLL::IChunkSource
            {
            public:
                explicit OverlayChunkSource(const ::Replay::ReplayEngineDLL::IChunkSource& in_base);

                const Capture::APIDumpLoader::DataChunk* get_data_chunk_ptr (const uint32_t& in_data_chunk_id) const override;
                uint32_t                                 get_pointer_n_bytes()                                 const override;

                /* Adds a chunk. If the base has a chunk with the same ID, the new chunk hides it. */
                void set_chunk(const uint32_t&                          in_data_chunk_id,
                               const Capture::APIDumpLoader::DataChunk& in_chunk);

            private:
                const ::Replay::ReplayEngineDLL::IChunkSource&        m_base;
                std::map<uint32_t, Capture::APIDumpLoader::DataChunk> m_chunk_map;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_OVERLAY_CHUNK_SOURCE_H */
