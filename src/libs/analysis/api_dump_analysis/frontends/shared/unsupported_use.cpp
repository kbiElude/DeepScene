/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/unsupported_use.h"
#include "capture/api_dump_names/function_names.h"
#include "common/macros.h"
#include <stdio.h>

void Analysis::APIDumpAnalysis::Frontends::UnsupportedUse::report(const APIInterceptor::APIFunction& in_function,
                                                                  const char*                        in_reason_ptr)
{
    constexpr size_t N_MESSAGE_CHARACTERS = 512;

    char message_i8vec512[N_MESSAGE_CHARACTERS] = {};

    ::snprintf(message_i8vec512,
               sizeof(message_i8vec512),
               "%s: %s. A workload that does this is not supported yet, and what the analysis finds of it would be wrong or left out.",
               Capture::APIDumpNames::get_function_name(in_function),
               in_reason_ptr);

    AI_ASSERT_ALWAYS(false,
                     message_i8vec512);
}

void Analysis::APIDumpAnalysis::Frontends::UnsupportedUse::report_with_value(const APIInterceptor::APIFunction& in_function,
                                                                             const char*                        in_reason_ptr,
                                                                             const uint64_t&                    in_value)
{
    constexpr size_t N_MESSAGE_CHARACTERS = 512;

    char message_i8vec512[N_MESSAGE_CHARACTERS] = {};

    ::snprintf(message_i8vec512,
               sizeof(message_i8vec512),
               "%s, 0x%llX",
               in_reason_ptr,
               static_cast<unsigned long long>(in_value) );

    report(in_function,
           message_i8vec512);
}
