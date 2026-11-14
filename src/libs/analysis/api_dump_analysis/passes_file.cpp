/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/passes_file.h"
#include "analysis/api_dump_analysis/passes_format.h"
#include "analysis/api_dump_analysis/passes_serializer.h"
#include "utils/hashing/hasher.h"
#include <fstream>
#include <future>
#include <stdio.h>
#include <string.h>
#include <zstd.h>

const Analysis::APIDumpAnalysis::SectionInfo Analysis::APIDumpAnalysis::PassesFile::SECTION_INFO_ARRAY[] =
{
    {PassesFormat::TAG_META, 2},
    {PassesFormat::TAG_RSRC, 1},
    {PassesFormat::TAG_DRAW, 2},
    {PassesFormat::TAG_PASS, 2},
    {PassesFormat::TAG_SHPE, 1},
    {PassesFormat::TAG_FRAM, 1},
    {PassesFormat::TAG_RACC, 1},
    {PassesFormat::TAG_REPR, 1},
    {PassesFormat::TAG_SERI, 1},
    {PassesFormat::TAG_DEPT, 2},
    {PassesFormat::TAG_SURF, 1},
    {PassesFormat::TAG_PROJ, 1},
    {PassesFormat::TAG_COVR, 1},
    {PassesFormat::TAG_FACT, 2}
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

std::vector<uint8_t> Analysis::APIDumpAnalysis::PassesFile::pack_section(const SectionInfo&    in_section_info,
                                                                         const AnalysisResult& in_analysis)
{
    const std::vector<uint8_t> section_bytes_vec = serialize_section(in_section_info.tag,
                                                                     in_analysis);

    return SectionedFile::pack_section(in_section_info,
                                       section_bytes_vec);
}

bool Analysis::APIDumpAnalysis::PassesFile::read(const std::string& in_filename,
                                                 AnalysisResult*    out_analysis_ptr,
                                                 std::string*       out_error_ptr)
{
    std::vector<uint8_t> byte_vec;
    bool                 result = false;

    if (SectionedFile::read_file(in_filename,
                                &byte_vec,
                                 out_error_ptr) == false)
    {
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
    bool result = false;

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

    {
        const auto read_section_func = [out_analysis_ptr](const uint32_t&             in_tag,
                                                          const std::vector<uint8_t>& in_raw_byte_vec)
        {
            return read_section_bytes(in_tag,
                                      in_raw_byte_vec,
                                      out_analysis_ptr);
        };

        result = SectionedFile::read_sections(in_byte_vec,
                                              sizeof(PassesFormat::FileHeader),
                                              SECTION_INFO_ARRAY,
                                              N_SECTIONS,
                                              read_section_func,
                                              out_error_ptr);
    }

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

        case PassesFormat::TAG_PROJ:
        {
            result = PassesSerializer::deserialize_projection_scales(in_raw_byte_vec,
                                                                     &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_COVR:
        {
            result = PassesSerializer::deserialize_draw_keys(in_raw_byte_vec,
                                                             &Model::covering_draw_key_map,
                                                             &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_DRAW:
        {
            result = PassesSerializer::deserialize_draws(in_raw_byte_vec,
                                                        &inout_analysis_ptr->model);

            break;
        }

        case PassesFormat::TAG_FACT:
        {
            result = PassesSerializer::deserialize_draw_facts(in_raw_byte_vec,
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

        case PassesFormat::TAG_PROJ:
        {
            result_vec = PassesSerializer::serialize_projection_scales(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_COVR:
        {
            result_vec = PassesSerializer::serialize_draw_keys(in_analysis.model,
                                                               &Model::covering_draw_key_map);

            break;
        }

        case PassesFormat::TAG_DRAW:
        {
            result_vec = PassesSerializer::serialize_draws(in_analysis.model);

            break;
        }

        case PassesFormat::TAG_FACT:
        {
            result_vec = PassesSerializer::serialize_draw_facts(in_analysis.model);

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
    const std::vector<uint8_t> byte_vec = write_bytes(in_analysis);
    bool                       result   = false;

    if (byte_vec.empty() )
    {
        *out_error_ptr = "The analysis cannot be compressed";

        goto end;
    }

    result = SectionedFile::write_file(in_filename,
                                       byte_vec,
                                       out_error_ptr);

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
