/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front ends do with a call that they do not follow, and that is not of no matter: the process ends, saying the function and why. Nothing is left out
 * without being said (AI_ASSERT_ALWAYS).
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_UNSUPPORTED_USE_H
#define APIDUMPANALYSIS_FRONTENDS_UNSUPPORTED_USE_H

#include "common/types.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class UnsupportedUse
            {
            public:
                /* Ends the process: the function that the workload calls, and the reason that what it does is not followed. */
                static void report(const APIInterceptor::APIFunction& in_function,
                                   const char*                        in_reason_ptr);

                /* The same, with the value of the argument that makes the call one that is not followed. */
                static void report_with_value(const APIInterceptor::APIFunction& in_function,
                                              const char*                        in_reason_ptr,
                                              const uint64_t&                    in_value);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_UNSUPPORTED_USE_H */
