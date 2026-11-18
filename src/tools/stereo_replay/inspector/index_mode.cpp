/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/workload_index.h"
#include "index_mode.h"
#include <stdio.h>

bool Stereo::Replay::Inspector::make_index(const Options& in_options,
                                           std::string*   out_error_ptr)
{
    /* The threads that read segments ahead (a 32-bit process holds the segments in its 4 GB). */
    constexpr uint32_t N_READER_THREADS_32_BIT = 3;
    constexpr uint32_t N_READER_THREADS_64_BIT = 6;

    const uint32_t                        n_reader_threads = (sizeof(void*) == sizeof(uint64_t) ) ? N_READER_THREADS_64_BIT
                                                                                                  : N_READER_THREADS_32_BIT;
    std::string                           index_filename;
    Analysis::APIDumpIndex::WorkloadIndex index;
    bool                                  result           = false;
    bool                                  was_built        = false;

    if (Analysis::APIDumpIndex::WorkloadIndex::load_or_build(in_options.workload_filename.c_str(),
                                                             n_reader_threads,
                                                            &index,
                                                            &was_built,
                                                             out_error_ptr) == false)
    {
        goto end;
    }

    index_filename = Analysis::APIDumpIndex::WorkloadIndex::get_default_filename(in_options.workload_filename);

    fprintf(stdout,
            "%s the index %s\n"
            "  Segments: %zu\n"
            "  Calls:    %llu\n"
            "  Frames:   %llu%s\n",
            (was_built) ? "Made" : "Read",
            index_filename.c_str(),
            index.get_segment_vec().size(),
            static_cast<unsigned long long>(index.get_n_calls() ),
            static_cast<unsigned long long>(index.get_n_frames() ),
            (index.is_truncated() ) ? " (the file ends in the middle of a segment)" : "");

    result = true;

end:
    return result;
}
