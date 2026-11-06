/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The container of the files that the analysis makes (.passes, .deps): a header which each file has of its own, and then sections, each a
 * Zstandard frame of the bytes of a part of the file, with its tag, its version, its size and the hash of its bytes in front of it:
 *
 *   section := SectionHeader stored_bytes
 *
 * A reader skips the sections it does not know; the ones it does know must come in the order it expects, once each, in the version it expects.
 * The same bytes always give the same file.
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

        /* A section which a reader knows. The file holds them in the order they are listed in. */
        struct SectionInfo
        {
            uint32_t tag;
            uint32_t version;
        };

        class SectionedFile
        {
        public:
            /* Called for each known section of the file, in order, with its bytes (decompressed, and checked against the hash). False says that the
             * bytes are not valid.
             */
            typedef std::function<bool(const uint32_t& in_tag, const std::vector<uint8_t>& in_raw_byte_vec)> SectionFunc;

            /* The four characters of a tag. */
            static std::string get_tag_name(const uint32_t& in_tag);

            /* The header of a section and its compressed bytes; empty if the section cannot be made. */
            static std::vector<uint8_t> pack_section(const SectionInfo&          in_section_info,
                                                     const std::vector<uint8_t>& in_raw_byte_vec);

            /* Goes through the sections which follow the first in_position bytes of the file (those of its own header). False, and the error, if a
             * section is missing, repeated, out of order, damaged, of a version which is not the one that is known, or is not valid.
             */
            static bool read_sections(const std::vector<uint8_t>& in_byte_vec,
                                      const size_t&               in_position,
                                      const SectionInfo*          in_known_section_ptr,
                                      const size_t&               in_n_known_sections,
                                      const SectionFunc&          in_section_func,
                                      std::string*                out_error_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_SECTIONED_FILE_H */
