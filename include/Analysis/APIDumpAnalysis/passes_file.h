/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads and writes .passes files (see passes_format.h). The same analysis always gives the same bytes.
 */
#ifndef APIDUMPANALYSIS_PASSES_FILE_H
#define APIDUMPANALYSIS_PASSES_FILE_H

#include "Analysis/APIDumpAnalysis/model.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class PassesFile
        {
        public:
            /* The error is set when the function returns false. */
            static bool read(const std::string& in_filename,
                             AnalysisResult*    out_analysis_ptr,
                             std::string*       out_error_ptr);

            static bool                 read_bytes (const std::vector<uint8_t>& in_byte_vec,
                                                    AnalysisResult*             out_analysis_ptr,
                                                    std::string*                out_error_ptr);
            static bool                 write      (const std::string&          in_filename,
                                                    const AnalysisResult&       in_analysis,
                                                    std::string*                out_error_ptr);
            static std::vector<uint8_t> write_bytes(const AnalysisResult&       in_analysis);

        private:
            /* A section this reader knows. The file holds them in the order of SECTION_INFO_ARRAY. */
            struct SectionInfo
            {
                uint32_t tag;
                uint32_t version;
            };

            static void        append_bytes(const void*           in_data_ptr,
                                            const size_t&         in_n_bytes,
                                            std::vector<uint8_t>* inout_byte_vec_ptr);
            static std::string get_tag_name(const uint32_t&       in_tag);

            /* The header of a section and its compressed bytes; empty if the section cannot be made. */
            static std::vector<uint8_t> pack_section(const SectionInfo&    in_section_info,
                                                     const AnalysisResult& in_analysis);

            static bool                 read_section_bytes(const uint32_t&             in_tag,
                                                           const std::vector<uint8_t>& in_raw_byte_vec,
                                                           AnalysisResult*             inout_analysis_ptr);
            static std::vector<uint8_t> serialize_section (const uint32_t&             in_tag,
                                                           const AnalysisResult&       in_analysis);

            static const size_t      N_SECTIONS;
            static const SectionInfo SECTION_INFO_ARRAY[];
        };
    }
}

#endif /* APIDUMPANALYSIS_PASSES_FILE_H */
