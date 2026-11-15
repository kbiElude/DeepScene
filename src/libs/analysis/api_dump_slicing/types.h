/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the slicing of a workload.
 */
#ifndef ANALYSIS_APIDUMPSLICING_TYPES_H
#define ANALYSIS_APIDUMPSLICING_TYPES_H

#include <stdint.h>

namespace Analysis
{
    namespace APIDumpSlicing
    {
        /* The options of a slice. */
        struct SliceOptions
        {
            uint64_t last_frame                 = 0; /* the last frame of the slice, counted from 0 as the printer does                                        */
            uint32_t n_reader_threads           = 2; /* threads that read segments ahead of the writer                                                         */
            uint32_t strip_chunks_above_n_bytes = 0; /* a data chunk larger than this many bytes is written empty (its number stays); 0 strips nothing         */
        };

        /* What a slice contains. */
        struct SliceReport
        {
            uint64_t n_bytes_written      = 0; /* the size of the file                                                                                         */
            uint64_t n_calls_written      = 0;
            uint64_t n_frames_written     = 0; /* the frames that end in the slice                                                                             */
            uint32_t n_segments_copied    = 0; /* the compressed segments that were copied from the workload unchanged                                         */
            uint32_t n_segments_rewritten = 0; /* the segments that were decoded and encoded again: the one that is cut, or all of them if chunks are stripped */
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_TYPES_H */
