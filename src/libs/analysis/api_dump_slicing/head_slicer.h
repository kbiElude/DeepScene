/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Writes the first frames of a workload, frames 0 to B, as a workload of its own. The slice contains everything that came before frame B and keeps
 * the original frame numbers, so it replays to the same images. It is as exact as a slice can be: the segments of the file that lie wholly inside the
 * slice are copied unchanged, and only the segment that frame B ends in is cut.
 *
 * A slice is a smaller workload that is easier to move around, to analyse and to replay up to a frame. A head slice does not help with a problem that
 * shows at frame 29,000 of a 30,000-frame capture, but it helps a lot with one that shows at frame 300.
 */
#ifndef ANALYSIS_APIDUMPSLICING_HEAD_SLICER_H
#define ANALYSIS_APIDUMPSLICING_HEAD_SLICER_H

#include "analysis/api_dump_slicing/types.h"
#include "capture/api_dump_loader/workload_index.h"
#include "capture/api_dump_loader/workload_reader.h"
#include <stddef.h>
#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpSlicing
    {
        class HeadSlicer
        {
        public:
            /* Writes the slice of the workload. It reads the index of the workload, or builds it and stores it next to the workload (see
             * WorkloadIndex). Returns false, with the reason in the error, if the workload cannot be read, has no frame B (SliceOptions::last_frame),
             * or the slice cannot be written.
             */
            static bool slice(const std::string&  in_workload_filename,
                              const std::string&  in_slice_filename,
                              const SliceOptions& in_options,
                              SliceReport*        out_report_ptr,
                              std::string*        out_error_ptr);

        private:
            /* Where the slice ends: the segment that contains the end of the last frame (or the end of the workload, if that frame is still running
             * there), and how many calls of that segment the slice keeps.
             */
            struct SliceEnd
            {
                size_t n_calls_in_segment = 0; /* 0 means all calls */
                size_t n_segment          = 0;
            };

            /* Counts the calls in the segment that end a frame. */
            static uint64_t count_frame_ends(const Capture::APIDumpLoader::Workload& in_segment);

            /* Keeps the calls of the segment up to and including the call that ends in_last_frame, and the data chunks that these calls refer to.
             * Returns the number of calls kept. in_first_frame_index is the index of the first frame that ends in the segment. A frame that does
             * not end in the segment (the last frame of the workload, which is still running) keeps all of its calls.
             */
            static size_t cut_segment(const uint64_t&                   in_first_frame_index,
                                      const uint64_t&                   in_last_frame,
                                      Capture::APIDumpLoader::Workload* inout_segment_ptr);

            /* Finds where the slice ends. Returns false if the workload has no frame in_last_frame. */
            static bool find_slice_end(const Capture::APIDumpLoader::WorkloadIndex& in_index,
                                       const uint64_t&                              in_last_frame,
                                       SliceEnd*                                    out_end_ptr);
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_HEAD_SLICER_H */
