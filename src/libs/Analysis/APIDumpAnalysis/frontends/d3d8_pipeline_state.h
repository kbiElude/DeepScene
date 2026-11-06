/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the D3D8 device has bound and set that a draw depends on: buffers, textures, shaders (or the vertex format), render
 * states, texture stage states and lights. The state of the fixed-function pipeline is kept by FixedFunctionState, which tells
 * how the state key is made.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_PIPELINE_STATE_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_PIPELINE_STATE_H

#include "Analysis/APIDumpAnalysis/frontends/fixed_function_state.h"
#include "Analysis/APIDumpAnalysis/types.h"
#include <map>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D8PipelineState
            {
            public:
                D3D8PipelineState();

                /* Describes a draw with what is bound now. */
                Analysis::APIDumpAnalysis::DrawDescription describe_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                         const uint32_t&                                     in_vertex_count,
                                                                         const bool&                                         in_is_indexed);

                /* Describes a draw whose vertices (and indices) are in the application's memory, not in buffers. */
                Analysis::APIDumpAnalysis::DrawDescription describe_user_pointer_draw(const Analysis::APIDumpAnalysis::PrimitiveTopology& in_topology,
                                                                                      const uint32_t&                                     in_vertex_count,
                                                                                      const bool&                                         in_is_indexed,
                                                                                      const uint32_t&                                     in_stride);

                /* The FVF code of the vertices; 0 if a vertex shader is set, whose declaration says what they are like. */
                uint32_t get_fvf() const;

                Analysis::APIDumpAnalysis::ResourceId get_index_buffer_id() const;

                /* The vertex buffer of a stream and its stride; false if no buffer is bound to it. */
                bool get_stream_source(const uint32_t&                        in_stream,
                                       Analysis::APIDumpAnalysis::ResourceId* out_id_ptr,
                                       uint32_t*                              out_stride_ptr) const;

                /* Back to what a device starts with, which is also what Reset leaves. */
                /* The state of the fixed-function pipeline, which the front end follows the calls that set it of. */
                FixedFunctionState& get_fixed_function_state();

                void reset();

                /* The functions which take an index return false, and change nothing, for an index which no device has: it would only make
                 * the state grow.
                 */
                void set_index_buffer(const Analysis::APIDumpAnalysis::ResourceId& in_id);

                /* The shader NONE is no pixel shader. */
                void set_pixel_shader(const Analysis::APIDumpAnalysis::ResourceId& in_id);


                /* The vertex buffer of a stream; an id of NONE unbinds the stream. */
                bool set_stream_source(const uint32_t&                              in_stream,
                                       const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                       const uint32_t&                              in_stride);



                /* The vertices have the format that an FVF code says, and the fixed-function pipeline handles them. */
                void set_vertex_shader(const uint32_t& in_fvf);

                /* A vertex shader, whose declaration says what the vertices are like. */
                void set_vertex_shader(const Analysis::APIDumpAnalysis::ResourceId& in_id,
                                       const uint64_t&                              in_declaration_hash);

            private:
                struct StreamBinding
                {
                    Analysis::APIDumpAnalysis::ResourceId id     = Analysis::APIDumpAnalysis::ResourceId::NONE;
                    uint32_t                              stride = 0;
                };

                /* The shaders that are set, in the order of the stages of the pipeline. */
                std::vector<Analysis::APIDumpAnalysis::ResourceId> get_shader_id_vec() const;


                static FixedFunctionState::Rules get_rules();

                static bool is_value_irrelevant_render_state       (const uint32_t& in_state);
                static bool is_value_irrelevant_texture_stage_state(const uint32_t& in_state);


                FixedFunctionState                    m_fixed_function_state;
                Analysis::APIDumpAnalysis::ResourceId m_index_buffer_id;
                Analysis::APIDumpAnalysis::ResourceId m_pixel_shader_id;
                std::map<uint32_t, StreamBinding>     m_stream_binding_map;       /* by stream */
                uint64_t                              m_vertex_layout_value;      /* of the FVF code, or of the declaration of the vertex shader */
                Analysis::APIDumpAnalysis::ResourceId m_vertex_shader_id;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_PIPELINE_STATE_H */
