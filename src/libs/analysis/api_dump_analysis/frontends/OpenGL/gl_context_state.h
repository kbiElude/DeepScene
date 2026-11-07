/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the OpenGL front end keeps of one context.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CONTEXT_STATE_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CONTEXT_STATE_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_pipeline_state.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* What a glPushAttrib saved: the state, and which parts of it the mask asked for. Follows GlAttributes and GlPipelineState. */
            struct GlAttributeFrame
            {
                GlAttributes    attributes;
                uint32_t        mask = 0;
                GlPipelineState pipeline_state;
            };

            /* The state of one OpenGL context, from the calls that set it (the values a context starts with are those of the specification).
             * The matrices of the three kinds each have a stack, which is never empty (the current matrix is its last element): the model-view
             * and the projection, which the front end tells the model builder of, and the others (texture and color), which it only keeps right.
             * Follows GlAttributeFrame, GlAttributes, GlImmediateBlock, GlListRecording, GlMatrix and GlPipelineState.
             */
            struct GlContextState
            {
                std::vector<GlAttributeFrame> attribute_frame_vec;
                GlAttributes                  attributes;
                GlImmediateBlock              immediate_block;
                GlListRecording               list_recording;
                std::vector<GlMatrix>         modelview_f32mat_vec;
                std::vector<GlMatrix>         other_f32mat_vec;
                GlPipelineState               pipeline_state;
                std::vector<GlMatrix>         projection_f32mat_vec;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CONTEXT_STATE_H */
