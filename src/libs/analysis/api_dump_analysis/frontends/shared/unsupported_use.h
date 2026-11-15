/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front ends do with a call that they do not follow and that cannot be ignored: they end the process with a message that names the
 * function and the reason. Nothing is dropped silently (AI_ASSERT_ALWAYS).
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
                /* Ends the process. The message names the function that the workload called and the reason why it is not followed. */
                static void report(const APIInterceptor::APIFunction& in_function,
                                   const char*                        in_reason_ptr);

                /* Like report(), and the message also contains the argument value that makes the call unsupported. */
                static void report_with_value(const APIInterceptor::APIFunction& in_function,
                                              const char*                        in_reason_ptr,
                                              const uint64_t&                    in_value);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_UNSUPPORTED_USE_H */
