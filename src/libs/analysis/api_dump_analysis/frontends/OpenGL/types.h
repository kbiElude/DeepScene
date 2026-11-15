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

            /* Whether a piece of data that draws read from the memory of the application is used again, which decides how long it is kept. A piece
             * that is seen in one frame only is kept for that frame (the eyes of a stereo frame each draw it). A piece that is seen in more than
             * one frame is kept for the following frames as well.
             */
            enum class GLBlobReuse
            {
                ACROSS_FRAMES,
                ONE_FRAME
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

            /* What a display list does to the state that is specific to an eye (the matrices, and the pixels that the viewport and the scissor box
             * are expressed in). It decides whether a list that the driver made once can be called for both eyes of a stereo frame.
             *  - A list that only draws and sets state is the same for both eyes.
             *  - A list that changes a matrix relative to the current one (translation, rotation, scale, multiplication) is also the same for both
             *    eyes, because the matrix of the eye passes through it.
             *  - A list that stores something relative to the current matrix (the position of a light, a clip plane, texture coordinate planes, the
             *    raster position: see GLFunctionPolicy::bakes_current_matrix()) is also the same for both eyes, if it is called once for each eye:
             *    each eye then gets the result of its own matrix.
             *  - A list that loads a matrix, sets a projection, a viewport or a scissor box stores values of its own. It is run once for each eye,
             *    or replayed inline with those calls changed.
             */
            enum class GLListEyeDependence
            {
                EYE_DEPENDENT,
                INDEPENDENT,
                RELATIVE_MATRIX_ONLY
            };

            /* The fixed-function pipeline values that tell draws apart, besides what is enabled. The value of each is what the call set
             * (a function, a mode, the color mask). */
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
                SHADE_MODEL
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

            /* A display list: the calls between its glNewList and glEndList, and the data of those calls. The data is not part of the workload in
             * which the calls were made, because that workload is released before the list is called. Uses GLListEyeDependence and GLRecordedCall.
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

            /* How the program lays out the bytes of an upload (glPixelStorei). A row is as long as the row length (the width if the row length is 0),
             * and each row starts at a multiple of the alignment. This is client state: glPushAttrib does not save it.
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

            /* A texture of the program: its resource (known once the first glTexImage2D gives it a size), and the levels it has been given. */
            struct GLTexture
            {
                std::set<uint32_t>                    dirty_level_set;           /* the levels of render_level_set that changed since something read them (or since they were given) */
                uint32_t                              height            = 0;
                uint32_t                              internal_format   = 0;
                std::map<uint32_t, GLTextureLevel>    level_map;
                std::set<uint32_t>                    render_level_set;          /* the levels whose content is not in the workload (rendered, or generated by the driver)             */
                std::map<uint32_t, GLTextureRegion>   render_region_map;         /* by level: the smallest region that holds everything rendered into the level since it was given     */
                Analysis::APIDumpAnalysis::ResourceId resource_id       = Analysis::APIDumpAnalysis::ResourceId::NONE;
                uint32_t                              width             = 0;
            };

            /* What the program set about how the levels of a texture are generated (glTexParameter), stored under the texture name. The program
             * sets it before the texture has a size, and it stays when the texture is given a new one.
             */
            struct GLTextureParameters
            {
                bool     generates_mipmaps = false; /* GL_GENERATE_MIPMAP: a change of the first level regenerates the levels below it                  */
                uint32_t max_level         = 1000;  /* GL_TEXTURE_MAX_LEVEL, with the OpenGL default: the last level that generation produces          */
            };

            /* A rectangle of the framebuffer, in pixels from its lower left corner, as set by glViewport and glScissor. */
            struct GLBox
            {
                int32_t height = 0;
                int32_t width  = 0;
                int32_t x      = 0;
                int32_t y      = 0;
            };

            /* The state that glPushAttrib saves and glPopAttrib restores, as far as the front end follows it besides the pipeline state. The
             * matrices are not part of it: they have stacks of their own. Uses GLBox.
             */
            struct GLAttributes
            {
                uint32_t                     active_texture_unit  = 0;        /* counted from 0; GL_TEXTURE0 is unit 0 */
                std::array<float, 4>         clear_color_fp32vec4 = {};
                double                       clear_depth          = 1.0;
                uint32_t                     clear_stencil        = 0;
                bool                         is_scissor_enabled   = false;
                uint32_t                     matrix_mode          = 0x1700;   /* GL_MODELVIEW */
                GLBox                        scissor_box;
                std::map<uint32_t, uint32_t> texture_binding_map;             /* the GL_TEXTURE_2D texture bound to each texture unit, by unit number */
                GLBox                        viewport;
            };

            /* A piece of data that draws read from the memory of the application. The bytes are kept while the piece is worth keeping
             * (see GLBlobReuse).
             */
            struct GLBlob
            {
                std::shared_ptr<const std::vector<uint8_t> > data_ptr;                                                       /* null: the bytes are not kept         */
                uint64_t                                     last_frame  = 0;                                                /* the last frame that used the piece   */
                uint32_t                                     n_frames    = 0;                                                /* the number of frames that used it    */
                Analysis::APIDumpAnalysis::ResourceId        resource_id = Analysis::APIDumpAnalysis::ResourceId::NONE;
            };

            /* An array of vertex data that a draw reads from the memory of the application, as copied by the dumper. It holds the part of the array
             * that the draw touches, starting at the element first_element. Uses GLClientArrayKind.
             */
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

            /* The vertices between a glBegin and its glEnd. Together they are one draw. The positions are kept only if the model builder asked for
             * the geometry of draws (see ModelBuilder::wants_geometry).
             */
            struct GLImmediateBlock
            {
                uint64_t                             first_call_index = 0;     /* the glBegin */
                bool                                 is_open         = false;
                uint32_t                             mode            = 0;
                uint32_t                             n_vertices      = 0;
                std::vector<std::array<float, 3> >   position_vec;
                bool                                 wants_positions = false;
            };

            /* How the vertices of a call are passed: the number of components per vertex, the type of a component, and whether the call passes a
             * pointer to the vertices (the data of the call is then a chunk) or passes the components as arguments. Uses GLVertexComponent.
             */
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
