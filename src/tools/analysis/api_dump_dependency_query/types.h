/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the query tool of the dependencies.
 */
#ifndef APIDUMPDEPENDENCYQUERY_TYPES_H
#define APIDUMPDEPENDENCYQUERY_TYPES_H

#include "analysis/api_dump_analysis/query_options.h"
#include "analysis/api_dump_dependencies/query_commands.h"
#include <string>

namespace Analysis
{
    namespace APIDumpDependencyQuery
    {
        struct Options
        {
            Analysis::APIDumpDependencies::QueryArguments arguments;
            std::string                                   command;
            Analysis::APIDumpAnalysis::QueryOptions       common;
            std::string                                   deps_filename;
        };
    }
}

#endif /* APIDUMPDEPENDENCYQUERY_TYPES_H */
