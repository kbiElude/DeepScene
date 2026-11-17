/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef TESTS_DUMPED_CALL_FACTORY_H
#define TESTS_DUMPED_CALL_FACTORY_H

#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/api_function_argument.h"
#include <stdint.h>
#include <vector>

namespace Tests
{
    /* A call of the function whose in-arguments are the unsigned numbers, in order. */
    inline Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function,
                                                           const std::vector<uint32_t>&       in_number_vec = {})
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        for (const auto number : in_number_vec)
        {
            result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(number) );
        }

        return result;
    }
}

#endif /* TESTS_DUMPED_CALL_FACTORY_H */
