/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_blob_store.h"
#include "utils/hashing/hasher.h"
#include <string.h>

Analysis::APIDumpAnalysis::Frontends::GlBlobStore::GlBlobStore(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr   (in_builder_ptr),
     m_frame_index   (0),
     m_n_cached_bytes(0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::GlBlobStore::cache_bytes(const void*   in_data_ptr,
                                                                    const size_t& in_n_bytes,
                                                                    GlBlob*       inout_blob_ptr)
{
    const uint8_t* bytes_ptr = static_cast<const uint8_t*>(in_data_ptr);

    inout_blob_ptr->data_ptr = std::make_shared<const std::vector<uint8_t> >(bytes_ptr,
                                                                             bytes_ptr + in_n_bytes);

    m_n_cached_bytes += in_n_bytes;
}

std::shared_ptr<const std::vector<uint8_t> > Analysis::APIDumpAnalysis::Frontends::GlBlobStore::get_data_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_resource_id) const
{
    std::shared_ptr<const std::vector<uint8_t> > result_ptr;
    const auto                                   key_iterator = m_key_by_resource_id_map.find(in_resource_id);

    if (key_iterator != m_key_by_resource_id_map.end() )
    {
        result_ptr = m_blob_map.at(key_iterator->second).data_ptr;
    }

    return result_ptr;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GlBlobStore::get_n_blobs() const
{
    return static_cast<uint64_t>(m_blob_map.size() );
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GlBlobStore::get_n_cached_bytes() const
{
    return m_n_cached_bytes;
}

Analysis::APIDumpAnalysis::ResourceId Analysis::APIDumpAnalysis::Frontends::GlBlobStore::get_resource_id(const Analysis::APIDumpAnalysis::ResourceKind& in_kind,
                                                                                                         const void*                                    in_data_ptr,
                                                                                                         const size_t&                                  in_n_bytes)
{
    const uint64_t content_hash = Utils::Hashing::Hasher::hash_bytes(in_data_ptr,
                                                                     in_n_bytes);
    const BlobKey  key          = BlobKey(static_cast<uint32_t>(in_kind),
                                          content_hash,
                                          static_cast<uint64_t>(in_n_bytes) );
    const auto     iterator     = m_blob_map.find(key);
    GlBlob*        blob_ptr     = nullptr;

    if (iterator == m_blob_map.end() )
    {
        Analysis::APIDumpAnalysis::ResourceDescription description;

        description.content_hash          = content_hash;
        description.has_content_hash      = true;
        description.is_application_memory = true;
        description.kind                  = in_kind;
        description.size_in_bytes         = in_n_bytes;

        const std::string debug_name = (in_kind == Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX) ? "client index array"
                                                                                                          : "client vertex array";

        blob_ptr = &m_blob_map[key];

        blob_ptr->last_frame  = m_frame_index;
        blob_ptr->n_frames    = 1;
        blob_ptr->resource_id = m_builder_ptr->on_create_resource(description,
                                                                  debug_name);

        m_frame_blob_ptr_vec.push_back(blob_ptr);

        m_key_by_resource_id_map[blob_ptr->resource_id] = key;
    }
    else
    {
        blob_ptr = &iterator->second;

        /* The first time that it is seen in this frame. */
        if (blob_ptr->last_frame != m_frame_index)
        {
            blob_ptr->last_frame = m_frame_index;

            ++blob_ptr->n_frames;

            m_frame_blob_ptr_vec.push_back(blob_ptr);
        }
    }

    if (blob_ptr->data_ptr == nullptr)
    {
        cache_bytes(in_data_ptr,
                    in_n_bytes,
                    blob_ptr);
    }

    return blob_ptr->resource_id;
}

Analysis::APIDumpAnalysis::Frontends::GlBlobReuse Analysis::APIDumpAnalysis::Frontends::GlBlobStore::get_reuse(const Analysis::APIDumpAnalysis::ResourceId& in_resource_id) const
{
    constexpr uint32_t N_FRAMES_OF_REUSE = 2;

    GlBlobReuse result       = GlBlobReuse::ONE_FRAME;
    const auto  key_iterator = m_key_by_resource_id_map.find(in_resource_id);

    if (key_iterator != m_key_by_resource_id_map.end() )
    {
        const uint32_t n_frames = m_blob_map.at(key_iterator->second).n_frames;

        if (n_frames >= N_FRAMES_OF_REUSE)
        {
            result = GlBlobReuse::ACROSS_FRAMES;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::GlBlobStore::on_frame_ended()
{
    for (GlBlob* blob_ptr : m_frame_blob_ptr_vec)
    {
        const GlBlobReuse reuse = get_reuse(blob_ptr->resource_id);

        if (reuse == GlBlobReuse::ONE_FRAME &&
            blob_ptr->data_ptr != nullptr)
        {
            m_n_cached_bytes -= blob_ptr->data_ptr->size();

            blob_ptr->data_ptr.reset();
        }
    }

    m_frame_blob_ptr_vec.clear();

    ++m_frame_index;
}
