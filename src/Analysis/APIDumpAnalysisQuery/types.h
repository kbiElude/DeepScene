/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the query tool.
 */
#ifndef APIDUMPANALYSISQUERY_TYPES_H
#define APIDUMPANALYSISQUERY_TYPES_H

#include "Analysis/APIDumpAnalysis/types.h"
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisQuery
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS      = 1,
            CANNOT_READ_PASSES = 2,
            NOT_FOUND          = 3,
            SUCCESS            = 0
        };

        struct Options
        {
            Analysis::APIDumpAnalysis::QueryArguments arguments;
            std::string                               command;
            bool                                      is_compact = false;
            std::string                               passes_filename;
        };
    }
}

#endif /* APIDUMPANALYSISQUERY_TYPES_H */
