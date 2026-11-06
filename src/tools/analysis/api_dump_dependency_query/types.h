/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the query tool of the dependencies.
 */
#ifndef APIDUMPDEPENDENCYQUERY_TYPES_H
#define APIDUMPDEPENDENCYQUERY_TYPES_H

#include "analysis/api_dump_dependencies/query_commands.h"
#include <string>

namespace Analysis
{
    namespace APIDumpDependencyQuery
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS      = 1,
            CANNOT_READ_DEPS   = 2,
            NOT_FOUND          = 3,
            SUCCESS            = 0,
            WORKLOAD_MISMATCH  = 4
        };

        struct Options
        {
            Analysis::APIDumpDependencies::QueryArguments arguments;
            std::string                                   command;
            std::string                                   deps_filename;
            bool                                          is_compact    = false;
            bool                                          is_verify_off = false;
            std::string                                   workload_filename; /* where the workload is, if not where the analysis says */
        };
    }
}

#endif /* APIDUMPDEPENDENCYQUERY_TYPES_H */
