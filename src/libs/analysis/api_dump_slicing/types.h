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
        /* What is cut out of a workload. */
        struct SliceOptions
        {
            uint64_t last_frame                 = 0; /* the slice ends with this frame (counted from 0, as the printer does)              */
            uint32_t n_reader_threads           = 2; /* which read segments ahead                                                          */
            uint32_t strip_chunks_above_n_bytes = 0; /* a data chunk of more bytes is written empty (its number stays); 0: none is        */
        };

        /* What a slice has. */
        struct SliceReport
        {
            uint64_t n_bytes_written      = 0; /* the file                                                                  */
            uint64_t n_calls_written      = 0;
            uint64_t n_frames_written     = 0; /* the frames that end in the slice                                          */
            uint32_t n_segments_copied    = 0; /* the compressed frames of the workload as they are                         */
            uint32_t n_segments_rewritten = 0; /* decoded and encoded again: the one that is cut, or all with stripped chunks */
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_TYPES_H */
