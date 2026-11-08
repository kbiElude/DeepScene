/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the query tool of the textures.
 */
#ifndef APIDUMPTEXTUREQUERY_TYPES_H
#define APIDUMPTEXTUREQUERY_TYPES_H

#include "analysis/api_dump_analysis/query_options.h"
#include "analysis/api_dump_textures/query_commands.h"
#include <string>

namespace Analysis
{
    namespace APIDumpTextureQuery
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS        = 1,
            CANNOT_READ_TEXTURES = 2,
            NOT_FOUND            = 3,
            SUCCESS              = 0,
            WORKLOAD_MISMATCH    = 4
        };

        struct Options
        {
            Analysis::APIDumpTextures::QueryArguments arguments;
            std::string                               command;
            Analysis::APIDumpAnalysis::QueryOptions   common;
            std::string                               textures_filename;
        };
    }
}

#endif /* APIDUMPTEXTUREQUERY_TYPES_H */
