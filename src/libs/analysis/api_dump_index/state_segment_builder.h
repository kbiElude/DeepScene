/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Makes the state frame of a segment (see WorkloadIndex): the calls a replay of unneeded frames still makes, in the layout of the workload file.
 * Chunks and surfaces keep their places (unused ones are empty), so numbering after the segment is unchanged.
 */
#ifndef APIDUMPINDEX_STATE_SEGMENT_BUILDER_H
#define APIDUMPINDEX_STATE_SEGMENT_BUILDER_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpIndex
    {
        class StateSegmentBuilder
        {
        public:
            /* Makes the calls that the indices say (of the calls of the segment, in this order, one may come again) and keeps the chunks they use. False if the
             * frame cannot be made.
             */
            static bool build(const Capture::APIDumpLoader::Workload&            in_segment,
                              const std::vector<size_t>& in_call_index_vec,
                              std::vector<uint8_t>*      out_frame_byte_vec_ptr);

        private:
                        static void append_call(const Capture::APIDumpLoader::DumpedAPICall&  in_call,
                                    std::vector<uint8_t>* inout_byte_vec_ptr);

            static void append_u32(const uint32_t&       in_value,
                                   std::vector<uint8_t>* inout_byte_vec_ptr);
        };
    }
}

#endif /* APIDUMPINDEX_STATE_SEGMENT_BUILDER_H */
