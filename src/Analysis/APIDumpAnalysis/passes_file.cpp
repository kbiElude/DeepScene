/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/hasher.h"
#include "Analysis/APIDumpAnalysis/passes_file.h"
#include "Analysis/APIDumpAnalysis/passes_format.h"
#include "Analysis/APIDumpAnalysis/passes_serializer.h"
#include <fstream>
#include <future>
#include <stdio.h>
#include <string.h>
#include <zstd.h>

const Analysis::APIDumpAnalysis::PassesFile::SectionInfo Analysis::APIDumpAnalysis::PassesFile::SECTION_INFO_ARRAY[] =
{
    {PassesFormat::TAG_META, 2},
    {PassesFormat::TAG_RSRC, 1},
    {PassesFormat::TAG_DRAW, 1},
    {PassesFormat::TAG_PASS, 2},
    {PassesFormat::TAG_SHPE, 1},
    {PassesFormat::TAG_FRAM, 1},
    {PassesFormat::TAG_RACC, 1},
    {PassesFormat::TAG_REPR, 1},
    {PassesFormat::TAG_SERI, 1},
    {PassesFormat::TAG_DEPT, 2},
    {PassesFormat::TAG_SURF, 1},
    {PassesFormat::TAG_COVR, 1}
};

const size_t Analysis::APIDumpAnalysis::PassesFile::N_SECTIONS = sizeof(SECTION_INFO_ARRAY) / sizeof(SECTION_INFO_ARRAY[0]);


void Analysis::APIDumpAnalysis::PassesFile::append_bytes(const void*           in_data_ptr,
                                                         const size_t&         in_n_bytes,
                                                         std::vector<uint8_t>* inout_byte_vec_ptr)
{
    const uint8_t* data_u8_ptr = static_cast<const uint8_t*>(in_data_ptr);

    inout_byte_vec_ptr->insert(inout_byte_vec_ptr->end(),
                               data_u8_ptr,
                               data_u8_ptr + in_n_bytes);
}

std::string Analysis::APIDumpAnalysis::PassesFile::get_tag_name(const uint32_t& in_tag)
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

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesFile::pack_section(const SectionInfo&    in_section_info,
                                                                         const AnalysisResult& in_analysis)
{
    constexpr int COMPRESSION_LEVEL = 19;

    const std::vector<uint8_t>  raw_byte_vec   = serialize_section(in_section_info.tag,
                                                                   in_analysis);
    PassesFormat::SectionHeader section_header = {};

    std::vector<uint8_t> result_vec;
    std::vector<uint8_t> stored_byte_vec(::ZSTD_compressBound(raw_byte_vec.size() ));

    const size_t n_stored_bytes = ::ZSTD_compress(stored_byte_vec.data(),
                                                  stored_byte_vec.size(),
                                                  raw_byte_vec.data   (),
                                                  raw_byte_vec.size   (),
                                                  COMPRESSION_LEVEL);

    /* The buffer is as big as the worst case needs, so this does not fail unless something is very wrong. */
    if (::ZSTD_isError(n_stored_bytes) == 0)
    {
        stored_byte_vec.resize(n_stored_bytes);

        section_header.n_raw_bytes    = raw_byte_vec.size   ();
        section_header.n_stored_bytes = stored_byte_vec.size();
        section_header.raw_hash       = Hasher::hash_bytes  (raw_byte_vec.data(),
                                                             raw_byte_vec.size() );
        section_header.tag            = in_section_info.tag;
        section_header.version        = in_section_info.version;

        append_bytes(&section_header,
                      sizeof(section_header),
                     &result_vec);
        append_bytes( stored_byte_vec.data(),
                      stored_byte_vec.size(),
                     &result_vec);
    }

    return result_vec;
}

bool Analysis::APIDumpAnalysis::PassesFile::read(const std::string& in_filename,
                                                 AnalysisResult*    out_analysis_ptr,
                                                 std::string*       out_error_ptr)
{
    std::vector<uint8_t> byte_vec;
    std::ifstream        file(in_filename,
                              std::ios::binary | std::ios::ate);
    bool                 result = false;

    if (file.is_open() == false)
    {
        *out_error_ptr = "Cannot open " + in_filename;

        goto end;
    }

    byte_vec.resize(static_cast<size_t>(file.tellg() ));

    file.seekg(0);
    file.read (reinterpret_cast<char*>     (byte_vec.data() ),
               static_cast<std::streamsize>(byte_vec.size() ));

    if (file.good() == false)
    {
        *out_error_ptr = "Cannot read " + in_filename;

        goto end;
    }

    result = read_bytes(byte_vec,
                        out_analysis_ptr,
                        out_error_ptr);

end:
    return result;
}

bool Analysis::APIDumpAnalysis::PassesFile::read_bytes(const std::vector<uint8_t>& in_byte_vec,
                                                       AnalysisResult*             out_analysis_ptr,
                                                       std::string*                out_error_ptr)
{
    constexpr uint64_t MAX_SECTION_N_BYTES = 1ull << 30; /* cap on the claimed size of a section */

    size_t n_next_section = 0;
    size_t position       = sizeof(PassesFormat::FileHeader);
    bool   result         = false;

    *out_analysis_ptr = AnalysisResult();

    if (in_byte_vec.size() < sizeof(PassesFormat::FileHeader) )
    {
        *out_error_ptr = "The file is too short to be a .passes file";

        goto end;
    }

    {
        PassesFormat::FileHeader header;

        ::memcpy(&header,
                 in_byte_vec.data(),
                 sizeof(header) );

        if (::memcmp(header.magic_i8vec8,
                     PassesFormat::MAGIC,
                     sizeof(PassesFormat::MAGIC) ) != 0)
        {
            *out_error_ptr = "Not a .passes file (wrong magic)";

            goto end;
        }

        if (header.version != PassesFormat::FILE_VERSION)
        {
            *out_error_ptr = "Unsupported .passes file version " + std::to_string(header.version);

            goto end;
        }

        if (header.key_scheme_version != PassesFormat::KEY_SCHEME_VERSION)
        {
            *out_error_ptr = "Unsupported key scheme version " + std::to_string(header.key_scheme_version);

            goto end;
        }
    }

    while (position < in_byte_vec.size() )
    {
        PassesFormat::SectionHeader header;
        size_t                      n_known_section = N_SECTIONS;

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
                    n_section < N_SECTIONS;
                  ++n_section)
        {
            if (SECTION_INFO_ARRAY[n_section].tag == header.tag)
            {
                n_known_section = n_section;
            }
        }

        /* Skip sections this reader does not know. */
        if (n_known_section == N_SECTIONS)
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
            *out_error_ptr = "Section " + get_tag_name(SECTION_INFO_ARRAY[n_next_section].tag) + " is missing";

            goto end;
        }

        if (header.version != SECTION_INFO_ARRAY[n_known_section].version)
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

        std::vector<uint8_t> raw_byte_vec(static_cast<size_t>(header.n_raw_bytes) );
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

        if (Hasher::hash_bytes(raw_byte_vec.data(),
                               raw_byte_vec.size() ) != header.raw_hash)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " is damaged (hash mismatch)";

            goto end;
        }

        if (read_section_bytes(header.tag,
                               raw_byte_vec,
                               out_analysis_ptr) == false)
        {
            *out_error_ptr = "Section " + get_tag_name(header.tag) + " is invalid";

            goto end;
        }

        ++n_next_section;
    }

    if (n_next_section != N_SECTIONS)
    {
        *out_error_ptr = "Section " + get_tag_name(SECTION_INFO_ARRAY[n_next_section].tag) + " is missing";

        goto end;
    }

    result = true;

end:
    return result;
}

bool Analysis::APIDumpAnalysis::PassesFile::read_section_bytes(const uint32_t&             in_tag,
                                                               const std::vector<uint8_t>& in_raw_byte_vec,
                                                               AnalysisResult*             inout_analysis_ptr)
{
    bool result = false;

    switch (in_tag)
    {
        case PassesFormat::TAG_DEPT:
        {
            result = PassesSerializer::deserialize_depths(in_raw_byte_vec,
                                                          &Model::depth_histogram_map,
                                                          &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_SURF:
        {
            result = PassesSerializer::deserialize_depths(in_raw_byte_vec,
                                                          &Model::surface_depth_histogram_map,
                                                          &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_COVR:
        {
            result = PassesSerializer::deserialize_covering(in_raw_byte_vec,
                                                            &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_DRAW:
        {
            result = PassesSerializer::deserialize_draws(in_raw_byte_vec,
                                                        &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_FRAM:
        {
            result = PassesSerializer::deserialize_frames(in_raw_byte_vec,
                                                         &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_META:
        {
            result = PassesSerializer::deserialize_meta(in_raw_byte_vec,
                                                        inout_analysis_ptr);

            break;
        }

        case PassesFormat::TAG_PASS:
        {
            result = PassesSerializer::deserialize_passes(in_raw_byte_vec,
                                                         &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_RACC:
        {
            result = PassesSerializer::deserialize_resource_access(in_raw_byte_vec,
                                                                  &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_REPR:
        {
            result = PassesSerializer::deserialize_representatives(in_raw_byte_vec,
                                                                  &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_RSRC:
        {
            result = PassesSerializer::deserialize_resources(in_raw_byte_vec,
                                                            &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_SERI:
        {
            result = PassesSerializer::deserialize_series(in_raw_byte_vec,
                                                         &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_SHPE:
        {
            result = PassesSerializer::deserialize_shapes(in_raw_byte_vec,
                                                         &inout_analysis_ptr->model);

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesFile::serialize_section(const uint32_t&       in_tag,
                                                                              const AnalysisResult& in_analysis)
{
    std::vector<uint8_t> result_vec;

    switch (in_tag)
    {
        case PassesFormat::TAG_DEPT:
        {
            result_vec = PassesSerializer::serialize_depths(in_analysis.model,
                                                            &Model::depth_histogram_map);

            break;
        }

        case PassesFormat::TAG_SURF:
        {
            result_vec = PassesSerializer::serialize_depths(in_analysis.model,
                                                            &Model::surface_depth_histogram_map);

            break;
        }

        case PassesFormat::TAG_COVR:
        {
            result_vec = PassesSerializer::serialize_covering(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_DRAW:
        {
            result_vec = PassesSerializer::serialize_draws(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_FRAM:
        {
            result_vec = PassesSerializer::serialize_frames(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_META:
        {
            result_vec = PassesSerializer::serialize_meta(in_analysis);

            break;
        }

        case PassesFormat::TAG_PASS:
        {
            result_vec = PassesSerializer::serialize_passes(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_RACC:
        {
            result_vec = PassesSerializer::serialize_resource_access(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_REPR:
        {
            result_vec = PassesSerializer::serialize_representatives(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_RSRC:
        {
            result_vec = PassesSerializer::serialize_resources(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_SERI:
        {
            result_vec = PassesSerializer::serialize_series(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_SHPE:
        {
            result_vec = PassesSerializer::serialize_shapes(in_analysis.model);

            break;
        }

        default:
        {
            break;
        }
    }

    return result_vec;
}

bool Analysis::APIDumpAnalysis::PassesFile::write(const std::string&    in_filename,
                                                  const AnalysisResult& in_analysis,
                                                  std::string*          out_error_ptr)
{
    /* A file is written beside the real one, and takes its place when it is complete: a failure leaves the old file as it was. */
    const std::string          temporary_filename = in_filename + ".tmp";
    const std::vector<uint8_t> byte_vec           = write_bytes(in_analysis);
    bool                       result             = false;

    std::ofstream file(temporary_filename,
                       std::ios::binary | std::ios::trunc);

    if (byte_vec.empty() )
    {
        *out_error_ptr = "The analysis cannot be compressed";

        goto end;
    }

    if (file.is_open() == false)
    {
        *out_error_ptr = "Cannot create " + temporary_filename;

        goto end;
    }

    file.write(reinterpret_cast<const char*>(byte_vec.data() ),
               static_cast<std::streamsize> (byte_vec.size() ));
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

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesFile::write_bytes(const AnalysisResult& in_analysis)
{
    PassesFormat::FileHeader                         file_header = {};
    std::vector<std::future<std::vector<uint8_t> > > future_vec;
    std::vector<uint8_t>                             result_vec;

    ::memcpy(file_header.magic_i8vec8,
             PassesFormat::MAGIC,
             sizeof(PassesFormat::MAGIC) );

    file_header.key_scheme_version = PassesFormat::KEY_SCHEME_VERSION;
    file_header.version            = PassesFormat::FILE_VERSION;

    append_bytes(&file_header,
                  sizeof(file_header),
                 &result_vec);

    /* The sections are independent, and compressing them is what takes the time: they are made together, and put in order. */
    for (const auto& section_info : SECTION_INFO_ARRAY)
    {
        future_vec.push_back(std::async(std::launch::async,
                                        [&section_info, &in_analysis]
                                        {
                                            return pack_section(section_info,
                                                                in_analysis);
                                        }) );
    }

    for (auto& future : future_vec)
    {
        const std::vector<uint8_t> section_byte_vec = future.get();

        if (section_byte_vec.empty() )
        {
            result_vec.clear();
        }
        else
        if (result_vec.empty() == false)
        {
            append_bytes(section_byte_vec.data(),
                         section_byte_vec.size(),
                        &result_vec);
        }
    }

    return result_vec;
}
