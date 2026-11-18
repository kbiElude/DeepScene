/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says what the calls of one API do to the frames a replay makes (see CallClass).
 */
#ifndef APIDUMPINDEX_I_CALL_DESCRIBER_H
#define APIDUMPINDEX_I_CALL_DESCRIBER_H

#include "analysis/api_dump_index/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include "capture/api_dump_loader/types.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class ICallDescriber
        {
        public:
            virtual ~ICallDescriber() = default;

            /* Calls come in the order they were made: the answer may depend on the calls before. */
            virtual CallClass describe(const Capture::APIDumpLoader::DumpedAPICall& in_call) = 0;

            virtual Capture::APIDumpLoader::APIGroup get_group() const = 0;
        };
    }
}

#endif /* APIDUMPINDEX_I_CALL_DESCRIBER_H */
