/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The state of the Direct3D 7 device that a draw depends on: render states, texture stage states, lights and textures (kept by
 * FixedFunctionState), plus the vertex format, which each draw supplies. There are no shaders.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D7_PIPELINE_STATE_H
#define APIDUMPANALYSIS_FRONTENDS_D3D7_PIPELINE_STATE_H

#include "analysis/api_dump_analysis/frontends/shared/fixed_function_state.h"
#include "analysis/api_dump_analysis/types.h"
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

                /* Describes a draw using the current state. The vertices are in the application's memory unless a vertex buffer is given. */
                Analysis::APIDumpAnalysis::DrawDescription describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                         const uint32_t&                                     in_vertex_count,
                                                                         const bool&                                         in_is_indexed,
                                                                         const uint32_t&                                     in_fvf,
                                                                         const Analysis::APIDumpAnalysis::ResourceId&        in_vertex_buffer_id);

                /* The state of the fixed-function pipeline. The front end updates it from the calls that set it. */
                FixedFunctionState& get_fixed_function_state();

                /* Restores the state a new device starts with. */
                void reset();





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
