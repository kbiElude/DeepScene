/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The describers of all APIs, asked as one: a call goes to the describer of its API. Calls of other APIs are state.
 *
 * Use one set for each walk over the calls, in order.
 */
#ifndef APIDUMPINDEX_CALL_DESCRIBER_SET_H
#define APIDUMPINDEX_CALL_DESCRIBER_SET_H

#include "analysis/api_dump_index/i_call_describer.h"
#include <memory>
#include <vector>

namespace Analysis
{
    namespace APIDumpIndex
    {
        class CallDescriberSet
        {
        public:
            CallDescriberSet();

            /* Calls come in the order they were made. */
            CallClass describe(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            /* The class of the function alone. */
            static CallClass get_function_class(const APIInterceptor::APIFunction& in_function);

        private:
            std::vector<std::unique_ptr<ICallDescriber> > m_describer_ptr_vec;
        };
    }
}

#endif /* APIDUMPINDEX_CALL_DESCRIBER_SET_H */
