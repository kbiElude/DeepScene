/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the OpenGL front end keeps for a context.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H

#include "analysis/api_dump_analysis/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include "common/api_function_argument.h"
#include <array>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* The result of handling a draw call that reads from the memory of the application. */
            enum class GLArrayDrawResult
            {
                DRAW,      /* a draw that the model builder is told about                   */
                EMPTY,     /* a call that draws nothing (a count of 0)                      */
                UNREADABLE /* the arguments or the data are not what the call requires      */
            };

            /* What an array of vertex data is for. The values are the ones the dumper stores (ClientArrayKind of the interception). */
            enum class GLClientArrayKind : uint32_t
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

            /* What a display list does to per-eye state (matrices, viewport and scissor pixels); decides whether it can be called for both eyes.
             *  - Only draws and sets state: the same for both eyes.
             *  - Changes a matrix relative to the current one: the same for both eyes, the eye's matrix passes through.
             *  - Stores something relative to the current matrix (see GLFunctionPolicy::bakes_current_matrix()): the same for both eyes
             *    if called once per eye.
             *  - Loads a matrix or sets a projection, viewport or scissor box: run once per eye, or replayed inline with those calls changed.
             */
            enum class GLListEyeDependence
            {
                EYE_DEPENDENT,
                INDEPENDENT,
                RELATIVE_MATRIX_ONLY
            };

            /* The fixed-function pipeline values that tell draws apart, besides what is enabled. The value of each is what the call set
             * (a function, a mode, the color mask). The numbers go into the keys of draws, so the entries keep their place and new ones are added at the end.
             */
            enum class GLStateValue
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
                SHADE_MODEL,

                FRONT_FACE,
                STENCIL_DEPTH_FAIL,
                STENCIL_DEPTH_PASS,
                STENCIL_FAIL,
                STENCIL_FUNC,
                STENCIL_REFERENCE,
                STENCIL_VALUE_MASK,
                STENCIL_WRITE_MASK
            };

            /* The type of the components of a vertex call. */
            enum class GLVertexComponent
            {
                DOUBLE,
                FLOAT,
                INT,
                SHORT
            };

            /* A 4x4 matrix in the OpenGL layout: the columns are stored one after the other (the elements 12 to 14 are the translation). */
            typedef std::array<float, 16> GLMatrix;

            /* A call stored in a display list, with its arguments. The chunks of data that the arguments refer to are in the list. */
            struct GLRecordedCall
            {
                std::vector<APIInterceptor::APIFunctionArgument> arg_in_vec;
                APIInterceptor::APIFunction                      func = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
            };

            /* A display list: the calls between glNewList and glEndList with their data (kept here because the original workload is released
             * before the list is called). Uses GLListEyeDependence and GLRecordedCall.
             */
            struct GLDisplayList
            {
                std::vector<GLRecordedCall>      call_vec;
                GLListEyeDependence              eye_dependence = GLListEyeDependence::INDEPENDENT;
                Capture::APIDumpLoader::Workload workload; /* the data that the calls refer to, under the numbers that the calls of the list use */
            };

            /* The display list that a context is making: the calls are stored in it, and also run if is_executing is set. Uses GLDisplayList. */
            struct GLListRecording
            {
                bool                           is_executing = false; /* GL_COMPILE_AND_EXECUTE */
                std::shared_ptr<GLDisplayList> list_ptr;             /* null: no list is being made */
                uint32_t                       list_number  = 0;
            };

            /* Upload byte layout (glPixelStorei): rows are row_length long (width if 0), each starting at a multiple of the alignment.
             * Client state: glPushClientAttrib saves it (with GL_CLIENT_PIXEL_STORE_BIT), and glPushAttrib does not.
             */
            struct GLPixelStore
            {
                uint32_t unpack_alignment  = 4;
                uint32_t unpack_row_length = 0;
            };

            /* A level of a texture, as given by the last glTexImage2D (or copy) that set it. */
            struct GLTextureLevel
            {
                uint32_t height          = 0;
                uint32_t internal_format = 0;
                uint32_t width           = 0;
            };

            /* A rectangle of a level, in texels from its lower left corner. */
            struct GLTextureRegion
            {
                uint32_t height = 0;
                uint32_t width  = 0;
                uint32_t x      = 0;
                uint32_t y      = 0;
            };

            /* A texture: its resource (known once glTexImage2D gives it a size) and its levels. */
            struct GLTexture
            {
                std::set<uint32_t>                    dirty_level_set;           /* render levels changed since last read                     */
                uint32_t                              height            = 0;
                uint32_t                              internal_format   = 0;
                std::map<uint32_t, GLTextureLevel>    level_map;
                std::set<uint32_t>                    render_level_set;          /* levels not in the workload (rendered or driver-generated) */
                std::map<uint32_t, GLTextureRegion>   render_region_map;         /* by level: smallest region holding all rendered content    */
                Analysis::APIDumpAnalysis::ResourceId resource_id       = Analysis::APIDumpAnalysis::ResourceId::NONE;
                uint32_t                              width             = 0;
            };

            /* How a texture's levels are generated (glTexParameter), stored by texture name; it may be set before the texture has a size
             * and survives a new size.
             */
            struct GLTextureParameters
            {
                bool     generates_mipmaps = false; /* GL_GENERATE_MIPMAP: first-level change regenerates lower levels    */
                uint32_t max_level         = 1000;  /* GL_TEXTURE_MAX_LEVEL: last generated level (GL default)            */
            };

            /* A rectangle of the framebuffer, in pixels from its lower left corner, as set by glViewport and glScissor. */
            struct GLBox
            {
                int32_t height = 0;
                int32_t width  = 0;
                int32_t x      = 0;
                int32_t y      = 0;
            };

            /* The state glPushAttrib saves, besides the pipeline state, as far as followed. Matrices have their own stacks. Uses GLBox. */
            struct GLAttributes
            {
                uint32_t                     active_texture_unit  = 0;        /* counted from 0; GL_TEXTURE0 is unit 0 */
                std::array<float, 4>         clear_color_fp32vec4 = {};
                double                       clear_depth          = 1.0;
                uint32_t                     clear_stencil        = 0;
                bool                         is_scissor_enabled   = false;
                uint32_t                     matrix_mode          = 0x1700;   /* GL_MODELVIEW */
                GLBox                        scissor_box;
                std::map<uint32_t, uint32_t> texture_binding_map;             /* GL_TEXTURE_2D binding by unit number */
                GLBox                        viewport;
            };

            /* Vertex data a draw reads from application memory, as copied by the dumper: the touched part, from first_element. Uses GLClientArrayKind. */
            struct GLClientArray
            {
                const uint8_t*    data_ptr      = nullptr; /* points into the data of the call; n_bytes long */
                uint32_t          divisor       = 0;
                uint32_t          first_element = 0;
                uint32_t          flags         = 0;
                uint32_t          index         = 0;
                GLClientArrayKind kind          = GLClientArrayKind::VERTEX;
                uint32_t          n_bytes       = 0;
                uint32_t          n_elements    = 0;
                uint64_t          pointer       = 0;
                int32_t           size          = 0;
                uint32_t          stride        = 0;       /* 0: the elements are tightly packed */
                uint32_t          type          = 0;       /* a GL type                          */
            };

            /* The vertices between glBegin and glEnd, one draw. Positions are kept only if ModelBuilder::wants_geometry. */
            struct GLImmediateBlock
            {
                uint64_t                             first_call_index = 0;     /* the glBegin */
                bool                                 is_open         = false;
                uint32_t                             mode            = 0;
                uint32_t                             n_vertices      = 0;
                std::vector<std::array<float, 3> >   position_vec;
                bool                                 wants_positions = false;
            };

            /* How a call passes vertices: components per vertex, component type, and pointer (data is a chunk) or arguments. Uses GLVertexComponent. */
            struct GLVertexLayout
            {
                GLVertexComponent component    = GLVertexComponent::FLOAT;
                bool              is_vector    = false;
                uint32_t          n_components = 0;
            };

            /* The last matrix reported to the model builder. It is reported again only when it differs. Uses GLMatrix. */
            struct GLReportedMatrix
            {
                bool     has_value     = false;
                GLMatrix matrix_f32mat = {};
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H */
