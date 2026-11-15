/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The fixed-function pipeline state that the APIs with such a pipeline have in common: render states, texture stage states, lights and the
 * textures bound to the stages. The differences between the APIs (the state numbers, the defaults, which states are free numbers) are given by
 * the rules that the state is created with.
 *
 * The state key identifies the state that a draw is made with. States that choose a mode (a blend mode, depth writes) contribute their value
 * to the key. States that are free numbers (colors, fog distances, references) contribute only whether they are set. Otherwise two draws
 * would differ whenever such a number changed. Lights contribute their index and type.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_STATE_H
#define APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_STATE_H

#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <set>
#include <stdint.h>
#include <utility>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class FixedFunctionState
            {
            public:
                typedef bool (*IsValueIrrelevantFuncPtr)(const uint32_t& in_state);

                /* The fixed-function state of an API, as the API defines it. */
                struct Rules
                {
                    uint32_t                 blend_enable_default;
                    uint32_t                 blend_enable_state;
                    uint32_t                 depth_enable_default;
                    uint32_t                 depth_enable_state;
                    IsValueIrrelevantFuncPtr is_render_state_value_irrelevant_func_ptr;
                    IsValueIrrelevantFuncPtr is_texture_stage_state_value_irrelevant_func_ptr;
                    uint32_t                 n_max_lights;         /* an index at or above this is not a light of a device */
                    uint32_t                 n_max_render_states;
                    uint32_t                 n_max_stage_states;
                    uint32_t                 n_max_stages;
                    uint32_t                 stencil_enable_default;
                    uint32_t                 stencil_enable_state;
                };

                explicit FixedFunctionState(const Rules& in_rules);

                /* Describes a draw with the current state. The caller adds where the vertices come from. */
                Analysis::APIDumpAnalysis::DrawDescription describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                         const uint32_t&                                     in_vertex_count,
                                                                         const bool&                                         in_is_indexed);

                /* Returns to the state that a device starts with. */
                void reset();

                /* The functions that take an index return false and change nothing for an index that no device has. Accepting it would only make
                 * the state grow.
                 */
                bool set_light       (const uint32_t& in_index,
                                      const uint32_t& in_type);
                bool set_light_enable(const uint32_t& in_index,
                                      const bool&     in_is_enabled);

                bool set_render_state(const uint32_t& in_state,
                                      const uint32_t& in_value);

                /* A view whose resource is NONE unbinds the stage. */
                bool set_texture(const uint32_t&                                in_stage,
                                 const Analysis::APIDumpAnalysis::ResourceView& in_view);

                bool set_texture_stage_state(const uint32_t& in_stage,
                                             const uint32_t& in_state,
                                             const uint32_t& in_value);

            private:
                typedef std::pair<uint32_t, uint32_t> StageStateKey;

                uint64_t compute_state_key() const;

                /* The value of a render state, or the default if the state was never set. */
                uint32_t get_render_state_value(const uint32_t& in_state,
                                                const uint32_t& in_default) const;


                std::set<uint32_t>                                          m_enabled_light_set;
                bool                                                        m_is_state_key_valid;
                std::map<uint32_t, uint32_t>                                m_light_type_map;           /* by light index */
                std::map<uint32_t, uint32_t>                                m_render_state_map;
                Rules                                                       m_rules;
                uint64_t                                                    m_state_key;
                std::map<StageStateKey, uint32_t>                           m_texture_stage_state_map;
                std::map<uint32_t, Analysis::APIDumpAnalysis::ResourceView> m_texture_view_map;         /* by stage       */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_STATE_H */
