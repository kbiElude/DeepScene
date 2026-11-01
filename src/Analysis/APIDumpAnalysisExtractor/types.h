/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the extractor tool.
 */
#ifndef APIDUMPANALYSISEXTRACTOR_TYPES_H
#define APIDUMPANALYSISEXTRACTOR_TYPES_H

#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisExtractor
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS        = 1,
            CANNOT_READ_WORKLOAD = 2,
            CANNOT_WRITE_PASSES  = 3,
            SUCCESS              = 0
        };

        struct Options
        {
            std::string input_filename;
            uint64_t    max_frames                = 0;    /* 0: all of them */
            std::string output_filename;
            bool        print_stats               = false;
            double      progress_interval_seconds = 5.0;  /* 0: every segment */
        };
    }
}

#endif /* APIDUMPANALYSISEXTRACTOR_TYPES_H */
