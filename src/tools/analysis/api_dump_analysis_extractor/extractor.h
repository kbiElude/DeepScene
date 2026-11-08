/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Extractor tool logic (defined in main.cpp).
 */
#ifndef APIDUMPANALYSISEXTRACTOR_EXTRACTOR_H
#define APIDUMPANALYSISEXTRACTOR_EXTRACTOR_H

#include "analysis/api_dump_analysis/model.h"
#include "types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisExtractor
    {
        void print_stats(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);
        void print_usage();

        /* Entry point of the tool (called by the global main). */
        int run(int   in_argc,
                char* in_argv[]);
    }
}

#endif /* APIDUMPANALYSISEXTRACTOR_EXTRACTOR_H */
