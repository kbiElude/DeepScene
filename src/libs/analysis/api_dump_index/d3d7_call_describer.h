/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Describes the calls of Direct3D 7.
 */
#ifndef APIDUMPINDEX_D3D7_CALL_DESCRIBER_H
#define APIDUMPINDEX_D3D7_CALL_DESCRIBER_H

#include "analysis/api_dump_index/i_call_describer.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class D3D7CallDescriber : public ICallDescriber
        {
        public:
            CallClass describe(const Capture::APIDumpLoader::DumpedAPICall& in_call) override;
            Capture::APIDumpLoader::APIGroup  get_group() const override;

            /* The class of the function alone. */
            static CallClass get_function_class(const APIInterceptor::APIFunction& in_function);
        };
    }
}

#endif /* APIDUMPINDEX_D3D7_CALL_DESCRIBER_H */
