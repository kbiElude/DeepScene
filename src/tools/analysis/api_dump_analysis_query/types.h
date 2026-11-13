/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the query tool.
 */
#ifndef APIDUMPANALYSISQUERY_TYPES_H
#define APIDUMPANALYSISQUERY_TYPES_H

#include "analysis/api_dump_analysis/query_options.h"
#include "analysis/api_dump_analysis/types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisQuery
    {
        struct Options
        {
            Analysis::APIDumpAnalysis::QueryArguments arguments;
            std::string                               command;
            Analysis::APIDumpAnalysis::QueryOptions   common;
            std::string                               passes_filename;
        };
    }
}

#endif /* APIDUMPANALYSISQUERY_TYPES_H */
