/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the OpenGL front end keeps of a context.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H

#include "analysis/api_dump_analysis/types.h"
#include <array>
#include <memory>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* What a draw from the memory of the application made of the call. */
            enum class GlArrayDrawResult
            {
                DRAW,      /* a draw to tell the model builder of                       */
                EMPTY,     /* a call that draws nothing (a count of 0)                  */
                UNREADABLE /* arguments or data that are not what the call has to have */
            };

            /* How a piece of data that draws read from the memory of the application is seen to be used again, which says how long it is worth keeping:
             * the bytes of a piece that is seen in one frame only are kept for that frame (the eyes of a stereo frame draw it each), and those of a
             * piece that is seen in more than one frame are kept on.
             */
            enum class GlBlobReuse
            {
                ACROSS_FRAMES,
                ONE_FRAME
            };

            /* What an array of vertex data is for; the values are those which the dumper stores (ClientArrayKind of the interception). */
            enum class GlClientArrayKind : uint32_t
            {
                COLOR           = 0,
                EDGE_FLAG       = 1,
                FOG_COORD       = 2,
                GENERIC         = 3,
                INDEX           = 4,
                NORMAL          = 5,
                SECONDARY_COLOR = 6,
                TEX_COORD       = 7,
                VERTEX          = 8
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

            /* What a vertex of a vertex call is made of. */
            enum class GlVertexComponent
            {
                DOUBLE,
                FLOAT,
                INT,
                SHORT
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

            /* A piece of data that draws read from the memory of the application. The bytes are there while the piece is worth keeping (see
             * GlBlobReuse).
             */
            struct GlBlob
            {
                std::shared_ptr<const std::vector<uint8_t> > data_ptr;                                                       /* null: the bytes are not kept  */
                uint64_t                                     last_frame  = 0;                                                /* the last frame it was seen in */
                uint32_t                                     n_frames    = 0;                                                /* the frames it was seen in     */
                Analysis::APIDumpAnalysis::ResourceId        resource_id = Analysis::APIDumpAnalysis::ResourceId::NONE;
            };

            /* An array of vertex data that a draw reads from the memory of the application, as the dumper copied it: the part of it that the draw
             * touches, from the element first_element on. Follows GlClientArrayKind.
             */
            struct GlClientArray
            {
                const uint8_t*    data_ptr      = nullptr; /* in the data of the call, n_bytes of it */
                uint32_t          divisor       = 0;
                uint32_t          first_element = 0;
                uint32_t          flags         = 0;
                uint32_t          index         = 0;
                GlClientArrayKind kind          = GlClientArrayKind::VERTEX;
                uint32_t          n_bytes       = 0;
                uint32_t          n_elements    = 0;
                uint64_t          pointer       = 0;
                int32_t           size          = 0;
                uint32_t          stride        = 0;       /* 0: the elements follow each other */
                uint32_t          type          = 0;       /* a GL type                          */
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
