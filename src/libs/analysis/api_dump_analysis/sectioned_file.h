/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The container of the files that the analysis writes (.passes, .deps, .textures): a header that is specific to each file, followed by
 * sections. A section is a Zstandard frame with the bytes of one part of the file, preceded by its tag, its version, its size and the hash of its bytes:
 *
 *   section := SectionHeader stored_bytes
 *
 * A reader skips the sections it does not know. The sections it does know must come in the expected order, once each, in the expected version.
 * The same bytes always produce the same file.
 */
#ifndef APIDUMPANALYSIS_SECTIONED_FILE_H
#define APIDUMPANALYSIS_SECTIONED_FILE_H

#include <functional>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* The tag of a section: four characters. */
        constexpr uint32_t make_section_tag(const char& in_a,
                                            const char& in_b,
                                            const char& in_c,
                                            const char& in_d)
        {
            return static_cast<uint32_t>(static_cast<uint8_t>(in_a))       |
                   static_cast<uint32_t>(static_cast<uint8_t>(in_b)) << 8  |
                   static_cast<uint32_t>(static_cast<uint8_t>(in_c)) << 16 |
                   static_cast<uint32_t>(static_cast<uint8_t>(in_d)) << 24;
        }

        struct SectionHeader
        {
            uint32_t tag;
            uint32_t version;
            uint64_t n_stored_bytes;
            uint64_t n_raw_bytes;
            uint64_t raw_hash;
        };

        static_assert(sizeof(SectionHeader) == 32,
                      "Unexpected padding in a section header");

        /* The header of the files whose sections store the results of an analysis (.deps, .textures): the kind of file, the version of the layout,
         * and the version of the meaning of the sections.
         */
        struct AnalysisFileHeader
        {
            char     magic_i8vec8[8];
            uint32_t version;
            uint32_t analysis_version;
        };

        static_assert(sizeof(AnalysisFileHeader) == 16,
                      "Unexpected padding in the file header");

        /* What a reader or writer of a kind of file that has an AnalysisFileHeader needs to know about it. */
        struct AnalysisFileKind
        {
            uint32_t    analysis_version;
            const char* extension_ptr;    /* with the dot, for messages: ".deps" */
            uint32_t    file_version;
            const char* magic_ptr;        /* sizeof(AnalysisFileHeader::magic_i8vec8) bytes */
        };

        /* A section that a reader knows. The file stores the known sections in the order in which they are listed. */
        struct SectionInfo
        {
            uint32_t tag;
            uint32_t version;
        };

        class SectionedFile
        {
        public:
            /* Called for each known section of the file, in order, with its bytes (decompressed and checked against the hash). Returns false
             * if the bytes are not valid.
             */
            typedef std::function<bool(const uint32_t& in_tag, const std::vector<uint8_t>& in_raw_byte_vec)> SectionFunc;

            /* Checks the AnalysisFileHeader at the start of the bytes against the expected kind of file. Returns false and the reason if it does not match. */
            static bool check_analysis_header(const std::vector<uint8_t>& in_byte_vec,
                                              const AnalysisFileKind&     in_kind,
                                              std::string*                out_error_ptr);

            /* The four characters of a tag, as a string. */
            static std::string get_tag_name(const uint32_t& in_tag);

            /* The bytes of the AnalysisFileHeader for a kind of file. */
            static std::vector<uint8_t> make_analysis_header_bytes(const AnalysisFileKind& in_kind);

            /* The header of a section followed by its compressed bytes; empty if the section cannot be made. */
            static std::vector<uint8_t> pack_section(const SectionInfo&          in_section_info,
                                                     const std::vector<uint8_t>& in_raw_byte_vec);

            /* The bytes of a file. Returns false and the reason if the file cannot be opened or read. */
            static bool read_file(const std::string&    in_filename,
                                  std::vector<uint8_t>* out_byte_vec_ptr,
                                  std::string*          out_error_ptr);

            /* Goes through the sections that follow the first in_position bytes of the file (its own header). Returns false and the error if a
             * section is missing, repeated, out of order, damaged, of an unexpected version, or invalid.
             */
            static bool read_sections(const std::vector<uint8_t>& in_byte_vec,
                                      const size_t&               in_position,
                                      const SectionInfo*          in_known_section_ptr,
                                      const size_t&               in_n_known_sections,
                                      const SectionFunc&          in_section_func,
                                      std::string*                out_error_ptr);

            /* Writes the bytes next to the file and replaces the file with them when all bytes are written, so a failure leaves the old file unchanged. */
            static bool write_file(const std::string&          in_filename,
                                   const std::vector<uint8_t>& in_byte_vec,
                                   std::string*                out_error_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_SECTIONED_FILE_H */
