/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The state that the OpenGL front end keeps for one context.
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
            /* What a glPushAttrib saved: the state, and the parts of it that the mask selected. Follows GLAttributes and GLPipelineState. */
            struct GLAttributeFrame
            {
                GLAttributes    attributes;
                uint32_t        mask = 0;
                GLPipelineState pipeline_state;
            };

            /* The state of one OpenGL context, built from the calls that set it. A new context starts with the values of the specification.
             * Each kind of matrix has a stack that is never empty: its last element is the current matrix. The front end reports the model-view
             * and projection matrices to the model builder. It tracks the texture and color matrices only to keep them correct.
             * Follows GLAttributeFrame, GLAttributes, GLImmediateBlock, GLListRecording, GLMatrix and GLPipelineState.
             */
            struct GLContextState
            {
                std::vector<GLAttributeFrame> attribute_frame_vec;
                GLAttributes                  attributes;
                GLImmediateBlock              immediate_block;
                GLListRecording               list_recording;
                std::vector<uint64_t>         modelview_anchor_call_index_vec; /* per matrix of the stack below: the call that begins its camera (the last load of that matrix, a glLoadIdentity or a glLoadMatrix), or UINT64_MAX */
                std::vector<GLMatrix>         modelview_f32mat_vec;
                std::vector<GLMatrix>         other_f32mat_vec;
                GLPipelineState               pipeline_state;
                GLPixelStore                  pixel_store;
                std::vector<GLMatrix>         projection_f32mat_vec;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_CONTEXT_STATE_H */
