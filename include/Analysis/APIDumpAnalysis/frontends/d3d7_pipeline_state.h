/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the Direct3D 7 device has set that a draw depends on: render states, texture stage states, lights and textures, which
 * FixedFunctionState keeps, and the format of the vertices, which a draw brings. There are no shaders.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D7_PIPELINE_STATE_H
#define APIDUMPANALYSIS_FRONTENDS_D3D7_PIPELINE_STATE_H

#include "Analysis/APIDumpAnalysis/frontends/fixed_function_state.h"
#include "Analysis/APIDumpAnalysis/types.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D7PipelineState
            {
            public:
                D3D7PipelineState();

                /* Describes a draw with what is set now. The vertices are in the application's memory unless a vertex buffer is given. */
                Analysis::APIDumpAnalysis::DrawDescription describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                         const uint32_t&                                     in_vertex_count,
                                                                         const bool&                                         in_is_indexed,
                                                                         const uint32_t&                                     in_fvf,
                                                                         const Analysis::APIDumpAnalysis::ResourceId&        in_vertex_buffer_id);

                /* Back to what a device starts with. */
                void reset();

                /* The functions which take an index return false, and change nothing, for an index which no device has. */
                bool set_light       (const uint32_t& in_index,
                                      const uint32_t& in_type);
                bool set_light_enable(const uint32_t& in_index,
                                      const bool&     in_is_enabled);

                bool set_render_state(const uint32_t& in_state,
                                      const uint32_t& in_value);

                /* A view with the resource NONE unbinds the stage. */
                bool set_texture(const uint32_t&                                in_stage,
                                 const Analysis::APIDumpAnalysis::ResourceView& in_view);

                bool set_texture_stage_state(const uint32_t& in_stage,
                                             const uint32_t& in_state,
                                             const uint32_t& in_value);

            private:
                static FixedFunctionState::Rules get_rules();

                static bool is_value_irrelevant_render_state       (const uint32_t& in_state);
                static bool is_value_irrelevant_texture_stage_state(const uint32_t& in_state);


                FixedFunctionState m_fixed_function_state;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D7_PIPELINE_STATE_H */
