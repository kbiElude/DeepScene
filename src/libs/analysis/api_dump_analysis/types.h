/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The vocabulary of the analysis of a workload: what the front ends of the APIs report to the model builder, and what ends up in the
 * analysis file. Nothing here knows about a particular API. A value that comes from an API (a format, creation flags) is kept as the API
 * wrote it, together with its API-neutral meaning.
 */
#ifndef APIDUMPANALYSIS_TYPES_H
#define APIDUMPANALYSIS_TYPES_H

#include "analysis/api_dump_analysis/depth_histogram.h"
#include "capture/api_dump_loader/api_info.h"
#include <array>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* How much of the part of a resource that an access reaches is overwritten by a write (see AccessEvent). It decides whether the old
         * content survives the write: it survives wherever the write did not reach.
         */
        enum class AccessCoverage
        {
            FULL,    /* all of it                                                                   */
            PARTIAL, /* a part of it: a rectangle, a viewport, a mask                               */
            UNKNOWN  /* the front end cannot tell (the extent of a draw, say); treated as PARTIAL   */
        };

        /* What an access does to the content of the resource. A write that blends, tests or masks is READ_WRITE, because the old content is an input. */
        enum class AccessMode
        {
            READ,
            READ_WRITE,
            WRITE
        };

        /* What the resource is used as in an access: the API-neutral meaning of a binding, a copy or a clear (see AccessEvent). */
        enum class AccessRole
        {
            CLEAR_TARGET,
            COLOR_ATTACHMENT,
            DEPTH_STENCIL_ATTACHMENT,
            INDEX_INPUT,
            SAMPLED,                  /* read by a shader, or as a texture by an API without shaders       */
            SHADER_CODE,
            TRANSFER_DESTINATION,
            TRANSFER_SOURCE,
            UPLOAD_DESTINATION,       /* written with data from the application                            */
            VERTEX_INPUT
        };

        /* Where a constant lives. */
        enum class ConstantBlockKind
        {
            BUFFER,               /* a buffer resource (a uniform or constant buffer)                                      */
            DEFAULT_UNIFORMS,     /* loose constants of a shader or program: constant registers, the default uniform block */
            FIXED_FUNCTION_STATE, /* state the API keeps outside any shader (transforms, lights, materials)                 */
            PUSH_CONSTANTS
        };

        enum class ConstantProvenance
        {
            API_DEFINED, /* the API says what the constant is (e.g. a D3D8 view transform) */
            INFERRED     /* the meaning was inferred; the API does not define it           */
        };

        enum class ConstantSemantic
        {
            MODEL_MATRIX,                 /* object space to world space */
            MODEL_VIEW_MATRIX,
            MODEL_VIEW_PROJECTION_MATRIX,
            PROJECTION_MATRIX,
            VIEW_MATRIX,
            VIEW_PROJECTION_MATRIX,

            UNKNOWN
        };

        /* The exit code of a program that reads a workload and writes a file with the results. */
        enum class ExtractorExitCode
        {
            BAD_ARGUMENTS        = 1,
            CANNOT_READ_WORKLOAD = 2,
            CANNOT_WRITE_OUTPUT  = 3,
            RESULT_IS_INCOMPLETE = 4, /* the workload was read, but not everything that the program looks for could be kept */
            SUCCESS              = 0
        };

        enum class FormatClass
        {
            COLOR,
            COMPRESSED,
            DEPTH,
            DEPTH_STENCIL,

            UNKNOWN
        };

        enum class PrimitiveTopology
        {
            LINE_LIST,
            LINE_STRIP,
            POINT_LIST,
            TRIANGLE_FAN,
            TRIANGLE_LIST,
            TRIANGLE_STRIP,

            UNKNOWN
        };

        /* The exit code of a program that queries a file of the analysis. */
        enum class QueryExitCode
        {
            BAD_ARGUMENTS     = 1,
            CANNOT_READ_FILE  = 2,
            NOT_FOUND         = 3,
            SUCCESS           = 0,
            WORKLOAD_MISMATCH = 4
        };

        /* How a query ended. */
        enum class QueryStatus
        {
            BAD_ARGUMENTS,
            NOT_FOUND,
            OK
        };

        /* Which representative frame of a shape to use. */
        enum class RepresentativeChoice
        {
            FIRST,
            HEAVIEST,
            LIGHTEST
        };

        /* What an application does that depends on the size of its render target in pixels. Each kind is counted when a workload is analyzed
         * (see WorkloadSummary), so that it is known when the application cannot safely be made to render larger than it asks for.
         */
        enum class ResolutionHazard
        {
            CLEARS_WITH_RECTANGLES, /* a clear of parts of a target (rectangles are in pixels)      */
            COPIES_WITH_RECTANGLES, /* a copy of parts of a surface (rectangles are in pixels)      */
            LINE_WIDTH_STATES,      /* the width of lines, which is in pixels                       */
            PIXEL_READBACKS,        /* a read of pixels of a surface, at positions in pixels        */
            POINT_SIZE_STATES,      /* the size of points, which is in pixels                       */
            SCISSOR_BOXES,          /* a scissor box, which is in pixels                            */
            SCREEN_SPACE_VERTICES,  /* vertices that are positioned in pixels                       */
            SHADER_CREATIONS,       /* shaders, whose constants may hold the size of the screen     */
            SIZE_QUERIES,           /* the application asks how large the viewport or a surface is  */

            /* The values are stored in files by number, so new kinds are added at the end. */
            RASTER_OPERATIONS,                /* pixels or bitmaps that are drawn at positions in pixels, or zoomed       */
            TEXTURE_UPLOADS_TO_RENDER_TARGETS /* pixels given to a texture that has the size of the render target         */
        };

        /* How the frames of a workload use a resource (see ResourceAccess). */
        enum class ResourceAccessClass
        {
            FRAME_LOCAL,  /* written before each read, in every frame: a frame can be played twice                          */
            LOOP_CARRIED, /* read before it is written, in a frame that also writes it: holds the result of the previous frame */
            STATIC,       /* written only up to the first frame that reads it                                      */
            UNREAD,       /* written but never read (the back buffer, say)                                         */
            UNUSED        /* no frame touches it                                                                   */
        };

        /* Resources are numbered from 1 in the order the model builder meets them; the number says nothing else. */
        enum class ResourceId : uint32_t
        {
            NONE = 0
        };

        enum class ResourceKind
        {
            BUFFER_INDEX,
            BUFFER_VERTEX,
            SHADER,
            SWAP_CHAIN_IMAGE,
            TEXTURE_2D,
            TEXTURE_3D,
            TEXTURE_CUBE,

            UNKNOWN
        };

        enum class ShaderStage
        {
            COMPUTE,
            FRAGMENT,
            GEOMETRY,
            TESS_CONTROL,
            TESS_EVALUATION,
            VERTEX,

            UNKNOWN
        };

        /* How much of the picture the vertices of a draw cover. */
        enum class ViewCoverage
        {
            COVERING, /* the vertices reach all four edges                                  */
            PARTIAL,  /* the vertices were examined and do not reach all four edges         */
            UNKNOWN   /* there were no vertices to examine                                  */
        };

        /* Whether a workload file is the one that an analysis was made from (see WorkloadIdentity::verify). */
        enum class WorkloadMatch
        {
            MATCHES,
            MISMATCH,
            UNREADABLE /* the file cannot be opened or read, so nothing can be said */
        };

        enum class TransferKind
        {
            COPY,  /* from one resource to another                  */
            UPLOAD /* from the application's memory into a resource */
        };


        /* Where a resource is bound, in the terms of the API: a stage or a texture unit; a set, a binding and an array element; or a
         * register space, a register and an array element. Whatever the API does not have is 0.
         */
        struct BindingPoint
        {
            uint32_t element = 0;
            uint32_t group   = 0;
            uint32_t index   = 0;
        };

        struct ClearOperation
        {
            bool                 clears_color           = false;
            bool                 clears_depth           = false;
            bool                 clears_stencil         = false;
            std::array<float, 4> color_fp32vec4         = {};
            float                depth                  = 0.0f;
            bool                 has_rectangles         = false; /* the clear covers rectangles (or a scissor), not the whole target     */
            bool                 is_limited_to_viewport = false; /* the API clears only the viewport part of the target                  */
            uint32_t             stencil                = 0;
        };

        /* A range of bytes in a block of constants. Blocks that are not backed by memory visible to the application (state kept outside
         * shaders, constant registers) have a layout that the front end defines. Uses ConstantBlockKind and ResourceId.
         */
        struct ConstantLocation
        {
            ResourceId        block_id    = ResourceId::NONE; /* the buffer, or the shader that owns the default uniforms; otherwise NONE */
            ConstantBlockKind block_kind  = ConstantBlockKind::BUFFER;
            uint64_t          byte_offset = 0;
            uint64_t          byte_size   = 0;
        };

        /* Identifies the constant that a time series belongs to: the number of the pass structure, and the index of the constant in the
         * constant_signature_vec of that structure.
         */
        typedef std::pair<uint32_t, uint32_t> ConstantSeriesKey;

        /* How wide the picture is for a camera: the x scale of the perspective projection of the draws (1 divided by the tangent of half the
         * horizontal angle of view), averaged over the draws of a pass in one kind of frame and weighted by their vertices. The projection of a
         * pass can change from frame to frame (a zoom), so the projection of the first frame is only one of several values.
         */
        struct ProjectionScale
        {
            float    mean   = 0.0f;
            uint64_t weight = 0;
        };

        /* Identifies a depth histogram: the number of the frame shape and the number of the pass structure (a pass in one kind of frame). */
        typedef std::pair<uint32_t, uint32_t> DepthHistogramKey;

        /* What a pass found out about how deep its draws are: the depth of the origin of each object, the depth of the vertices (the
         * surfaces), and how wide the picture is for the camera. See DrawMeasurer.
         */
        struct PassDepth
        {
            DepthHistogram  depth_histogram;
            ProjectionScale projection_scale;
            DepthHistogram  surface_depth_histogram;
        };

        /* A constant as used to tell passes apart: where it is and what it is, not its value. The block is a key, not an ID. Uses
         * ConstantBlockKind, ConstantProvenance and ConstantSemantic.
         */
        struct ConstantSignature
        {
            uint64_t           block_key   = 0;
            ConstantBlockKind  block_kind  = ConstantBlockKind::BUFFER;
            uint64_t           byte_offset = 0;
            uint64_t           byte_size   = 0;
            ConstantProvenance provenance  = ConstantProvenance::API_DEFINED;
            ConstantSemantic   semantic    = ConstantSemantic::UNKNOWN;
        };

        /* Uses ConstantLocation, ConstantProvenance and ConstantSemantic. */
        struct ConstantUpdate
        {
            ConstantLocation     location;
            ConstantProvenance   provenance = ConstantProvenance::API_DEFINED;
            ConstantSemantic     semantic   = ConstantSemantic::UNKNOWN;
            std::vector<uint8_t> value_u8_vec;                                  /* as the API stores it */
        };

        /* Where a constant is kept in the state of a draw: the kind of block, the block, and the position in it (byte offset and size). */
        typedef std::tuple<ConstantBlockKind, uint32_t, uint64_t, uint64_t> ConstantStateKey;

        /* The last value given to a constant that has a known meaning, and the key of the block that contains it at that time. */
        struct ConstantState
        {
            uint64_t       block_key = 0;
            ConstantUpdate update;
        };

        typedef std::map<ConstantStateKey, ConstantState> ConstantStateMap;

        /* Where an item is stored in the workload, so that it does not have to be copied. */
        struct DataReference
        {
            uint64_t content_hash  = 0;
            uint32_t data_chunk_id = 0;
            uint64_t n_bytes       = 0;
        };

        /* raw_value is meaningful only to the front end that wrote it; format_class is its API-neutral meaning. */
        struct Format
        {
            FormatClass format_class = FormatClass::UNKNOWN;
            uint32_t    raw_value    = 0;
        };

        /* A resource, or a part of it (a mip level, an array layer). */
        struct ResourceView
        {
            uint32_t   layer       = 0;
            uint32_t   mip_level   = 0;
            ResourceId resource_id = ResourceId::NONE;
        };


        /* The mip levels and array layers (or slices, or faces) of a resource that an access reaches. A count of 0 means all of them from the
         * first one on, so the default range is the whole resource. The front end of an API that has neither can leave it at the default.
         */
        struct SubresourceRange
        {
            uint32_t first_layer     = 0;
            uint32_t first_mip_level = 0;
            uint32_t n_layers        = 0;
            uint32_t n_mip_levels    = 0;
        };

        /* Where a write lands in the first mip level of a layer of a resource, in texels (or pixels), if the front end can tell: a rectangle
         * with its corner at x and y. A write that is not reported as a rectangle may reach any part of the level, as far as anyone knows.
         */
        struct AccessRectangle
        {
            uint32_t height   = 0;
            bool     is_known = false;
            uint32_t width    = 0;
            uint32_t x        = 0;
            uint32_t y        = 0;
        };

        /* One use of a resource by the application, in the order in which the uses happened (see IAccessSink). The dependencies between the
         * frames of a workload are computed from these events. Nothing here knows about a particular API: a front end reports a draw, a clear
         * or a copy in these terms. An API with more to say (the queue that a command runs on, a barrier between two commands) has fields for
         * it that the other APIs leave at their defaults. Uses AccessCoverage, AccessMode, AccessRectangle, AccessRole, ResourceId and
         * SubresourceRange, which it depends on.
         */
        struct AccessEvent
        {
            uint64_t         call_index        = 0;
            AccessCoverage   coverage          = AccessCoverage::UNKNOWN; /* of a write: READ_WRITE and WRITE                  */
            AccessMode       mode              = AccessMode::READ;
            uint32_t         queue_id          = 0;                       /* 0 for an API that has one queue                   */
            AccessRectangle  rectangle;                                   /* of a write; not known unless the front end says   */
            ResourceId       resource_id       = ResourceId::NONE;
            AccessRole       role              = AccessRole::SAMPLED;
            SubresourceRange subresource_range;
        };

        /* What is rendered to. Follows ResourceView, which it depends on. */
        struct AttachmentSet
        {
            std::vector<ResourceView> color_view_vec;
            ResourceView              depth_stencil_view; /* resource_id is NONE if there is none */
        };

        /* What the frames of a workload did with a resource, as numbers of frames. A frame reads a resource when a draw samples or otherwise
         * reads it, or when a copy takes it as the source. A frame writes it when a draw renders to it, or when a clear, upload or copy targets
         * it. A frame is loop carried for the resource when it reads the resource before writing it, and also writes it.
         */
        struct ResourceAccess
        {
            uint64_t first_read_frame_index  = 0; /* valid if n_frames_read > 0                                   */
            uint64_t first_write_frame_index = 0; /* valid if n_frames_written > 0                                */
            uint64_t last_write_frame_index  = 0; /* valid if n_frames_written > 0                                */
            uint64_t n_frames_loop_carried   = 0; /* the frames that read it before writing it, and wrote it      */
            uint64_t n_frames_read           = 0;
            uint64_t n_frames_written        = 0;
        };

        /* Follows Format, ResourceKind and ShaderStage, which it depends on. */
        struct ResourceDescription
        {
            uint64_t     api_flags             = 0; /* what the front end considers worth telling resources apart by (pool, creation flags) */
            uint64_t     content_hash          = 0; /* of the shader code, or of the data that an immutable resource starts with           */
            uint32_t     depth_or_layer_count  = 1;
            Format       format;
            bool         has_content_hash      = false;
            uint32_t     height                = 0;
            bool         is_application_memory = false; /* a copy of the application's memory (a client array): identified by its kind, not by its bytes or size */
            bool         is_depth_stencil      = false;
            bool         is_dynamic            = false;
            bool         is_render_target      = false;
            ResourceKind kind                  = ResourceKind::UNKNOWN;
            uint32_t     mip_level_count       = 1;
            uint32_t     sample_count          = 1;
            ShaderStage  shader_stage          = ShaderStage::UNKNOWN;
            uint64_t     size_in_bytes         = 0; /* of a buffer */
            uint32_t     width                 = 0;
        };

        /* A resource as the model builder knows it. Follows ResourceDescription and ResourceView. */
        struct Resource
        {
            uint64_t            created_at_call_index   = 0;
            std::string         debug_name;                  /* for people; never part of an identity                                  */
            ResourceDescription description;
            uint64_t            destroyed_at_call_index = 0;
            ResourceId          id                      = ResourceId::NONE;
            bool                is_destroyed            = false;
            uint64_t            match_key               = 0;
            ResourceView        parent_view;                 /* resource_id is NONE if the resource is not part of another one          */
        };

        /* Follows BindingPoint and ResourceView, which it depends on. */
        struct TextureBinding
        {
            BindingPoint binding_point;
            ResourceView view;
        };

        /* A texture binding as used to tell draws apart. Uses BindingPoint, which it depends on. */
        struct TextureBindingKey
        {
            BindingPoint binding_point;
            uint64_t     view_key = 0;
        };

        /* Some of the vertices of a draw, in model space: the surface that the draw draws, as far as the analysis needs it (see
         * DrawMeasurer::record_draw_surface_depth). It is not the whole geometry, because a draw can have a million vertices.
         */
        struct GeometrySample
        {
            /* The vertices are already in picture space (pre-transformed) and no matrix moves them. The positions are then the x and y of a
             * vertex in pixels, with the reciprocal of its depth in place of the third coordinate (see DrawMeasurer::record_screen_depth).
             */
            bool                               is_pre_transformed = false;
            std::vector<std::array<float, 3> > position_vec;
        };

        /* One draw, with everything that the front end has resolved by the time the draw is made. Uses GeometrySample and TextureBinding. */
        struct DrawDescription
        {
            uint64_t                              first_call_index        = UINT64_MAX;                /* the first call of the draw, if the draw consists of more calls than its own (glBegin, for a block that glEnd ends); UINT64_MAX: none */
            std::shared_ptr<const GeometrySample> geometry_ptr;                                        /* null: the front end does not know the vertices */
            ResourceId                            index_buffer_id         = ResourceId::NONE;
            uint32_t                              instance_count          = 1;
            bool                                  is_camera_right_handed  = false;                     /* the camera looks down minus z (OpenGL): the depth of a vertex is minus its z in camera space, and the projection is the OpenGL one */
            bool                                  is_indexed              = false;
            PrimitiveTopology                     primitive_topology      = PrimitiveTopology::UNKNOWN;
            bool                                  reads_color_attachments = false;                     /* blending: the existing color is an input     */
            bool                                  reads_depth_stencil     = false;                     /* depth or stencil test is on                  */
            std::vector<ResourceId>               shader_id_vec;                                       /* empty: the draw uses no shader               */
            uint64_t                              state_key               = 0;                         /* of all the state that the draw needs         */
            std::vector<TextureBinding>           texture_binding_vec;
            std::vector<ResourceId>               vertex_buffer_id_vec;
            uint32_t                              vertex_count            = 0;                         /* the number of indices, if the draw is indexed */
            uint64_t                              vertex_layout_key       = 0;
            uint64_t                              view_anchor_call_index  = UINT64_MAX;                /* the call that starts the camera of the draw. A player can change it to move the camera for an eye (OpenGL: the load of the model-view matrix that the matrix of the draw derives from, a glLoadIdentity or glLoadMatrix); UINT64_MAX: none */
        };

        struct Viewport
        {
            float height    = 0.0f;
            float max_depth = 1.0f;
            float min_depth = 0.0f;
            float width     = 0.0f;
            float x         = 0.0f;
            float y         = 0.0f;
        };

        /* The matrices that a draw is made with, as given by the constants. */
        struct DrawMatrices
        {
            bool                  has_projection   = false;
            bool                  has_view         = false;
            bool                  is_orthographic  = false; /* far things are not smaller in the projection */
            bool                  is_perspective   = false; /* the projection divides by the depth          */
            std::array<float, 16> model_f32mat     = {1.0f, 0.0f, 0.0f, 0.0f,
                                                      0.0f, 1.0f, 0.0f, 0.0f,
                                                      0.0f, 0.0f, 1.0f, 0.0f,
                                                      0.0f, 0.0f, 0.0f, 1.0f};
            std::array<float, 16> projection_f32mat = {};
            std::array<float, 16> view_f32mat       = {};
        };

        /* A draw as used to tell passes apart: the resources are represented by their keys, and the counts are left out. Uses
         * PrimitiveTopology and TextureBindingKey.
         */
        struct DrawStructure
        {
            uint64_t                       index_buffer_key        = 0;
            bool                           is_indexed              = false;
            bool                           is_instanced            = false;
            bool                           is_orthographic         = false; /* drawn with a projection in which far things are not smaller: a good sign of a 2D overlay, but not a sure one */
            PrimitiveTopology              primitive_topology      = PrimitiveTopology::UNKNOWN;
            std::vector<uint64_t>          shader_key_vec;
            uint64_t                       state_key               = 0;
            std::vector<TextureBindingKey> texture_binding_key_vec;
            std::vector<uint64_t>          vertex_buffer_key_vec;
            uint64_t                       vertex_layout_key       = 0;
        };

        /* The key of the structure of a draw, and the key that tells apart the draws of one signature by their size
         * (MatchKeyBuilder::compute_draw_cover_key). The quads of a text and the quads of a backdrop have one structure but are different kinds of draw.
         */
        typedef std::pair<uint64_t, uint64_t> DrawFactsKey;

        /* What the draws of one structure did, over all the passes they were made in. It tells an overlay that has content of its own from one
         * that works on the picture below it. The footprint is in picture units (the edges are at -1 and 1) and covers the vertices that were
         * sampled. The view depth is the z of a vertex in camera space (for an orthographic projection, it tells which layer the draw is on).
         */
        struct DrawFacts
        {
            float    area_fraction_max              = 0.0f; /* the largest part of the picture that the box around the vertices of one draw covers (1: all of it)     */
            float    area_fraction_min              = 0.0f; /* the smallest part (a structure can be a sprite in some draws and a backdrop in others)                  */
            uint64_t n_blended_draws                = 0;    /* the draws that read the existing color (blending)                                                       */
            uint64_t n_draws                        = 0;
            uint64_t n_draws_after_3d_draw          = 0;    /* the draws after a perspective draw in the same frame                                                    */
            uint64_t n_draws_eye_space              = 0;    /* the draws with the origin at the camera and the vertices in front of it                                 */
            uint64_t n_draws_flat_layer             = 0;    /* the draws in pixels, or orthographic                                                                    */
            uint64_t n_draws_sampling_copied_color  = 0;    /* the draws that sample a resource that a copy of the picture filled                                      */
            uint64_t n_draws_with_footprint         = 0;    /* the draws whose sampled vertices could be placed in the picture                                         */
            float    view_depth_max                 = 0.0f;
            float    view_depth_min                 = 0.0f;
            float    x_max                          = 0.0f;
            float    x_min                          = 0.0f;
            float    y_max                          = 0.0f;
            float    y_min                          = 0.0f;
        };

        /* A draw of a pass: the key of its structure, and the key by which the draw is recognized at the moment it is issued (see
         * MatchKeyBuilder::compute_draw_signature_key).
         */
        struct DrawSignature
        {
            uint64_t signature_key = 0;
            uint64_t structure_key = 0;
        };

        /* One occurrence of a pass in a frame, with its size. A pass owns the calls from after the last draw (or clear) of the previous pass up
         * to its own last draw, since state set before a draw is meant for that draw. end_call_index is the call that closed the pass: it
         * belongs to the next pass, or is the present.
         */
        struct PassInstance
        {
            uint64_t begin_call_index     = 0;
            uint64_t begin_timestamp_qpc  = 0;
            uint64_t end_call_index       = 0;
            uint64_t last_draw_call_index = 0; /* the last draw, or the clear if there are no draws          */
            uint64_t n_draws              = 0;
            uint64_t n_vertices           = 0;
            uint32_t structure_id         = 0; /* of a PassStructure */
        };

        /* What a pass is, regardless of its size: where it renders to and how, and which kinds of draws it consists of. A match key
         * recognizes it in another run. Uses ConstantSignature and Viewport.
         */
        struct PassStructure
        {
            bool                           clears_color           = false;
            bool                           clears_depth           = false;
            bool                           clears_stencil         = false;
            std::vector<uint64_t>          color_view_key_vec;
            std::vector<ConstantSignature> constant_signature_vec;                 /* sorted            */
            uint64_t                       depth_stencil_view_key = 0;
            std::vector<DrawSignature>     draw_signature_vec;                     /* sorted by structure key, each once      */
            std::vector<uint64_t>          draw_structure_key_vec;                 /* sorted, each once */
            uint64_t                       key                    = 0;
            Viewport                       viewport;
        };

        /* A copy or an upload, which happens between passes. Uses TransferKind, ResourceView and DataReference. */
        struct TransferRecord
        {
            uint64_t      call_index  = 0;
            DataReference data;                            /* of an upload */
            ResourceView  destination;
            TransferKind  kind        = TransferKind::COPY;
            ResourceView  source;                          /* of a copy    */
        };

        /* What a frame consisted of. Uses PassInstance and TransferRecord. */
        struct FrameDetail
        {
            std::vector<PassInstance>   pass_instance_vec;
            std::vector<TransferRecord> transfer_record_vec;
        };

        /* A frame of the capture. */
        struct FrameRecord
        {
            uint64_t begin_timestamp_qpc = 0;
            uint64_t end_timestamp_qpc   = 0;
            uint64_t first_call_index    = 0;
            uint64_t frame_index         = 0;
            uint64_t last_call_index     = 0;
            uint64_t n_draws             = 0;
            uint32_t n_passes            = 0;
            uint32_t shape_id            = 0; /* of a FrameShape */
        };

        /* What identifies a draw for a player that classifies draws when they are issued: its signature key, and the key of the signature combined with the size of the draw. */
        struct DrawKeys
        {
            uint64_t cover_key     = 0;
            uint64_t signature_key = 0;
        };

        /* One draw of a frame: where it is, and the key of its structure (see DrawStructure), which is the same in every run. */
        struct DrawInstance
        {
            uint64_t call_index    = 0;
            uint64_t cover_key     = 0; /* of the draw's signature and size (see MatchKeyBuilder::compute_draw_cover_key) */
            uint64_t first_call_index = 0; /* the first call of the draw: call_index, unless its calls form a block (glBegin to glEnd). A draw that is left out is left out as a whole block */
            uint32_t pass_index    = 0; /* a position in CompletedFrame::pass_instance_vec */
            uint64_t structure_key = 0;
            uint64_t view_anchor_call_index = UINT64_MAX; /* of the draw: see DrawDescription::view_anchor_call_index */
        };

        /* A frame as the model builder saw it when it completed, with what a player needs. The call indices are the ones given to
         * ModelBuilder::begin_call; the passes are in the order in which they were made.
         */
        struct CompletedFrame
        {
            std::vector<DrawInstance> draw_instance_vec; /* in call order */
            uint64_t                  first_call_index = 0;
            uint64_t                  frame_index      = 0; /* counted from 0 */
            uint64_t                  last_call_index  = 0; /* the call of the present */
            std::vector<ResourceId>   loop_carried_resource_id_vec; /* resources that the frame reads before it writes, and also writes */
            std::vector<PassInstance> pass_instance_vec;
            std::vector<bool>         pass_is_presented_vec; /* whether each pass above renders to what is presented */
            std::vector<uint64_t>     pass_key_vec;         /* match key of each pass above                             */
            uint64_t                  shape_key        = 0; /* match key of the frame's shape                           */
        };

        /* What the model builder has collected so far for the frame that is not over. It is read when the frame is presented, and then
         * emptied. The vectors that have an entry for each pass are in the order of pass_instance_vec.
         */
        struct FrameInProgress
        {
            uint64_t                     begin_timestamp_qpc = 0;
            std::vector<DepthHistogram>  depth_histogram_vec;
            std::vector<DrawInstance>    draw_instance_vec;
            uint64_t                     first_call_index    = 0;
            bool                         has_calls           = false; /* a call has arrived since the last present */
            bool                         has_seen_3d_draw    = false; /* a perspective draw was made this frame */
            std::vector<PassInstance>    pass_instance_vec;
            std::vector<bool>            pass_is_presented_vec;
            std::vector<ProjectionScale> projection_scale_vec;
            std::vector<DepthHistogram>  surface_depth_histogram_vec;
            std::vector<TransferRecord>  transfer_record_vec;
        };

        /* A pass until it ends. */
        struct OpenPass
        {
            AttachmentSet                attachments;
            uint64_t                     begin_call_index     = 0;
            uint64_t                     begin_timestamp_qpc  = 0;
            bool                         clears_color         = false;
            bool                         clears_depth         = false;
            bool                         clears_stencil       = false;
            std::set<uint64_t>           covering_draw_key_set;        /* keys of the draws of the pass that cover the view */
            PassDepth                    depth;
            std::map<uint64_t, uint64_t> draw_signature_map;           /* key of the draw structure -> key of its signature */
            bool                         has_draws            = false;
            uint64_t                     last_draw_call_index = 0;     /* last draw, or the clear if there is none */
            uint64_t                     n_draws              = 0;
            uint64_t                     n_vertices           = 0;
            Viewport                     viewport;
        };

        /* The pass structures that make up a frame. The order is ignored, because independent passes can happen in any order. */
        struct FrameShape
        {
            std::vector<uint32_t> pass_structure_id_vec; /* sorted */
        };

        /* How deep the vertices of a pass are, over all the kinds of frames that the pass occurs in: the depths below which a tenth, a half and
         * nine tenths of the vertices lie. The vertices are those of the surfaces if the analysis could read them, otherwise the origins of the
         * objects. A pass of a 2D overlay has all of them at one depth; the pass of a 3D scene seen from one place does not.
         */
        struct PassDepthSpread
        {
            float    depth_far           = 0.0f;
            float    depth_median        = 0.0f;
            float    depth_near          = 0.0f;
            float    far_to_near_ratio   = 0.0f; /* 0 if there is no depth */
            bool     has_depth           = false;
            bool     is_flat             = false; /* the far depth is at most 10% above the near depth: a range of the histogram, or the next one */
            uint64_t n_vertices          = 0;
        };

        /* How often a pass structure occurs. */
        struct PassUsage
        {
            uint64_t n_frames          = 0; /* frames with at least one instance */
            uint64_t n_instances       = 0;
            uint32_t pass_structure_id = 0;
        };

        /* Consecutive frames of one shape. Times are in seconds since the first call of the workload. */
        struct Run
        {
            double   begin_seconds     = 0.0;
            double   end_seconds       = 0.0;
            uint64_t first_frame_index = 0;
            double   fps               = 0.0; /* 0 if the run took no time */
            uint64_t n_draws           = 0;
            uint64_t n_frames          = 0;
            uint32_t shape_id          = 0;
        };

        /* When and how often a frame shape occurs. Times are in seconds since the first call of the workload. */
        struct ShapeUsage
        {
            double   first_seconds = 0.0;
            double   last_seconds  = 0.0;
            uint64_t n_frames      = 0;
            uint64_t n_runs        = 0;
            uint32_t shape_id      = 0;
        };

        /* The frames of a workload in time order: what ran when, and how often. */
        struct Timeline
        {
            std::vector<PassUsage>  pass_usage_vec;        /* by pass structure ID */
            std::vector<Run>        run_vec;
            std::vector<uint64_t>   setup_frame_index_vec; /* frames that are much longer than usual (loading)         */
            std::vector<ShapeUsage> shape_usage_vec;       /* by shape ID */
        };

        /* What the workload holds and how it was read. */
        struct WorkloadSummary
        {
            uint64_t                                             first_timestamp_qpc = 0;
            bool                                                 is_truncated        = false; /* the workload ends in the middle of a segment and the analysis read up to its end; not set when the analysis stopped at the frame limit */
            uint64_t                                             last_timestamp_qpc  = 0;
            uint64_t                                             n_calls             = 0;
            std::map<Capture::APIDumpLoader::APIGroup, uint64_t> n_calls_by_group_map;
            uint64_t                                             n_calls_ignored     = 0;     /* calls of APIs that have no front end        */
            std::map<ResolutionHazard, uint64_t>                 n_resolution_hazards_map;    /* by kind; absent if there are none           */
            uint64_t                                             n_segments          = 0;
            uint64_t                                             n_trailing_calls    = 0;     /* after the last present, so not in any frame */
            uint64_t                                             n_unfollowed_calls  = 0;     /* see the front ends                          */
            uint64_t                                             n_unsupported_calls = 0;     /* see the front ends                          */
            uint32_t                                             pointer_n_bytes     = 0;     /* of the process that was dumped              */
            uint64_t                                             qpc_frequency       = 0;     /* ticks per second                            */
        };

        /* What a query is about. Each command uses only some of the arguments. */
        struct QueryArguments
        {
            ResourceAccessClass  access_class     = ResourceAccessClass::LOOP_CARRIED;
            uint64_t             frame_index      = 0;
            bool                 has_access_class = false;
            bool                 has_frame_index  = false;
            bool                 has_pass_id      = false;
            bool                 has_shape_id     = false;
            uint64_t             limit            = 100; /* items per list; 0 is all             */
            uint64_t             offset           = 0;   /* items to skip                       */
            uint64_t             pass_id          = 0;
            uint64_t             shape_id         = 0;
            RepresentativeChoice which            = RepresentativeChoice::FIRST;
        };

        /* Identifies the workload that an analysis was made from: its size, and the hashes of three pieces of it (see WorkloadIdentity). A hash
         * of 0 was not recorded (a .passes file holds only the first one) and is not compared.
         */
        struct WorkloadReference
        {
            std::string filename;
            uint64_t    header_hash = 0; /* of the first 4 KiB of the file                         */
            uint64_t    middle_hash = 0; /* of the 4 KiB around the middle of the file             */
            uint64_t    n_bytes     = 0;
            uint64_t    tail_hash   = 0; /* of the last 4 KiB of the file                          */
        };

        bool operator==(const BindingPoint&        in_a,
                        const BindingPoint&        in_b);
        bool operator==(const ClearOperation&      in_a,
                        const ClearOperation&      in_b);
        bool operator==(const ConstantLocation&    in_a,
                        const ConstantLocation&    in_b);
        bool operator==(const ConstantSignature&   in_a,
                        const ConstantSignature&   in_b);
        bool operator==(const ConstantUpdate&      in_a,
                        const ConstantUpdate&      in_b);
        bool operator==(const DataReference&       in_a,
                        const DataReference&       in_b);
        bool operator==(const DrawDescription&     in_a,
                        const DrawDescription&     in_b);
        bool operator==(const DrawSignature&       in_a,
                        const DrawSignature&       in_b);
        bool operator==(const DrawStructure&       in_a,
                        const DrawStructure&       in_b);
        bool operator==(const Format&              in_a,
                        const Format&              in_b);
        bool operator==(const FrameShape&          in_a,
                        const FrameShape&          in_b);
        bool operator==(const PassStructure&       in_a,
                        const PassStructure&       in_b);
        bool operator==(const ResourceDescription& in_a,
                        const ResourceDescription& in_b);
        bool operator==(const ResourceView&        in_a,
                        const ResourceView&        in_b);
        bool operator==(const TextureBinding&      in_a,
                        const TextureBinding&      in_b);
        bool operator==(const TextureBindingKey&   in_a,
                        const TextureBindingKey&   in_b);
        bool operator==(const Viewport&            in_a,
                        const Viewport&            in_b);

        const char* get_resolution_hazard_name(const ResolutionHazard& in_hazard);
        uint32_t    get_resource_id_value     (const ResourceId&       in_id);
        ResourceId  make_resource_id          (const uint32_t&         in_value);
    }
}

#endif /* APIDUMPANALYSIS_TYPES_H */
