/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/shared/function_name_table.h"
#include "capture/api_dump_names/function_names.h"
#include <string.h>

const char* Analysis::APIDumpAnalysis::Frontends::FunctionNameTable::find_reason_ptr(const Entry*                       in_entry_ptr,
                                                                                     const size_t&                      in_n_entries,
                                                                                     const APIInterceptor::APIFunction& in_function)
{
    const char* full_name_ptr   = Capture::APIDumpNames::get_function_name(in_function);
    const char* method_name_ptr = (full_name_ptr != nullptr) ? get_method_name_ptr(full_name_ptr)
                                                              : nullptr;
    const char* result_ptr      = nullptr;

    /* A value that is not a function (the marks that begin and end the methods) has no name and is in no table. */
    for (size_t n_entry = 0;
                n_entry < in_n_entries &&
                full_name_ptr != nullptr;
              ++n_entry)
    {
        const char* name_ptr = in_entry_ptr[n_entry].name_ptr;
        const bool  is_match = (::strstr(name_ptr,
                                         "::") != nullptr) ? (::strcmp(name_ptr,
                                                                       full_name_ptr)   == 0)
                                                           : (::strcmp(name_ptr,
                                                                       method_name_ptr) == 0);

        if (is_match)
        {
            result_ptr = in_entry_ptr[n_entry].reason_ptr;

            break;
        }
    }

    return result_ptr;
}

const char* Analysis::APIDumpAnalysis::Frontends::FunctionNameTable::get_method_name_ptr(const char* in_full_name_ptr)
{
    const char* separator_ptr = ::strstr(in_full_name_ptr,
                                         "::");

    return (separator_ptr != nullptr) ? separator_ptr + 2
                                      : in_full_name_ptr;
}

bool Analysis::APIDumpAnalysis::Frontends::FunctionNameTable::is_query(const APIInterceptor::APIFunction& in_function)
{
    const char* full_name_ptr = Capture::APIDumpNames::get_function_name(in_function);
    bool        result        = false;

    if (full_name_ptr != nullptr)
    {
        const char* method_name_ptr = get_method_name_ptr(full_name_ptr);

        result = (::strncmp(method_name_ptr, "Check", 5) == 0 ||
                  ::strncmp(method_name_ptr, "Enum",  4) == 0 ||
                  ::strncmp(method_name_ptr, "Get",   3) == 0 ||
                  ::strncmp(method_name_ptr, "Is",    2) == 0);
    }

    return result;
}
