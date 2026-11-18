/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says what the calls of one API do to state (see StateEffect), so that StateCoalescer can leave out the calls that nothing observes.
 * A function that a describer does not know ends the process: a wrong guess would lose a call.
 */
#ifndef APIDUMPINDEX_I_STATE_EFFECT_DESCRIBER_H
#define APIDUMPINDEX_I_STATE_EFFECT_DESCRIBER_H

#include "analysis/api_dump_index/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include "capture/api_dump_loader/types.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class IStateEffectDescriber
        {
        public:
            virtual ~IStateEffectDescriber() = default;

            /* Calls come in the order they were made. Ends the process for a function it does not know. */
            virtual StateEffect describe(const Capture::APIDumpLoader::DumpedAPICall& in_call) = 0;

            virtual Capture::APIDumpLoader::APIGroup get_group() const = 0;
        };
    }
}

#endif /* APIDUMPINDEX_I_STATE_EFFECT_DESCRIBER_H */
