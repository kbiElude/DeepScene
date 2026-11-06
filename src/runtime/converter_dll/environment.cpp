/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "environment.h"
#include <stdlib.h>
#include <Windows.h>

bool Stereo::ConverterDLL::Environment::get_number(const char* in_name_ptr,
                                                   float*      out_value_ptr)
{
    char                value_i8vec32[32] = {};
    const unsigned long n_chars           = ::GetEnvironmentVariableA(in_name_ptr,
                                                                      value_i8vec32,
                                                                      static_cast<DWORD>(sizeof(value_i8vec32) ));
    const bool          result            = (n_chars > 0                      &&
                                             n_chars < sizeof(value_i8vec32) );

    if (result)
    {
        *out_value_ptr = static_cast<float>(::atof(value_i8vec32) );
    }

    return result;
}

bool Stereo::ConverterDLL::Environment::is_set(const char* in_name_ptr)
{
    return (::GetEnvironmentVariableA(in_name_ptr,
                                      nullptr,
                                      0) != 0);
}
