/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tells which workload an analysis file is of, without reading the workload (which is gigabytes): its size, and the hashes of
 * 4 KiB from its start, its middle and its end. A capture is written once and never changed in place, so a file that was cut,
 * extended, replaced or captured again differs in at least one of them.
 */
#ifndef APIDUMPANALYSIS_WORKLOAD_IDENTITY_H
#define APIDUMPANALYSIS_WORKLOAD_IDENTITY_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class WorkloadIdentity
        {
        public:
            /* The bytes that are hashed from each piece. */
            static constexpr uint64_t N_PIECE_BYTES = 4096;

            /* True if two analyses are of the same workload: the size is the same, and so is each hash that both of them hold (a file of the
             * analysis may hold fewer than another). The names do not matter.
             */
            static bool is_same_workload(const WorkloadReference& in_a,
                                         const WorkloadReference& in_b);

            /* The reference of the file, with the name as given. False, and the error, if the file cannot be read. */
            static bool read(const std::string& in_filename,
                             WorkloadReference* out_reference_ptr,
                             std::string*       out_error_ptr);

            /* Compares the file with the reference: the size, and every hash which the reference holds. The name is not compared (a
             * workload can be moved). The reason is set when the result is not MATCHES.
             */
            static WorkloadMatch verify(const std::string&       in_filename,
                                        const WorkloadReference& in_reference,
                                        std::string*             out_reason_ptr);

        private:
            /* Where the three pieces start, for a file of the size. */
            static void get_piece_offsets(const uint64_t& in_n_bytes,
                                          uint64_t*       out_header_offset_ptr,
                                          uint64_t*       out_middle_offset_ptr,
                                          uint64_t*       out_tail_offset_ptr);

            /* The hash of the bytes of the file from the offset on, up to the size of a piece. */
            static bool hash_piece(const std::string& in_filename,
                                   const uint64_t&    in_offset,
                                   uint64_t*          out_hash_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_WORKLOAD_IDENTITY_H */
