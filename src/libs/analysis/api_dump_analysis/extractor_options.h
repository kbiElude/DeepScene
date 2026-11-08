/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the programs that read a workload and write a file of what they found (APIDumpAnalysisExtractor, APIDumpDependencyExtractor, APIDumpTextureExtractor) are asked
 * to do in the same way: which workload, which file, how much of it, how many threads, and what to say while they work.
 */
#ifndef APIDUMPANALYSIS_EXTRACTOR_OPTIONS_H
#define APIDUMPANALYSIS_EXTRACTOR_OPTIONS_H

#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        struct ExtractorOptions
        {
            std::string input_filename;
            uint64_t    max_frames                     = 0;      /* 0: all of them                                                                                               */
            uint32_t    n_threads                      = 0;      /* that read the workload ahead of the analysis; set from the machine when the arguments are read, if not given */
            std::string output_filename;
            bool        print_stats                    = false;
            double      progress_interval_seconds      = 5.0;    /* 0: every segment                                                                                             */
            uint32_t    skip_data_chunks_above_n_bytes = 0;      /* the data chunks of more bytes are not read (they come empty); 0: all are read                               */
        };
    }
}

#endif /* APIDUMPANALYSIS_EXTRACTOR_OPTIONS_H */
