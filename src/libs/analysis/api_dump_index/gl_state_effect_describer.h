/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the calls of OpenGL do to state. Every function that is met has to be named in the table of classify(), with what it does to state, and a
 * function that is not ends the process: leaving out a call that something reads would give a wrong image, far from where it went wrong.
 * When in doubt, a call is a barrier: it is made as it was, and costs only the time that leaving it out would have saved.
 */
#ifndef APIDUMPINDEX_GL_STATE_EFFECT_DESCRIBER_H
#define APIDUMPINDEX_GL_STATE_EFFECT_DESCRIBER_H

#include "analysis/api_dump_index/gl_call_describer.h"
#include "analysis/api_dump_index/i_state_effect_describer.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class GLStateEffectDescriber : public IStateEffectDescriber
        {
        public:
            GLStateEffectDescriber();

            StateEffect describe  (const Capture::APIDumpLoader::DumpedAPICall& in_call) override;
            Capture::APIDumpLoader::APIGroup    get_group () const override;

        private:
            /* The kind of state that a key names; the rest of the key is what picks the state (an enum, a light). */
            enum class Family : uint32_t
            {
                ALPHA_FUNC = 1,
                BLEND_FUNC,
                CAPABILITY,
                CLEAR_COLOR,
                CLEAR_DEPTH,
                CLEAR_STENCIL,
                CLIENT_ARRAY,
                CULL_FACE,
                DEPTH_FUNC,
                DEPTH_MASK,
                FOG,
                LIGHT,
                LIGHT_MODEL,
                LINE_WIDTH,
                MATERIAL,
                NORMAL,
                PIXEL_STORE,
                POINTER,
                POLYGON_MODE,
                POLYGON_OFFSET,
                SCISSOR,
                SHADE_MODEL,
                TEXCOORD,
                TEXTURE_ENV,
                TEXTURE_GEN,
                TEXTURE_PARAMETER,
                VIEWPORT,

                /* Added later: the numbers are part of the keys, so they follow the others. */
                COLOR_MASK,
                FRONT_FACE,
                STENCIL_FUNC,
                STENCIL_MASK,
                STENCIL_OP
            };

            /* What the function does to state, by the table, when it is not recorded into a list. Ends the process for a function that is not in it. */
            static StateEffect classify(const Capture::APIDumpLoader::DumpedAPICall& in_call);

            static bool is_in(const uint32_t& in_value,
                              const uint32_t* in_value_ptr,
                              const size_t&   in_n_values);

            static StateEffect make_assign         (const uint32_t& in_domain,
                                                    const Family&   in_family,
                                                    const uint32_t& in_first,
                                                    const uint32_t& in_second,
                                                    const uint32_t& in_selector_mask);
            static StateEffect make_barrier        ();

            /* What a function that has a kind and no entry of its own does to state: queries and names change none, any other call is a barrier. */
            static StateEffect make_generic_effect (const APIInterceptor::APIFunction& in_function);
            static StateEffect make_ordered        (const uint32_t& in_domain_mask,
                                                    const uint32_t& in_selector_mask);
            static StateEffect make_select         (const uint32_t& in_selector,
                                                    const uint32_t& in_value);

            /* The number in the argument. Ends the process if the call has none: a key made of a guess would merge calls that differ. */
            static uint32_t read_argument(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const size_t&        in_index);

            /* The domains of state: a call that sets state of one is only observed by a call that reads that domain. */
            static const uint32_t DOMAIN_CAPABILITY;
            static const uint32_t DOMAIN_CLIENT_ARRAY;
            static const uint32_t DOMAIN_CURRENT;
            static const uint32_t DOMAIN_FOG;
            static const uint32_t DOMAIN_LIGHT;
            static const uint32_t DOMAIN_LIGHT_MODEL;
            static const uint32_t DOMAIN_MATERIAL;
            static const uint32_t DOMAIN_PIXEL_STORE;
            static const uint32_t DOMAIN_POINTER;
            static const uint32_t DOMAIN_RASTER;
            static const uint32_t DOMAIN_TEXTURE_ENV;
            static const uint32_t DOMAIN_TEXTURE_GEN;
            static const uint32_t DOMAIN_TEXTURE_PARAMETER;

            /* What the calls choose the target of later calls with. */
            static const uint32_t SELECTOR_ACTIVE_TEXTURE;
            static const uint32_t SELECTOR_CLIENT_ACTIVE_TEXTURE;
            static const uint32_t SELECTOR_MATRIX_MODE;

            GLCallDescriber m_call_describer; /* follows glNewList and glEndList */
        };
    }
}

#endif /* APIDUMPINDEX_GL_STATE_EFFECT_DESCRIBER_H */
