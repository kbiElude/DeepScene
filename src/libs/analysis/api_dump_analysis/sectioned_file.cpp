/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/sectioned_file.h"
#include "utils/hashing/hasher.h"
#include <string.h>
#include <zstd.h>

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

std::vector<uint8_t> Analysis::APIDumpAnalysis::SectionedFile::pack_section(const SectionInfo&          in_section_info,
                                                                            const std::vector<uint8_t>& in_raw_byte_vec)
{
    constexpr int COMPRESSION_LEVEL = 19;

    SectionHeader        section_header = {};
    std::vector<uint8_t> result_vec;
    std::vector<uint8_t> stored_byte_vec(::ZSTD_compressBound(in_raw_byte_vec.size() ));

    const size_t n_stored_bytes = ::ZSTD_compress(stored_byte_vec.data(),
                                                  stored_byte_vec.size(),
                                                  in_raw_byte_vec.data(),
                                                  in_raw_byte_vec.size(),
                                                  COMPRESSION_LEVEL);

    /* The buffer is as big as the worst case needs, so this does not fail unless something is very wrong. */
    if (::ZSTD_isError(n_stored_bytes) == 0)
    {
        stored_byte_vec.resize(n_stored_bytes);

        section_header.n_raw_bytes    = in_raw_byte_vec.size();
        section_header.n_stored_bytes = stored_byte_vec.size();
        section_header.raw_hash       = Utils::Hashing::Hasher::hash_bytes(in_raw_byte_vec.data(),
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

bool Analysis::APIDumpAnalysis::SectionedFile::read_sections(const std::vector<uint8_t>& in_byte_vec,
                                                             const size_t&               in_position,
                                                             const SectionInfo*          in_known_section_ptr,
                                                             const size_t&               in_n_known_sections,
                                                             const SectionFunc&          in_section_func,
                                                             std::string*                out_error_ptr)
{
    constexpr uint64_t MAX_SECTION_N_BYTES = 1ull << 30; /* cap on the claimed size of a section */

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

        /* The size is known from the compressed data too: a claim that differs is refused before anything is allocated for it. */
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

        if (Utils::Hashing::Hasher::hash_bytes(raw_byte_vec.data(),
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
