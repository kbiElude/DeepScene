/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * For GDI32, USER32 and WGL: windows, device contexts, pixel formats, buffer swaps and OpenGL contexts. Every call is a barrier, because a
 * context or a window decides which state the calls around it act on.
 */
#ifndef APIDUMPINDEX_WINDOW_SYSTEM_STATE_EFFECT_DESCRIBER_H
#define APIDUMPINDEX_WINDOW_SYSTEM_STATE_EFFECT_DESCRIBER_H

#include "analysis/api_dump_index/i_state_effect_describer.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class WindowSystemStateEffectDescriber : public IStateEffectDescriber
        {
        public:
            /* The group is GDI32, USER32 or WGL. */
            explicit WindowSystemStateEffectDescriber(const Capture::APIDumpLoader::APIGroup& in_group);

            StateEffect describe  (const Capture::APIDumpLoader::DumpedAPICall& in_call) override;
            Capture::APIDumpLoader::APIGroup    get_group () const override;

        private:
            Capture::APIDumpLoader::APIGroup m_group;
        };
    }
}

#endif /* APIDUMPINDEX_WINDOW_SYSTEM_STATE_EFFECT_DESCRIBER_H */
