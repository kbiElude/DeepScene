/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The vocabulary of the analysis of a workload: what the front ends of the APIs tell the model builder about, and what
 * ends up in the analysis file. Nothing here knows about any particular API; where a value comes from an API (a format,
 * creation flags), it is kept as the API wrote it, together with what it means in general.
 */
#ifndef APIDUMPANALYSIS_TYPES_H
#define APIDUMPANALYSIS_TYPES_H

#include "Capture/APIDumpLoader/api_info.h"
#include <array>
#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* Where a constant lives. */
        enum class ConstantBlockKind
        {
            BUFFER,               /* a buffer resource (a uniform or constant buffer)                                      */
            DEFAULT_UNIFORMS,     /* loose constants of a shader or program: constant registers, the default uniform block */
            FIXED_FUNCTION_STATE, /* state of the fixed-function pipeline (transforms, lights, materials)                  */
            PUSH_CONSTANTS
        };

        enum class ConstantProvenance
        {
            API_DEFINED, /* the API says what the constant is (e.g. a D3D8 view transform) */
            INFERRED     /* somebody has worked it out                                     */
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

        /* How a query ended. */
        enum class QueryStatus
        {
            BAD_ARGUMENTS,
            NOT_FOUND,
            OK
        };

        /* Which of the representative frames of a shape. */
        enum class RepresentativeChoice
        {
            FIRST,
            HEAVIEST,
            LIGHTEST
        };

        /* How the frames of a workload use a resource (see ResourceAccess). */
        /* What an application does that depends on the size of what it renders to in pixels, so that it cannot be made to render larger
         * than it asks without noticing: the calls are counted when a workload is analyzed (see WorkloadSummary).
         */
        enum class ResolutionHazard
        {
            CLEARS_WITH_RECTANGLES, /* a clear of parts of a target, which are in pixels           */
            COPIES_WITH_RECTANGLES, /* a copy of parts of a surface, which are in pixels           */
            POINT_SIZE_STATES,      /* the size of points, which is in pixels                      */
            SCREEN_SPACE_VERTICES,  /* vertices which are positioned in pixels                     */
            SHADER_CREATIONS,       /* shaders, whose constants may hold the size of the screen    */
            SIZE_QUERIES            /* the application asks how large the viewport or a surface is */
        };

        enum class ResourceAccessClass
        {
            FRAME_LOCAL,  /* written before each read, in every frame: a frame can be played twice                 */
            LOOP_CARRIED, /* read before written in a frame that also writes it: holds the previous frame's result */
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

        /* Whether a workload file is the one an analysis was made of (see WorkloadIdentity::verify). */
        enum class WorkloadMatch
        {
            MATCHES,
            MISMATCH,
            UNREADABLE /* the file cannot be opened or read: nothing can be said */
        };

        enum class TransferKind
        {
            COPY,  /* from one resource to another                  */
            UPLOAD /* from the application's memory into a resource */
        };


        /* Where a resource is bound, in the terms of the API: a stage or a texture unit; a set, a binding and an array element; a
         * register space, a register and an array element. Whatever the API has not got is 0.
         */
        struct BindingPoint
        {
            uint32_t element = 0;
            uint32_t group   = 0;
            uint32_t index   = 0;
        };

        struct ClearOperation
        {
            bool                 clears_color   = false;
            bool                 clears_depth   = false;
            bool                 clears_stencil = false;
            std::array<float, 4> color_fp32vec4 = {};
            float                depth          = 0.0f;
            uint32_t             stencil        = 0;
        };

        /* A range of bytes in a block of constants. Blocks which are not backed by memory the application can see (fixed-function
         * state, constant registers) have a layout which the front end defines. Follows ConstantBlockKind and ResourceId.
         */
        struct ConstantLocation
        {
            ResourceId        block_id    = ResourceId::NONE; /* the buffer, or the shader which owns the default uniforms; otherwise NONE */
            ConstantBlockKind block_kind  = ConstantBlockKind::BUFFER;
            uint64_t          byte_offset = 0;
            uint64_t          byte_size   = 0;
        };

        /* Which constant of which kind of pass a time series is of: the number of the pass structure, and the index of the
         * constant in the constant_signature_vec of that structure.
         */
        typedef std::pair<uint32_t, uint32_t> ConstantSeriesKey;

        /* Which depth histogram is of which pass in which kind of frame: the number of the frame shape, and that of the pass structure. */
        typedef std::pair<uint32_t, uint32_t> DepthHistogramKey;

        /* A constant as the passes are told apart by it: where it is, and what it is, not the value. The block is a key, not an
         * ID. Follows ConstantBlockKind, ConstantProvenance and ConstantSemantic.
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

        /* Follows ConstantLocation, ConstantProvenance and ConstantSemantic. */
        struct ConstantUpdate
        {
            ConstantLocation     location;
            ConstantProvenance   provenance = ConstantProvenance::API_DEFINED;
            ConstantSemantic     semantic   = ConstantSemantic::UNKNOWN;
            std::vector<uint8_t> value_u8_vec;                                  /* as the API stores it */
        };

        /* Where an item is kept in the workload, instead of copying it. */
        struct DataReference
        {
            uint64_t content_hash  = 0;
            uint32_t data_chunk_id = 0;
            uint64_t n_bytes       = 0;
        };

        /* raw_value is meaningful only to the front end which wrote it; format_class is what it means in general. */
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


        /* What is rendered to. Follows ResourceView, which it depends on. */
        struct AttachmentSet
        {
            std::vector<ResourceView> color_view_vec;
            ResourceView              depth_stencil_view; /* resource_id is NONE if there is none */
        };

        /* What the frames of a workload did with a resource, in numbers of frames. A frame reads a resource when a draw samples or
         * otherwise reads it, or a copy takes it as the source. It writes it when a draw renders to it, or a clear, upload or copy targets
         * it. It is loop carried when it reads the resource before writing it, and writes it.
         */
        struct ResourceAccess
        {
            uint64_t first_read_frame_index  = 0; /* valid if n_frames_read > 0                                      */
            uint64_t first_write_frame_index = 0; /* valid if n_frames_written > 0                                   */
            uint64_t last_write_frame_index  = 0; /* valid if n_frames_written is not 0                              */
            uint64_t n_frames_loop_carried   = 0; /* frames that read it before writing it, and wrote it             */
            uint64_t n_frames_read           = 0;
            uint64_t n_frames_written        = 0;
        };

        /* Follows Format, ResourceKind and ShaderStage, which it depends on. */
        struct ResourceDescription
        {
            uint64_t     api_flags            = 0; /* what the front end finds worth telling resources apart by (pool, creation flags) */
            uint64_t     content_hash         = 0; /* of the shader code, or of the data which an immutable resource starts with        */
            uint32_t     depth_or_layer_count = 1;
            Format       format;
            bool         has_content_hash     = false;
            uint32_t     height               = 0;
            bool         is_depth_stencil     = false;
            bool         is_dynamic           = false;
            bool         is_render_target     = false;
            ResourceKind kind                 = ResourceKind::UNKNOWN;
            uint32_t     mip_level_count      = 1;
            uint32_t     sample_count         = 1;
            ShaderStage  shader_stage         = ShaderStage::UNKNOWN;
            uint64_t     size_in_bytes        = 0; /* of a buffer */
            uint32_t     width                = 0;
        };

        /* A resource as the model builder knows it. Follows ResourceDescription and ResourceView. */
        struct Resource
        {
            uint64_t            created_at_call_index   = 0;
            std::string         debug_name;                  /* for people; never a part of an identity                                */
            ResourceDescription description;
            uint64_t            destroyed_at_call_index = 0;
            ResourceId          id                      = ResourceId::NONE;
            bool                is_destroyed            = false;
            uint64_t            match_key               = 0;
            ResourceView        parent_view;                 /* resource_id is NONE for a resource which is not a part of another one */
        };

        /* Follows BindingPoint and ResourceView, which it depends on. */
        struct TextureBinding
        {
            BindingPoint binding_point;
            ResourceView view;
        };

        /* A texture binding as the draws are told apart by it. Follows BindingPoint, which it depends on. */
        struct TextureBindingKey
        {
            BindingPoint binding_point;
            uint64_t     view_key = 0;
        };

        /* Where some of the vertices of a draw are, in the space of the model: the surface that it draws, as far as the analysis needs to know
         * it (see ModelBuilder::record_draw_surface_depth). Not the whole geometry, which a draw can have a million vertices of.
         */
        struct GeometrySample
        {
            std::vector<std::array<float, 3> > position_vec;
        };

        /* One draw, with everything that a front end has resolved by the time it is made. Follows GeometrySample and TextureBinding. */
        struct DrawDescription
        {
            std::shared_ptr<const GeometrySample> geometry_ptr;                                        /* null: the front end does not know the vertices */
            ResourceId                            index_buffer_id         = ResourceId::NONE;
            uint32_t                              instance_count          = 1;
            bool                                  is_indexed              = false;
            PrimitiveTopology                     primitive_topology      = PrimitiveTopology::UNKNOWN;
            bool                                  reads_color_attachments = false;                     /* blending: the existing color is an input      */
            bool                                  reads_depth_stencil     = false;                     /* depth or stencil test on                      */
            std::vector<ResourceId>               shader_id_vec;                                       /* empty: the fixed-function pipeline           */
            uint64_t                              state_key               = 0;                         /* of all the state which the draw needs        */
            std::vector<TextureBinding>           texture_binding_vec;
            std::vector<ResourceId>               vertex_buffer_id_vec;
            uint32_t                              vertex_count            = 0;                         /* indices, if the draw is indexed              */
            uint64_t                              vertex_layout_key       = 0;
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

        /* A draw as the passes are told apart by it: the resources are their keys, and the counts are left out. Follows
         * PrimitiveTopology and TextureBindingKey.
         */
        struct DrawStructure
        {
            uint64_t                       index_buffer_key        = 0;
            bool                           is_indexed              = false;
            bool                           is_instanced            = false;
            PrimitiveTopology              primitive_topology      = PrimitiveTopology::UNKNOWN;
            std::vector<uint64_t>          shader_key_vec;
            uint64_t                       state_key               = 0;
            std::vector<TextureBindingKey> texture_binding_key_vec;
            std::vector<uint64_t>          vertex_buffer_key_vec;
            uint64_t                       vertex_layout_key       = 0;
        };

        /* A draw of a pass: the key of its structure, and the key it is recognized by at the moment it is issued (see
         * KeyBuilder::compute_draw_signature_key).
         */
        struct DrawSignature
        {
            uint64_t signature_key = 0;
            uint64_t structure_key = 0;
        };

        /* One occurrence of a pass in a frame, with its size. A pass owns the calls after the previous pass's last draw (or clear) up to
         * its own, since state set before a draw is for that draw. end_call_index is the call that closed the pass: it belongs to the next
         * pass, or is the present.
         */
        struct PassInstance
        {
            uint64_t begin_call_index     = 0;
            uint64_t begin_timestamp_qpc  = 0;
            uint64_t end_call_index       = 0;
            uint64_t last_draw_call_index = 0; /* last draw, or the clear if there are no draws              */
            uint64_t n_draws              = 0;
            uint64_t n_vertices           = 0;
            uint32_t structure_id         = 0; /* of a PassStructure */
        };

        /* What a pass is, whatever its size: where it renders to and how, and which kinds of draws it is made of. A match key
         * recognizes it in another run. Follows ConstantSignature and Viewport.
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

        /* A copy or an upload, which happens between the passes. Follows TransferKind, ResourceView and DataReference. */
        struct TransferRecord
        {
            uint64_t      call_index  = 0;
            DataReference data;                            /* of an upload */
            ResourceView  destination;
            TransferKind  kind        = TransferKind::COPY;
            ResourceView  source;                          /* of a copy    */
        };

        /* What a frame was made of. Follows PassInstance and TransferRecord. */
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

        /* One draw of a frame: where it is, and the key of its structure (see DrawStructure), which is the same in every run. */
        struct DrawInstance
        {
            uint64_t call_index    = 0;
            uint32_t pass_index    = 0; /* a position in CompletedFrame::pass_instance_vec */
            uint64_t structure_key = 0;
        };

        /* A frame as the model builder saw it when it completed, with what a player needs. Call indices are those given to
         * ModelBuilder::begin_call; passes are in the order they were made.
         */
        struct CompletedFrame
        {
            std::vector<DrawInstance> draw_instance_vec; /* in the order of the calls */
            uint64_t                  first_call_index = 0;
            uint64_t                  frame_index      = 0; /* from 0         */
            uint64_t                  last_call_index  = 0; /* the present      */
            std::vector<ResourceId>   loop_carried_resource_id_vec; /* resources the frame reads before it writes, and writes */
            std::vector<PassInstance> pass_instance_vec;
            std::vector<uint64_t>     pass_key_vec;         /* match key of each pass above                             */
            uint64_t                  shape_key        = 0; /* match key of the frame's shape                           */
        };

        /* The pass structures that make a frame. Order is ignored, since independent passes may happen in any order. */
        struct FrameShape
        {
            std::vector<uint32_t> pass_structure_id_vec; /* sorted */
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
            std::vector<uint64_t>   setup_frame_index_vec; /* frames much longer than usual (loading)                  */
            std::vector<ShapeUsage> shape_usage_vec;       /* by shape ID */
        };

        /* What the workload holds and how it was read. */
        struct WorkloadSummary
        {
            uint64_t                                             first_timestamp_qpc = 0;
            bool                                                 is_truncated        = false; /* the workload ends in the middle of a segment */
            uint64_t                                             last_timestamp_qpc  = 0;
            uint64_t                                             n_calls             = 0;
            std::map<Capture::APIDumpLoader::APIGroup, uint64_t> n_calls_by_group_map;
            uint64_t                                             n_calls_ignored     = 0;     /* of APIs which have no front end             */
            std::map<ResolutionHazard, uint64_t>                 n_resolution_hazards_map;    /* by what it is; absent if there are none     */
            uint64_t                                             n_segments          = 0;
            uint64_t                                             n_trailing_calls    = 0;     /* after the last present: not in any frame    */
            uint64_t                                             n_unfollowed_calls  = 0;     /* see the front ends                          */
            uint64_t                                             n_unsupported_calls = 0;     /* see the front ends                          */
            uint32_t                                             pointer_n_bytes     = 0;     /* of the process which was dumped             */
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

        /* Identifies the workload an analysis is of: its size, and the hashes of three pieces of it (see WorkloadIdentity). A hash of 0
         * was not recorded (a .passes file holds the first only), and is not compared.
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
