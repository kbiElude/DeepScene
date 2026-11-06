/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Query tool logic (defined in main.cpp).
 */
#ifndef APIDUMPANALYSISQUERY_QUERY_H
#define APIDUMPANALYSISQUERY_QUERY_H

#include "types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisQuery
    {
        /* Returns false, and says why, if the arguments are not right. */
        bool parse_arguments(const int&   in_argc,
                             char*        in_argv[],
                             Options*     out_options_ptr,
                             std::string* out_error_ptr);

        void print_usage();

        /* Entry point of the tool (called by the global main). */
        int run(int   in_argc,
                char* in_argv[]);
    }
}

#endif /* APIDUMPANALYSISQUERY_QUERY_H */
