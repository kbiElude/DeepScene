/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * For an API whose calls are not coalesced (Direct3D 7 and 8 for now): every call is a barrier, so it is made as it was. The API is
 * named on purpose, so that an API that nobody described still ends the process.
 */
#ifndef APIDUMPINDEX_VERBATIM_STATE_EFFECT_DESCRIBER_H
#define APIDUMPINDEX_VERBATIM_STATE_EFFECT_DESCRIBER_H

#include "analysis/api_dump_index/i_state_effect_describer.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class VerbatimStateEffectDescriber : public IStateEffectDescriber
        {
        public:
            /* The group is D3D7 or D3D8. */
            explicit VerbatimStateEffectDescriber(const Capture::APIDumpLoader::APIGroup& in_group);

            StateEffect describe  (const Capture::APIDumpLoader::DumpedAPICall& in_call) override;
            Capture::APIDumpLoader::APIGroup    get_group () const override;

        private:
            Capture::APIDumpLoader::APIGroup m_group;
        };
    }
}

#endif /* APIDUMPINDEX_VERBATIM_STATE_EFFECT_DESCRIBER_H */
