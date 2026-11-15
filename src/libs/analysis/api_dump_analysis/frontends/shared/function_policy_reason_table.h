/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A lookup table for the policies of the Direct3D front ends. Each entry is a function name with the reason it is listed.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_FUNCTION_POLICY_REASON_TABLE_H
#define APIDUMPANALYSIS_FRONTENDS_FUNCTION_POLICY_REASON_TABLE_H

#include "common/types.h"
#include <stddef.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class FunctionPolicyReasonTable
            {
            public:
                /* A name is one of three things: the name of a method alone ("BeginScene", which matches every interface that has the method),
                 * the interface and method name ("IDirectDrawSurface7::GetDC", which matches that method only), or the name of a function that
                 * the DLL exports.
                 */
                struct Entry
                {
                    const char* name_ptr;
                    const char* reason_ptr;
                };

                /* The reason listed for the function in the table, or null if the table does not have the function. */
                static const char* find_reason_ptr(const Entry*                       in_entry_ptr,
                                                   const size_t&                      in_n_entries,
                                                   const APIInterceptor::APIFunction& in_function);

                /* True for the methods that only ask for something and change nothing: the names that start with Get, Enum, Check or Is. */
                static bool is_query(const APIInterceptor::APIFunction& in_function);

            private:
                /* The name of the method without the name of its interface. */
                static const char* get_method_name_ptr(const char* in_full_name_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FUNCTION_POLICY_REASON_TABLE_H */
