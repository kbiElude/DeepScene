/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the OpenGL front end keeps of a context.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H

#include <array>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* A 4x4 matrix as OpenGL keeps it: the columns one after the other (element 12 to 14 are the translation). */
            typedef std::array<float, 16> GlMatrix;

            /* A rectangle of the framebuffer, in pixels from its lower left corner, as glViewport and glScissor say it. */
            struct GlBox
            {
                int32_t height = 0;
                int32_t width  = 0;
                int32_t x      = 0;
                int32_t y      = 0;
            };

            /* The state that glPushAttrib saves and glPopAttrib puts back, as far as the front end follows it (the matrices are not a part of it:
             * they have stacks of their own). Follows GlBox.
             */
            struct GlAttributes
            {
                std::array<float, 4> clear_color_fp32vec4 = {};
                double               clear_depth          = 1.0;
                uint32_t             clear_stencil        = 0;
                bool                 is_scissor_enabled   = false;
                uint32_t             matrix_mode          = 0x1700; /* GL_MODELVIEW */
                GlBox                scissor_box;
                GlBox                viewport;
            };

            /* What a glPushAttrib saved: the state, and which parts of it the mask asked for. Follows GlAttributes. */
            struct GlAttributeFrame
            {
                GlAttributes attributes;
                uint32_t     mask = 0;
            };

            /* The state of one OpenGL context, from the calls that set it (the values a context starts with are those of the specification).
             * The matrices of the three kinds each have a stack, which is never empty (the current matrix is its last element): the model-view
             * and the projection, which the front end tells the model builder of, and the others (texture and color), which it only keeps right.
             * Follows GlAttributeFrame, GlAttributes and GlMatrix.
             */
            struct GlContextState
            {
                std::vector<GlAttributeFrame> attribute_frame_vec;
                GlAttributes                  attributes;
                std::vector<GlMatrix>         modelview_f32mat_vec;
                std::vector<GlMatrix>         other_f32mat_vec;
                std::vector<GlMatrix>         projection_f32mat_vec;
            };

            /* A matrix as the model builder was last told it, to tell it again only when it differs. Follows GlMatrix. */
            struct GlReportedMatrix
            {
                bool     has_value     = false;
                GlMatrix matrix_f32mat = {};
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H */
