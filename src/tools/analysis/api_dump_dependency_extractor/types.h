/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the extractor of the dependencies.
 */
#ifndef APIDUMPDEPENDENCYEXTRACTOR_TYPES_H
#define APIDUMPDEPENDENCYEXTRACTOR_TYPES_H

#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpDependencyExtractor
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS        = 1,
            CANNOT_READ_WORKLOAD = 2,
            CANNOT_WRITE_DEPS    = 3,
            SUCCESS              = 0
        };

        struct Options
        {
            std::string input_filename;
            uint64_t    max_frames                     = 0;      /* 0: all of them */
            uint32_t    n_threads                      = 0;      /* that read the workload ahead of the analysis, and work out the dependencies; set from the machine when the arguments are read */
            std::string output_filename;
            bool        print_stats                    = false;
            double      progress_interval_seconds      = 5.0;    /* 0: every segment */
            uint32_t    skip_data_chunks_above_n_bytes = 64 * 1024; /* the vertices and the pixels, which the analysis does not look at; 0: none is left out */
        };
    }
}

#endif /* APIDUMPDEPENDENCYEXTRACTOR_TYPES_H */
