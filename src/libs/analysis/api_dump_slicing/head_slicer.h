/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The first frames of a workload, as a workload of their own: frames 0 to B. It has everything that came before frame B, so it replays to the same images
 * (the frames are numbered as they were), and it is as exact as a slice can be: the segments of the file which are wholly in it are copied as they are
 * and only the one that frame B ends in is cut. What a slice is for is a workload which is smaller to move around, to analyse and to replay up to a
 * frame: a capture of 30,000 frames that a problem shows at frame 29,000 of is not made any smaller by a head slice, but one that shows at frame 300 is.
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
            /* Writes the slice of the workload. Reads the index of the workload (or makes it, and keeps it next to the workload: see WorkloadIndex). False,
             * and why, if the workload cannot be read, has no frame B, or the slice cannot be written.
             */
            static bool slice(const std::string&  in_workload_filename,
                              const std::string&  in_slice_filename,
                              const SliceOptions& in_options,
                              SliceReport*        out_report_ptr,
                              std::string*        out_error_ptr);

        private:
            /* Where the slice ends: the segment that has the end of the last frame (or the end of the workload, if that frame is still going on there), and how
             * many of its calls the slice has.
             */
            struct SliceEnd
            {
                size_t n_calls_in_segment = 0; /* 0: all of them */
                size_t n_segment          = 0;
            };

            /* The calls of the segment which end a frame. */
            static uint64_t count_frame_ends(const Capture::APIDumpLoader::Workload& in_segment);

            /* Leaves the calls up to the one that ends the frame (inclusive) in the segment, and the data chunks which they refer to; returns how many calls are
             * left. The segment starts the frames at in_first_frame_index; a frame which ends nowhere in it (the last of the workload, still going on) keeps
             * all of its calls.
             */
            static size_t cut_segment(const uint64_t&                   in_first_frame_index,
                                      const uint64_t&                   in_last_frame,
                                      Capture::APIDumpLoader::Workload* inout_segment_ptr);

            /* False if the workload has no such frame. */
            static bool find_slice_end(const Capture::APIDumpLoader::WorkloadIndex& in_index,
                                       const uint64_t&                              in_last_frame,
                                       SliceEnd*                                    out_end_ptr);
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_HEAD_SLICER_H */
