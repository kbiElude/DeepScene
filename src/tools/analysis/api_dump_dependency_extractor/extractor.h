/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Extractor tool logic (defined in main.cpp).
 */
#ifndef APIDUMPDEPENDENCYEXTRACTOR_EXTRACTOR_H
#define APIDUMPDEPENDENCYEXTRACTOR_EXTRACTOR_H

#include "analysis/api_dump_dependencies/types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpDependencyExtractor
    {
        void print_stats(const Analysis::APIDumpDependencies::DependencyFileContent& in_analysis);
        void print_usage();

        /* Entry point of the tool (called by the global main). */
        int run(int   in_argc,
                char* in_argv[]);
    }
}

#endif /* APIDUMPDEPENDENCYEXTRACTOR_EXTRACTOR_H */
