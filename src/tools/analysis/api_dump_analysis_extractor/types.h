/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the extractor tool.
 */
#ifndef APIDUMPANALYSISEXTRACTOR_TYPES_H
#define APIDUMPANALYSISEXTRACTOR_TYPES_H

#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysisExtractor
    {
        /* What the tool exits with. */
        enum class ExitCode
        {
            BAD_ARGUMENTS        = 1,
            CANNOT_READ_WORKLOAD = 2,
            CANNOT_WRITE_PASSES  = 3,
            SUCCESS              = 0
        };

    }
}

#endif /* APIDUMPANALYSISEXTRACTOR_TYPES_H */
