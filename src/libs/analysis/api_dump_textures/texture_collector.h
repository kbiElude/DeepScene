/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Collects what the front ends report about textures (see ITextureSink) for the timeline of a .textures file: the events in the order they
 * arrive, and for each upload that carries data, the place of its content in the table of the blob writer. The content is registered as one that
 * stays in the workload.
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
            /* The collector does not own the writer. The writer must outlive the collector. */
            explicit TextureCollector(TextureBlobWriter* in_writer_ptr);

            /* The first error that occurred, or an empty string if there was none. An event that causes an error is still kept, without its content. */
            const std::string& get_error() const;

            /* The events so far, in call order. */
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
