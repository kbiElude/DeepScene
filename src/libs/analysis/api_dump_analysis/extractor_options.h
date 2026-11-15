/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The options shared by the programs that read a workload and write a file of what they found (APIDumpAnalysisExtractor,
 * APIDumpDependencyExtractor, APIDumpTextureExtractor): which workload, which output file, how much to read, how many threads and how often
 * to report progress.
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
            uint32_t    n_threads                      = 0;      /* threads that read ahead of the analysis; derived from the machine when the arguments are read, unless given */
            std::string output_filename;
            bool        print_stats                    = false;
            double      progress_interval_seconds      = 5.0;    /* 0: every segment                                                                                             */
            uint32_t    skip_data_chunks_above_n_bytes = 0;      /* data chunks above this size are not read (they come back empty); 0: all are read                            */
        };
    }
}

#endif /* APIDUMPANALYSIS_EXTRACTOR_OPTIONS_H */
