/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_textures/texture_blob_codec.h"
#include "analysis/api_dump_textures/texture_blob_writer.h"
#include "analysis/api_dump_textures/textures_format.h"
#include <string.h>

Analysis::APIDumpTextures::TextureBlobWriter::TextureBlobWriter()
    :m_n_bytes(0)
{
    /* Stub */
}

bool Analysis::APIDumpTextures::TextureBlobWriter::add(const void*       in_data_ptr,
                                                       const size_t&     in_n_bytes,
                                                       const BlobFilter& in_filter,
                                                       uint32_t*         out_blob_index_ptr,
                                                       std::string*      out_error_ptr)
{
    BlobEntry            entry;
    std::vector<uint8_t> stored_byte_vec;
    bool                 result = false;

    if (m_file.is_open() == false)
    {
        *out_error_ptr = "The file of blobs is not open";

        goto end;
    }

    TextureBlobCodec::compute_hashes(in_data_ptr,
                                     in_n_bytes,
                                    &entry.content_hash,
                                    &entry.length_hash);

    {
        const ContentKey key(entry.content_hash,
                             entry.length_hash,
                             static_cast<uint64_t>(in_n_bytes) );
        const auto       found_iterator = m_index_by_content_map.find(key);

        if (found_iterator != m_index_by_content_map.end() )
        {
            *out_blob_index_ptr = found_iterator->second;
            result              = true;

            goto end;
        }
    }

    if (TextureBlobCodec::pack(in_data_ptr,
                               in_n_bytes,
                               in_filter,
                              &stored_byte_vec) == false)
    {
        *out_error_ptr = "A content cannot be compressed";

        goto end;
    }

    entry.file_offset_n_bytes = m_n_bytes;
    entry.filter              = in_filter;
    entry.n_raw_bytes         = in_n_bytes;
    entry.n_stored_bytes      = stored_byte_vec.size();

    m_file.write(reinterpret_cast<const char*>(stored_byte_vec.data() ),
                 static_cast<std::streamsize> (stored_byte_vec.size() ));

    if (m_file.fail() )
    {
        *out_error_ptr = "Cannot write the file of blobs";

        goto end;
    }

    m_n_bytes += stored_byte_vec.size();

    *out_blob_index_ptr = static_cast<uint32_t>(m_blob_vec.size() );

    m_index_by_content_map[ContentKey(entry.content_hash,
                                      entry.length_hash,
                                      entry.n_raw_bytes)] = *out_blob_index_ptr;

    m_blob_vec.push_back(entry);

    result = true;

end:
    return result;
}

bool Analysis::APIDumpTextures::TextureBlobWriter::add_workload_reference(const void*     in_data_ptr,
                                                                         const size_t&   in_n_bytes,
                                                                         const uint32_t& in_data_chunk_id,
                                                                         uint32_t*       out_blob_index_ptr,
                                                                         std::string*    out_error_ptr)
{
    const auto found_iterator = m_index_by_chunk_map.find(in_data_chunk_id);
    BlobEntry  entry;
    bool       result = true;

    (void) out_error_ptr;

    if (found_iterator != m_index_by_chunk_map.end() )
    {
        *out_blob_index_ptr = found_iterator->second;

        goto end;
    }

    TextureBlobCodec::compute_hashes(in_data_ptr,
                                     in_n_bytes,
                                    &entry.content_hash,
                                    &entry.length_hash);

    entry.data_chunk_id = in_data_chunk_id;
    entry.location      = BlobLocation::WORKLOAD_CHUNK;
    entry.n_raw_bytes   = in_n_bytes;

    *out_blob_index_ptr = static_cast<uint32_t>(m_blob_vec.size() );

    m_index_by_chunk_map[in_data_chunk_id] = *out_blob_index_ptr;

    m_blob_vec.push_back(entry);

end:
    return result;
}

bool Analysis::APIDumpTextures::TextureBlobWriter::close(std::string* out_error_ptr)
{
    bool result = false;

    if (m_file.is_open() == false)
    {
        *out_error_ptr = "The file of blobs is not open";

        goto end;
    }

    m_file.close();

    if (m_file.fail() )
    {
        *out_error_ptr = "Cannot write the file of blobs";

        goto end;
    }

    result = true;

end:
    return result;
}

const std::vector<Analysis::APIDumpTextures::BlobEntry>& Analysis::APIDumpTextures::TextureBlobWriter::get_blob_vec() const
{
    return m_blob_vec;
}

uint64_t Analysis::APIDumpTextures::TextureBlobWriter::get_n_bytes() const
{
    return m_n_bytes;
}

bool Analysis::APIDumpTextures::TextureBlobWriter::open(const std::string& in_filename,
                                                        std::string*       out_error_ptr)
{
    TexturesFormat::BlobFileHeader header = {};
    bool                           result = false;

    m_blob_vec.clear();
    m_index_by_chunk_map.clear();
    m_index_by_content_map.clear();

    m_n_bytes = 0;

    m_file.open(in_filename,
                std::ios::binary | std::ios::trunc);

    if (m_file.is_open() == false)
    {
        *out_error_ptr = "Cannot open " + in_filename;

        goto end;
    }

    ::memcpy(header.magic_i8vec8,
             TexturesFormat::BLOB_MAGIC,
             sizeof(TexturesFormat::BLOB_MAGIC) );

    header.version = TexturesFormat::BLOB_FILE_VERSION;

    m_file.write(reinterpret_cast<const char*>(&header),
                 sizeof(header) );

    if (m_file.fail() )
    {
        *out_error_ptr = "Cannot write " + in_filename;

        goto end;
    }

    m_n_bytes = sizeof(header);

    result = true;

end:
    return result;
}
