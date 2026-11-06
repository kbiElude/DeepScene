/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_chunk_source.h"

Stereo::Replay::EngineDLL::OverlayChunkSource::OverlayChunkSource(const ::Replay::ReplayEngineDLL::IChunkSource& in_base)
    :m_base(in_base)
{
    /* Stub */
}

const Capture::APIDumpLoader::DataChunk* Stereo::Replay::EngineDLL::OverlayChunkSource::get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const
{
    const auto chunk_iterator = m_chunk_map.find(in_data_chunk_id);

    return (chunk_iterator != m_chunk_map.end() ) ? &chunk_iterator->second
                                                  : m_base.get_data_chunk_ptr(in_data_chunk_id);
}

uint32_t Stereo::Replay::EngineDLL::OverlayChunkSource::get_pointer_n_bytes() const
{
    return m_base.get_pointer_n_bytes();
}

void Stereo::Replay::EngineDLL::OverlayChunkSource::set_chunk(const uint32_t&                          in_data_chunk_id,
                                                              const Capture::APIDumpLoader::DataChunk& in_chunk)
{
    m_chunk_map[in_data_chunk_id] = in_chunk;
}
