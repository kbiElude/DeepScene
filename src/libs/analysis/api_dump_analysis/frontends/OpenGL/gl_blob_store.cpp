/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_blob_store.h"
#include "utils/hashing/hasher.h"

Analysis::APIDumpAnalysis::Frontends::GlBlobStore::GlBlobStore(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr(in_builder_ptr)
{
    /* Stub */
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GlBlobStore::get_n_blobs() const
{
    return static_cast<uint64_t>(m_resource_id_map.size() );
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
    const auto     iterator     = m_resource_id_map.find(key);
    ResourceId     result       = ResourceId::NONE;

    if (iterator != m_resource_id_map.end() )
    {
        result = iterator->second;
    }
    else
    {
        Analysis::APIDumpAnalysis::ResourceDescription description;

        description.content_hash     = content_hash;
        description.has_content_hash = true;
        description.is_application_memory = true;
        description.kind             = in_kind;
        description.size_in_bytes    = in_n_bytes;

        const std::string debug_name = (in_kind == Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX) ? "client index array"
                                                                                                          : "client vertex array";

        result = m_builder_ptr->on_create_resource(description,
                                                   debug_name);

        m_resource_id_map[key] = result;
    }

    return result;
}
