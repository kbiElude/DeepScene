/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef ANALYSIS_APIDUMPSLICER_TYPES_H
#define ANALYSIS_APIDUMPSLICER_TYPES_H

#include "Analysis/APIDumpSlicing/types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpSlicer
    {
        /* What the exit code of the program says. */
        enum class ExitCode
        {
            BAD_ARGUMENTS = 1,
            SLICE_FAILED  = 2,
            SUCCESS       = 0
        };

        struct Options
        {
            std::string                            slice_filename;
            Analysis::APIDumpSlicing::SliceOptions slice_options;
            std::string                            workload_filename;
        };
    }
}

#endif /* ANALYSIS_APIDUMPSLICER_TYPES_H */
