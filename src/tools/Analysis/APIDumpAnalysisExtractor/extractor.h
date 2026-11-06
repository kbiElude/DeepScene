/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Extractor tool logic (defined in main.cpp).
 */
#ifndef APIDUMPANALYSISEXTRACTOR_EXTRACTOR_H
#define APIDUMPANALYSISEXTRACTOR_EXTRACTOR_H

#include "Analysis/APIDumpAnalysis/model.h"
#include "types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisExtractor
    {
        std::string get_default_output_filename(const std::string& in_input_filename);

        /* Returns false, and says why, if the arguments are not right. */
        bool parse_arguments(const int&   in_argc,
                             char*        in_argv[],
                             Options*     out_options_ptr,
                             std::string* out_error_ptr);

        void print_stats(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);
        void print_usage();

        /* Entry point of the tool (called by the global main). */
        int run(int   in_argc,
                char* in_argv[]);
    }
}

#endif /* APIDUMPANALYSISEXTRACTOR_EXTRACTOR_H */
