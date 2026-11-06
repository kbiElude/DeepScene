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
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICING_HEAD_SLICER_H */
