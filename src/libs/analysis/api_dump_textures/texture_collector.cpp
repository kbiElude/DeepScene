/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_textures/texture_collector.h"

Analysis::APIDumpTextures::TextureCollector::TextureCollector(TextureBlobWriter* in_writer_ptr)
    :m_writer_ptr(in_writer_ptr)
{
    /* Stub */
}

const std::string& Analysis::APIDumpTextures::TextureCollector::get_error() const
{
    return m_error;
}

const std::vector<Analysis::APIDumpTextures::TextureEvent>& Analysis::APIDumpTextures::TextureCollector::get_event_vec() const
{
    return m_event_vec;
}

void Analysis::APIDumpTextures::TextureCollector::on_texture_event(const TextureEvent& in_event,
                                                                   const uint8_t*      in_data_ptr,
                                                                   const size_t&       in_n_data_bytes,
                                                                   const uint32_t&     in_data_chunk_id)
{
    TextureEvent event = in_event;

    event.blob_index = NO_BLOB;

    if (in_data_ptr != nullptr)
    {
        std::string error;

        if (m_writer_ptr->add_workload_reference(in_data_ptr,
                                                 in_n_data_bytes,
                                                 in_data_chunk_id,
                                                &event.blob_index,
                                                &error) == false)
        {
            event.blob_index = NO_BLOB;

            if (m_error.empty() )
            {
                m_error = error;
            }
        }
    }

    m_event_vec.push_back(event);
}
