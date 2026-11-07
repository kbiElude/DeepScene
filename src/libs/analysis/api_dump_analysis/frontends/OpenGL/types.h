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
            /* What a vertex of a vertex call is made of. */
            enum class GlVertexComponent
            {
                DOUBLE,
                FLOAT,
                INT,
                SHORT
            };

            /* The values of the fixed-function pipeline that a draw is told apart by, besides what is enabled (the setting is what the call said:
             * a function, a mode, the mask of the colors). */
            enum class GlStateValue
            {
                ALPHA_FUNC,
                BLEND_DESTINATION,
                BLEND_SOURCE,
                COLOR_MASK,
                CULL_FACE,
                DEPTH_FUNC,
                DEPTH_MASK,
                POLYGON_MODE_BACK,
                POLYGON_MODE_FRONT,
                SHADE_MODEL
            };

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

            /* The state that glPushAttrib saves and glPopAttrib puts back, as far as the front end follows it besides the pipeline state (the
             * matrices are not a part of it: they have stacks of their own). Follows GlBox.
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

            /* The vertices between a glBegin and its glEnd, which are one draw. The positions are kept only if the model builder asked for the
             * geometry of the draws (see ModelBuilder::wants_geometry).
             */
            struct GlImmediateBlock
            {
                bool                                 is_open         = false;
                uint32_t                             mode            = 0;
                uint32_t                             n_vertices      = 0;
                std::vector<std::array<float, 3> >   position_vec;
                bool                                 wants_positions = false;
            };

            /* How the vertices of a call are given: the number of their components, what a component is, and whether the call has a pointer to
             * them (the data of the call is then a chunk) or the components as its arguments. Follows GlVertexComponent.
             */
            struct GlVertexLayout
            {
                GlVertexComponent component    = GlVertexComponent::FLOAT;
                bool              is_vector    = false;
                uint32_t          n_components = 0;
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
