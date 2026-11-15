/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tells which workload an analysis file belongs to without reading the workload (which is gigabytes): by its size and by the hashes of 4 KiB from
 * its start, its middle and its end. A capture is written once and never changed in place, so a file that was cut, extended, replaced or
 * captured again differs in at least one of them.
 */
#ifndef APIDUMPANALYSIS_WORKLOAD_IDENTITY_H
#define APIDUMPANALYSIS_WORKLOAD_IDENTITY_H

#include "analysis/api_dump_analysis/types.h"
#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class WorkloadIdentity
        {
        public:
            /* The number of bytes that are hashed from each piece. */
            static constexpr uint64_t N_PIECE_BYTES = 4096;

            /* For the tools that query a file of the analysis. Checks the workload (the one that the caller names, or the one that the reference
             * names if the caller names none) against the reference and prints the result on the error output. A workload that is not the one
             * the file belongs to is an error (MISMATCH, and the tool ends). A workload that cannot be read is only a warning (UNREADABLE),
             * because the queries do not need it. The noun is what the file is called in the message.
             */
            static WorkloadMatch check_for_query(const std::string&       in_filename_override,
                                                 const WorkloadReference& in_reference,
                                                 const char*              in_file_noun_ptr);

            /* True if two analyses belong to the same workload: the sizes are equal, and every hash that both references hold is equal (one file
             * of the analysis may hold fewer hashes than another). The names are ignored.
             */
            static bool is_same_workload(const WorkloadReference& in_a,
                                         const WorkloadReference& in_b);

            /* The reference of the file, with the name as given. Returns false and the error if the file cannot be read. */
            static bool read(const std::string& in_filename,
                             WorkloadReference* out_reference_ptr,
                             std::string*       out_error_ptr);

            /* Compares the file with the reference: the size, and every hash that the reference holds. The name is not compared (a workload
             * can be moved). The reason is set when the result is not MATCHES.
             */
            static WorkloadMatch verify(const std::string&       in_filename,
                                        const WorkloadReference& in_reference,
                                        std::string*             out_reason_ptr);

        private:
            /* Where the three pieces start, for a file of the given size. */
            static void get_piece_offsets(const uint64_t& in_n_bytes,
                                          uint64_t*       out_header_offset_ptr,
                                          uint64_t*       out_middle_offset_ptr,
                                          uint64_t*       out_tail_offset_ptr);

            /* The hash of the bytes of the file from the offset on, up to the size of one piece. */
            static bool hash_piece(const std::string& in_filename,
                                   const uint64_t&    in_offset,
                                   uint64_t*          out_hash_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_WORKLOAD_IDENTITY_H */
