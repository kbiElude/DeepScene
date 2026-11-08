/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Keeps what the front ends tell of the textures (see ITextureSink), for the timeline of a .textures file: the events in the order they came in, and for each
 * upload that has data the place of its content in the table of the writer of blobs, as a content that is left where it is in the workload.
 */
#ifndef APIDUMPTEXTURES_TEXTURE_COLLECTOR_H
#define APIDUMPTEXTURES_TEXTURE_COLLECTOR_H

#include "analysis/api_dump_textures/i_texture_sink.h"
#include "analysis/api_dump_textures/texture_blob_writer.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpTextures
    {
        class TextureCollector : public ITextureSink
        {
        public:
            /* The writer is not the collector's: it must outlive it. */
            explicit TextureCollector(TextureBlobWriter* in_writer_ptr);

            /* What went wrong first, if anything did (an event after it is kept all the same, without its content); empty if nothing did. */
            const std::string& get_error() const;

            /* The events so far, in the order of the calls. */
            const std::vector<TextureEvent>& get_event_vec() const;

            void on_texture_event(const TextureEvent& in_event,
                                  const uint8_t*      in_data_ptr,
                                  const size_t&       in_n_data_bytes,
                                  const uint32_t&     in_data_chunk_id) override;

        private:
            std::string               m_error;
            std::vector<TextureEvent> m_event_vec;
            TextureBlobWriter*        m_writer_ptr;
        };
    }
}

#endif /* APIDUMPTEXTURES_TEXTURE_COLLECTOR_H */
