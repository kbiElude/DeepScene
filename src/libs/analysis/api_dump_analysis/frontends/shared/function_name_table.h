/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The lookup that the policies of the Direct3D front ends are made with: a table of names, each with the reason that is written beside it.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_FUNCTION_NAME_TABLE_H
#define APIDUMPANALYSIS_FRONTENDS_FUNCTION_NAME_TABLE_H

#include "common/types.h"
#include <stddef.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class FunctionNameTable
            {
            public:
                /* A name is the name of a method alone ("BeginScene": every interface that has it), or the name of the interface and the method ("IDirectDrawSurface7::GetDC":
                 * that one only), or the name of a function that the DLL exports.
                 */
                struct Entry
                {
                    const char* name_ptr;
                    const char* reason_ptr;
                };

                /* The reason that is written beside the function in the table, or null if the table does not have it. */
                static const char* find_reason_ptr(const Entry*                       in_entry_ptr,
                                                   const size_t&                      in_n_entries,
                                                   const APIInterceptor::APIFunction& in_function);

                /* True for the methods that ask for something and change nothing: those whose names start with Get, Enum, Check or Is. */
                static bool is_query(const APIInterceptor::APIFunction& in_function);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FUNCTION_NAME_TABLE_H */
