/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_textures/textures_file.h"
#include "analysis/api_dump_textures/textures_format.h"
#include "analysis/api_dump_textures/textures_serializer.h"
#include <fstream>
#include <string.h>

const Analysis::APIDumpAnalysis::AnalysisFileKind Analysis::APIDumpTextures::TexturesFile::FILE_KIND =
{
    Analysis::APIDumpTextures::TexturesFormat::ANALYSIS_VERSION,
    ".textures",
    Analysis::APIDumpTextures::TexturesFormat::FILE_VERSION,
    Analysis::APIDumpTextures::TexturesFormat::MAGIC
};

/* The sections of the file, in its order. */
const Analysis::APIDumpAnalysis::SectionInfo Analysis::APIDumpTextures::TexturesFile::SECTION_INFO_ARRAY[] =
{
    {Analysis::APIDumpTextures::TexturesFormat::TAG_META, 1},
    {Analysis::APIDumpTextures::TexturesFormat::TAG_RSRC, 1},
    {Analysis::APIDumpTextures::TexturesFormat::TAG_BLOB, 1},
    {Analysis::APIDumpTextures::TexturesFormat::TAG_EVNT, 1}
};

std::string Analysis::APIDumpTextures::TexturesFile::get_blob_filename(const std::string& in_textures_filename)
{
    return in_textures_filename + ".data";
}

bool Analysis::APIDumpTextures::TexturesFile::read(const std::string& in_filename,
                                                   TextureTimeline*   out_timeline_ptr,
                                                   std::string*       out_error_ptr)
{
    std::vector<uint8_t> byte_vec;
    bool                 result = false;

    if (Analysis::APIDumpAnalysis::SectionedFile::read_file(in_filename,
                                                           &byte_vec,
                                                            out_error_ptr) == false)
    {
        goto end;
    }

    result = read_bytes(byte_vec,
                        out_timeline_ptr,
                        out_error_ptr);

end:
    return result;
}

bool Analysis::APIDumpTextures::TexturesFile::read_bytes(const std::vector<uint8_t>& in_byte_vec,
                                                         TextureTimeline*            out_timeline_ptr,
                                                         std::string*                out_error_ptr)
{
    constexpr size_t N_SECTIONS = sizeof(SECTION_INFO_ARRAY) / sizeof(SECTION_INFO_ARRAY[0]);

    bool result = false;

    *out_timeline_ptr = TextureTimeline();

    if (Analysis::APIDumpAnalysis::SectionedFile::check_analysis_header(in_byte_vec,
                                                                        FILE_KIND,
                                                                        out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto read_section_func = [out_timeline_ptr](const uint32_t&             in_tag,
                                                          const std::vector<uint8_t>& in_raw_byte_vec)
        {
            bool is_valid = false;

            switch (in_tag)
            {
                case TexturesFormat::TAG_BLOB:
                {
                    is_valid = TexturesSerializer::deserialize_blobs(in_raw_byte_vec,
                                                                     out_timeline_ptr);

                    break;
                }

                case TexturesFormat::TAG_EVNT:
                {
                    is_valid = TexturesSerializer::deserialize_events(in_raw_byte_vec,
                                                                      out_timeline_ptr);

                    break;
                }

                case TexturesFormat::TAG_META:
                {
                    /* The first of them: it starts the timeline, which the others add to. */
                    is_valid = TexturesSerializer::deserialize_meta(in_raw_byte_vec,
                                                                    out_timeline_ptr);

                    break;
                }

                case TexturesFormat::TAG_RSRC:
                {
                    is_valid = TexturesSerializer::deserialize_resources(in_raw_byte_vec,
                                                                         out_timeline_ptr);

                    break;
                }

                default:
                {
                    break;
                }
            }

            return is_valid;
        };

        result = Analysis::APIDumpAnalysis::SectionedFile::read_sections(in_byte_vec,
                                                                         sizeof(Analysis::APIDumpAnalysis::AnalysisFileHeader),
                                                                         SECTION_INFO_ARRAY,
                                                                         N_SECTIONS,
                                                                         read_section_func,
                                                                         out_error_ptr);
    }

end:
    return result;
}

bool Analysis::APIDumpTextures::TexturesFile::write(const std::string&     in_filename,
                                                    const TextureTimeline& in_timeline,
                                                    std::string*           out_error_ptr)
{
    const std::vector<uint8_t> byte_vec = write_bytes(in_timeline);
    bool                       result   = false;

    if (byte_vec.empty() )
    {
        *out_error_ptr = "The .textures file cannot be made";

        goto end;
    }

    result = Analysis::APIDumpAnalysis::SectionedFile::write_file(in_filename,
                                                                  byte_vec,
                                                                  out_error_ptr);

end:
    return result;
}

std::vector<uint8_t> Analysis::APIDumpTextures::TexturesFile::write_bytes(const TextureTimeline& in_timeline)
{
    std::vector<uint8_t> result_vec = Analysis::APIDumpAnalysis::SectionedFile::make_analysis_header_bytes(FILE_KIND);

    for (const auto& section_info : SECTION_INFO_ARRAY)
    {
        std::vector<uint8_t> raw_byte_vec;

        switch (section_info.tag)
        {
            case TexturesFormat::TAG_BLOB:
            {
                raw_byte_vec = TexturesSerializer::serialize_blobs(in_timeline);

                break;
            }

            case TexturesFormat::TAG_EVNT:
            {
                raw_byte_vec = TexturesSerializer::serialize_events(in_timeline);

                break;
            }

            case TexturesFormat::TAG_META:
            {
                raw_byte_vec = TexturesSerializer::serialize_meta(in_timeline);

                break;
            }

            case TexturesFormat::TAG_RSRC:
            {
                raw_byte_vec = TexturesSerializer::serialize_resources(in_timeline);

                break;
            }

            default:
            {
                break;
            }
        }

        const std::vector<uint8_t> section_byte_vec = Analysis::APIDumpAnalysis::SectionedFile::pack_section(section_info,
                                                                                                             raw_byte_vec);

        if (section_byte_vec.empty() )
        {
            result_vec.clear();

            break;
        }

        result_vec.insert(result_vec.end(),
                          section_byte_vec.begin(),
                          section_byte_vec.end() );
    }

    return result_vec;
}
