/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/sectioned_file.h"
#include "utils/hashing/stable_hasher.h"
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <zstd.h>

bool Analysis::APIDumpAnalysis::SectionedFile::check_analysis_header(const std::vector<uint8_t>& in_byte_vec,
                                                                     const AnalysisFileKind&     in_kind,
                                                                     std::string*                out_error_ptr)
{
    const std::string extension = in_kind.extension_ptr;
    bool              result    = false;

    if (in_byte_vec.size() < sizeof(AnalysisFileHeader) )
    {
        *out_error_ptr = "The file is too short to be a " +
                         extension                        +
                         " file";

        goto end;
    }

    {
        AnalysisFileHeader header;

        ::memcpy(&header,
                 in_byte_vec.data(),
                 sizeof(header) );

        if (::memcmp(header.magic_i8vec8,
                     in_kind.magic_ptr,
                     sizeof(header.magic_i8vec8) ) != 0)
        {
            *out_error_ptr = "Not a "             +
                             extension            +
                             " file (wrong magic)";

            goto end;
        }

        if (header.version != in_kind.file_version)
        {
            *out_error_ptr = "Unsupported "                +
                             extension                     +
                             " file version "              +
                             std::to_string(header.version);

            goto end;
        }

        if (header.analysis_version != in_kind.analysis_version)
        {
            *out_error_ptr = "The "                                                +
                             extension                                             +
                             " file was made by another version of the analysis (" +
                             std::to_string(header.analysis_version)               +
                             ", this is "                                          +
                             std::to_string(in_kind.analysis_version)              +
                             "): make it again";

            goto end;
        }
    }

    result = true;

end:
    return result;
}

std::string Analysis::APIDumpAnalysis::SectionedFile::get_tag_name(const uint32_t& in_tag)
{
    constexpr size_t N_BITS_PER_BYTE = 8;

    std::string result;

    for (size_t n_byte = 0;
                n_byte < sizeof(in_tag);
              ++n_byte)
    {
        result.push_back(static_cast<char>(in_tag >> (n_byte * N_BITS_PER_BYTE) ));
    }

    return result;
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::SectionedFile::make_analysis_header_bytes(const AnalysisFileKind& in_kind)
{
    AnalysisFileHeader   file_header = {};
    std::vector<uint8_t> result_vec(sizeof(file_header) );

    ::memcpy(file_header.magic_i8vec8,
             in_kind.magic_ptr,
             sizeof(file_header.magic_i8vec8) );

    file_header.analysis_version = in_kind.analysis_version;
    file_header.version          = in_kind.file_version;

    ::memcpy(result_vec.data(),
             &file_header,
             sizeof(file_header) );

    return result_vec;
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::SectionedFile::pack_section(const SectionInfo&          in_section_info,
                                                                            const std::vector<uint8_t>& in_raw_byte_vec)
{
    constexpr int COMPRESSION_LEVEL = 19;

    SectionHeader        section_header = {};
    std::vector<uint8_t> result_vec;
    std::vector<uint8_t> stored_byte_vec(::ZSTD_compressBound(in_raw_byte_vec.size() ));

    ::ZSTD_CCtx* const compression_context = ::ZSTD_createCCtx();

    /* A checksum of the content ends the frame, so that damage to a section is noticed even where the decompressor would not need the damaged
     * bits (a small frame has such bits).
     */
    ::ZSTD_CCtx_setParameter(compression_context,
                             ::ZSTD_c_compressionLevel,
                             COMPRESSION_LEVEL);
    ::ZSTD_CCtx_setParameter(compression_context,
                             ::ZSTD_c_checksumFlag,
                             1);

    const size_t n_stored_bytes = ::ZSTD_compress2(compression_context,
                                                   stored_byte_vec.data(),
                                                   stored_byte_vec.size(),
                                                   in_raw_byte_vec.data(),
                                                   in_raw_byte_vec.size() );

    ::ZSTD_freeCCtx(compression_context);

    /* The buffer is as large as the worst case needs, so this fails only if something is very wrong. */
    if (::ZSTD_isError(n_stored_bytes) == 0)
    {
        stored_byte_vec.resize(n_stored_bytes);

        section_header.n_raw_bytes    = in_raw_byte_vec.size();
        section_header.n_stored_bytes = stored_byte_vec.size();
        section_header.raw_hash       = Utils::Hashing::StableHasher::hash_bytes(in_raw_byte_vec.data(),
                                                                                 in_raw_byte_vec.size() );
        section_header.tag            = in_section_info.tag;
        section_header.version        = in_section_info.version;

        result_vec.resize(sizeof(section_header) + stored_byte_vec.size() );

        ::memcpy(result_vec.data(),
                 &section_header,
                 sizeof(section_header) );
        ::memcpy(result_vec.data() + sizeof(section_header),
                 stored_byte_vec.data(),
                 stored_byte_vec.size() );
    }

    return result_vec;
}

bool Analysis::APIDumpAnalysis::SectionedFile::read_file(const std::string&    in_filename,
                                                       std::vector<uint8_t>* out_byte_vec_ptr,
                                                       std::string*          out_error_ptr)
{
    std::ifstream file(in_filename,
                       std::ios::binary | std::ios::ate);
    bool          result = false;

    if (file.is_open() == false)
    {
        *out_error_ptr = "Cannot open " + in_filename;

        goto end;
    }

    out_byte_vec_ptr->resize(static_cast<size_t>(file.tellg() ));

    file.seekg(0);
    file.read (reinterpret_cast<char*>     (out_byte_vec_ptr->data() ),
               static_cast<std::streamsize>(out_byte_vec_ptr->size() ));

    if (file.good() == false)
    {
        *out_error_ptr = "Cannot read " + in_filename;

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::APIDumpAnalysis::SectionedFile::read_sections(const std::vector<uint8_t>& in_byte_vec,
                                                             const size_t&               in_position,
                                                             const SectionInfo*          in_known_section_ptr,
                                                             const size_t&               in_n_known_sections,
                                                             const SectionFunc&          in_section_func,
                                                             std::string*                out_error_ptr)
{
    constexpr uint64_t MAX_SECTION_N_BYTES = 1ull << 30; /* the largest size that a section may claim */

    size_t n_next_section = 0;
    size_t position       = in_position;
    bool   result         = false;

    while (position < in_byte_vec.size() )
    {
        SectionHeader header;
        size_t        n_known_section = in_n_known_sections;

        if (in_byte_vec.size() - position < sizeof(header) )
        {
            *out_error_ptr = "The file ends in the middle of a section header";

            goto end;
        }

        ::memcpy(&header,
                 in_byte_vec.data() + position,
                 sizeof(header) );

        position += sizeof(header);

        if (header.n_stored_bytes > in_byte_vec.size() - position)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " is truncated";

            goto end;
        }

        const uint8_t* stored_ptr = in_byte_vec.data() + position;

        position += static_cast<size_t>(header.n_stored_bytes);

        for (size_t n_section = 0;
                    n_section < in_n_known_sections;
                  ++n_section)
        {
            if (in_known_section_ptr[n_section].tag == header.tag)
            {
                n_known_section = n_section;
            }
        }

        /* Skip sections this reader does not know. */
        if (n_known_section == in_n_known_sections)
        {
            continue;
        }

        if (n_known_section < n_next_section)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " is repeated or out of order";

            goto end;
        }

        if (n_known_section > n_next_section)
        {
            *out_error_ptr = "Section " + get_tag_name(in_known_section_ptr[n_next_section].tag) + " is missing";

            goto end;
        }

        if (header.version != in_known_section_ptr[n_known_section].version)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " has an unsupported version " + std::to_string(header.version);

            goto end;
        }

        /* The size is also stored in the compressed frame. A claimed size that differs from it is refused before any memory is allocated for it. */
        bool is_size_valid = (header.n_raw_bytes <= MAX_SECTION_N_BYTES);

        if (is_size_valid)
        {
            const auto frame_n_bytes = ::ZSTD_getFrameContentSize(stored_ptr,
                                                                  static_cast<size_t>(header.n_stored_bytes) );

            is_size_valid = (frame_n_bytes == header.n_raw_bytes);
        }

        if (is_size_valid == false)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " claims an impossible size";

            goto end;
        }

        std::vector<uint8_t> raw_byte_vec(static_cast<size_t>(header.n_raw_bytes));
        const size_t         n_decompressed = ::ZSTD_decompress(raw_byte_vec.data(),
                                                                raw_byte_vec.size(),
                                                                stored_ptr,
                                                                static_cast<size_t>(header.n_stored_bytes) );

        const bool is_error = (::ZSTD_isError(n_decompressed) != 0);

        if (is_error                              ||
            n_decompressed != raw_byte_vec.size() )
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " cannot be decompressed";

            goto end;
        }

        if (Utils::Hashing::StableHasher::hash_bytes(raw_byte_vec.data(),
                                                     raw_byte_vec.size() ) != header.raw_hash)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " is damaged (hash mismatch)";

            goto end;
        }

        if (in_section_func(header.tag,
                            raw_byte_vec) == false)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " is invalid";

            goto end;
        }

        ++n_next_section;
    }

    if (n_next_section != in_n_known_sections)
    {
        *out_error_ptr = "Section " + get_tag_name(in_known_section_ptr[n_next_section].tag) + " is missing";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::APIDumpAnalysis::SectionedFile::write_file(const std::string&          in_filename,
                                                          const std::vector<uint8_t>& in_byte_vec,
                                                          std::string*                out_error_ptr)
{
    /* The file is written next to the real one and replaces it when complete, so a failure leaves the old file unchanged. */
    const std::string temporary_filename = in_filename + ".tmp";
    bool              result             = false;

    std::ofstream file(temporary_filename,
                       std::ios::binary | std::ios::trunc);

    if (file.is_open() == false)
    {
        *out_error_ptr = "Cannot create " + temporary_filename;

        goto end;
    }

    file.write(reinterpret_cast<const char*>(in_byte_vec.data() ),
               static_cast<std::streamsize> (in_byte_vec.size() ));
    file.close();

    if (file.fail() )
    {
        *out_error_ptr = "Cannot write " + temporary_filename;

        ::remove(temporary_filename.c_str() );

        goto end;
    }

    ::remove(in_filename.c_str() );

    if (::rename(temporary_filename.c_str(),
                 in_filename.c_str() ) != 0)
    {
        *out_error_ptr = "Cannot replace " + in_filename;

        goto end;
    }

    result = true;

end:
    return result;
}
