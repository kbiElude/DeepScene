/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpDependencies/deps_file.h"
#include "Analysis/APIDumpDependencies/deps_format.h"
#include "Analysis/APIDumpDependencies/deps_serializer.h"
#include <fstream>
#include <future>
#include <string.h>

namespace
{
    /* The sections of the file, in its order. */
    const Analysis::APIDumpAnalysis::SectionInfo SECTION_INFO_ARRAY[] =
    {
        {Analysis::APIDumpDependencies::DepsFormat::TAG_META, 1},
        {Analysis::APIDumpDependencies::DepsFormat::TAG_RSRC, 1},
        {Analysis::APIDumpDependencies::DepsFormat::TAG_FCAL, 1},
        {Analysis::APIDumpDependencies::DepsFormat::TAG_RDEP, 1}
    };

    constexpr size_t N_SECTIONS = sizeof(SECTION_INFO_ARRAY) / sizeof(SECTION_INFO_ARRAY[0]);
}

bool Analysis::APIDumpDependencies::DepsFile::read(const std::string&  in_filename,
                                                   DependencyAnalysis* out_analysis_ptr,
                                                   std::string*        out_error_ptr)
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

bool Analysis::APIDumpDependencies::DepsFile::read_bytes(const std::vector<uint8_t>& in_byte_vec,
                                                         DependencyAnalysis*         out_analysis_ptr,
                                                         std::string*                out_error_ptr)
{
    bool result = false;

    *out_analysis_ptr = DependencyAnalysis();

    if (in_byte_vec.size() < sizeof(DepsFormat::FileHeader) )
    {
        *out_error_ptr = "The file is too short to be a .deps file";

        goto end;
    }

    {
        DepsFormat::FileHeader header;

        ::memcpy(&header,
                 in_byte_vec.data(),
                 sizeof(header) );

        if (::memcmp(header.magic_i8vec8,
                     DepsFormat::MAGIC,
                     sizeof(DepsFormat::MAGIC) ) != 0)
        {
            *out_error_ptr = "Not a .deps file (wrong magic)";

            goto end;
        }

        if (header.version != DepsFormat::FILE_VERSION)
        {
            *out_error_ptr = "Unsupported .deps file version " + std::to_string(header.version);

            goto end;
        }

        if (header.analysis_version != DepsFormat::ANALYSIS_VERSION)
        {
            *out_error_ptr = "The .deps file was made by another version of the analysis (" + std::to_string(header.analysis_version) +
                             ", this is " + std::to_string(DepsFormat::ANALYSIS_VERSION) + "): make it again";

            goto end;
        }
    }

    result = Analysis::APIDumpAnalysis::SectionedFile::read_sections(in_byte_vec,
                                                                     sizeof(DepsFormat::FileHeader),
                                                                     SECTION_INFO_ARRAY,
                                                                     N_SECTIONS,
                                                                     [out_analysis_ptr](const uint32_t&             in_tag,
                                                                                        const std::vector<uint8_t>& in_raw_byte_vec)
                                                                     {
                                                                         bool is_valid = false;

                                                                         switch (in_tag)
                                                                         {
                                                                             case DepsFormat::TAG_FCAL:
                                                                             {
                                                                                 is_valid = DepsSerializer::deserialize_frames(in_raw_byte_vec,
                                                                                                                               out_analysis_ptr);

                                                                                 break;
                                                                             }

                                                                             case DepsFormat::TAG_META:
                                                                             {
                                                                                 /* The first of them: it starts the analysis, which the others add to. */
                                                                                 is_valid = DepsSerializer::deserialize_meta(in_raw_byte_vec,
                                                                                                                             out_analysis_ptr);

                                                                                 break;
                                                                             }

                                                                             case DepsFormat::TAG_RDEP:
                                                                             {
                                                                                 is_valid = DepsSerializer::deserialize_dependencies(in_raw_byte_vec,
                                                                                                                                     out_analysis_ptr);

                                                                                 break;
                                                                             }

                                                                             case DepsFormat::TAG_RSRC:
                                                                             {
                                                                                 is_valid = DepsSerializer::deserialize_resources(in_raw_byte_vec,
                                                                                                                                  out_analysis_ptr);

                                                                                 break;
                                                                             }

                                                                             default:
                                                                             {
                                                                                 break;
                                                                             }
                                                                         }

                                                                         return is_valid;
                                                                     },
                                                                     out_error_ptr);

end:
    return result;
}

bool Analysis::APIDumpDependencies::DepsFile::write(const std::string&        in_filename,
                                                    const DependencyAnalysis& in_analysis,
                                                    std::string*              out_error_ptr)
{
    const std::vector<uint8_t> byte_vec = write_bytes(in_analysis);
    bool                       result   = false;

    if (byte_vec.empty() )
    {
        *out_error_ptr = "The .deps file cannot be made";

        goto end;
    }

    {
        std::ofstream file(in_filename,
                           std::ios::binary | std::ios::trunc);

        file.write(reinterpret_cast<const char*>      (byte_vec.data() ),
                   static_cast<std::streamsize>       (byte_vec.size() ));
        file.close();

        if (file.fail() )
        {
            *out_error_ptr = "Cannot write " + in_filename;

            goto end;
        }
    }

    result = true;

end:
    return result;
}

std::vector<uint8_t> Analysis::APIDumpDependencies::DepsFile::write_bytes(const DependencyAnalysis& in_analysis)
{
    DepsFormat::FileHeader                           file_header = {};
    std::vector<std::future<std::vector<uint8_t> > > future_vec;
    std::vector<uint8_t>                             result_vec;

    ::memcpy(file_header.magic_i8vec8,
             DepsFormat::MAGIC,
             sizeof(DepsFormat::MAGIC) );

    file_header.analysis_version = DepsFormat::ANALYSIS_VERSION;
    file_header.version          = DepsFormat::FILE_VERSION;

    result_vec.resize(sizeof(file_header) );

    ::memcpy(result_vec.data(),
             &file_header,
             sizeof(file_header) );

    /* The sections are independent, and compressing them is what takes the time: they are made together, and put in order. */
    for (const auto& section_info : SECTION_INFO_ARRAY)
    {
        future_vec.push_back(std::async(std::launch::async,
                                        [&section_info, &in_analysis]
                                        {
                                            std::vector<uint8_t> raw_byte_vec;

                                            switch (section_info.tag)
                                            {
                                                case DepsFormat::TAG_FCAL:
                                                {
                                                    raw_byte_vec = DepsSerializer::serialize_frames(in_analysis);

                                                    break;
                                                }

                                                case DepsFormat::TAG_META:
                                                {
                                                    raw_byte_vec = DepsSerializer::serialize_meta(in_analysis);

                                                    break;
                                                }

                                                case DepsFormat::TAG_RDEP:
                                                {
                                                    raw_byte_vec = DepsSerializer::serialize_dependencies(in_analysis);

                                                    break;
                                                }

                                                case DepsFormat::TAG_RSRC:
                                                {
                                                    raw_byte_vec = DepsSerializer::serialize_resources(in_analysis);

                                                    break;
                                                }

                                                default:
                                                {
                                                    break;
                                                }
                                            }

                                            return Analysis::APIDumpAnalysis::SectionedFile::pack_section(section_info,
                                                                                                          raw_byte_vec);
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
            result_vec.insert(result_vec.end(),
                              section_byte_vec.begin(),
                              section_byte_vec.end() );
        }
    }

    return result_vec;
}
