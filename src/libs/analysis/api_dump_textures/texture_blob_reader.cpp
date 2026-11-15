/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_textures/texture_blob_codec.h"
#include "analysis/api_dump_textures/texture_blob_reader.h"
#include "analysis/api_dump_textures/textures_format.h"
#include <string.h>

Analysis::APIDumpTextures::TextureBlobReader::TextureBlobReader()
    :m_n_bytes(0)
{
    /* Stub */
}

uint64_t Analysis::APIDumpTextures::TextureBlobReader::get_n_bytes() const
{
    return m_n_bytes;
}

bool Analysis::APIDumpTextures::TextureBlobReader::open(const std::string& in_filename,
                                                        std::string*       out_error_ptr)
{
    TexturesFormat::BlobFileHeader header = {};
    bool                           result = false;

    m_file.close();
    m_file.clear();

    m_n_bytes = 0;

    m_file.open(in_filename,
                std::ios::binary | std::ios::ate);

    if (m_file.is_open() == false)
    {
        *out_error_ptr = "Cannot open " + in_filename;

        goto end;
    }

    m_n_bytes = static_cast<uint64_t>(m_file.tellg() );

    if (m_n_bytes < sizeof(header) )
    {
        *out_error_ptr = in_filename + " is too short to be a file of blobs";

        goto end;
    }

    m_file.seekg(0);
    m_file.read (reinterpret_cast<char*>(&header),
                 sizeof(header) );

    if (m_file.good() == false)
    {
        *out_error_ptr = "Cannot read " + in_filename;

        goto end;
    }

    if (::memcmp(header.magic_i8vec8,
                 TexturesFormat::BLOB_MAGIC,
                 sizeof(TexturesFormat::BLOB_MAGIC) ) != 0)
    {
        *out_error_ptr = in_filename + " is not a file of blobs (wrong magic)";

        goto end;
    }

    if (header.version != TexturesFormat::BLOB_FILE_VERSION)
    {
        *out_error_ptr = "Unsupported version " + std::to_string(header.version) + " of the file of blobs";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::APIDumpTextures::TextureBlobReader::read(const BlobEntry&      in_entry,
                                                        std::vector<uint8_t>* out_raw_byte_vec_ptr,
                                                        std::string*          out_error_ptr)
{
    std::vector<uint8_t> stored_byte_vec;
    bool                 result = false;

    out_raw_byte_vec_ptr->clear();

    /* A content that stays in the workload is not in this file. The reader of the workload has to provide its bytes. */
    if (in_entry.location != BlobLocation::STORED)
    {
        *out_error_ptr = "A content is in the workload, not in the file of blobs";

        goto end;
    }

    if (m_file.is_open() == false)
    {
        *out_error_ptr = "The file of blobs is not open";

        goto end;
    }

    /* An entry that points outside the file (the file is truncated, or belongs to another workload) is refused before anything is read. */
    if (in_entry.file_offset_n_bytes <  sizeof(TexturesFormat::BlobFileHeader)               ||
        in_entry.file_offset_n_bytes >  m_n_bytes                                            ||
        in_entry.n_stored_bytes      >  m_n_bytes - in_entry.file_offset_n_bytes)
    {
        *out_error_ptr = "A content is not in the file of blobs";

        goto end;
    }

    stored_byte_vec.resize(static_cast<size_t>(in_entry.n_stored_bytes) );

    m_file.clear();
    m_file.seekg(static_cast<std::streamoff>(in_entry.file_offset_n_bytes) );
    m_file.read (reinterpret_cast<char*>(stored_byte_vec.data() ),
                 static_cast<std::streamsize>(stored_byte_vec.size() ));

    if (m_file.good() == false)
    {
        *out_error_ptr = "Cannot read the file of blobs";

        goto end;
    }

    result = TextureBlobCodec::unpack(stored_byte_vec.data(),
                                      stored_byte_vec.size(),
                                      in_entry,
                                      out_raw_byte_vec_ptr,
                                      out_error_ptr);

end:
    return result;
}
