/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_frontend.h"
#include "analysis/api_dump_analysis/frontends/shared/clear_operation_factory.h"
#include "analysis/api_dump_analysis/frontends/shared/fixed_function_vertex_format.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_function_policy.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_resolution_hazards.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_resource_describer.h"
#include "analysis/api_dump_analysis/frontends/shared/unsupported_use.h"
#include "utils/hashing/stable_hasher.h"
#include "interception/hooks/D3D8/types.h"
#include "interception/core/D3D8Utils/argument_info.h"
#include "interception/core/D3D8Utils/methods.h"
#include "interception/core/D3D8Utils/value_readers.h"
#include <algorithm>
#include <string.h>
#include <string>

Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::D3D8Frontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :FixedFunctionFrontendBase(in_builder_ptr),
     m_auto_depth_stencil_id  (Analysis::APIDumpAnalysis::ResourceId::NONE),
     m_base_vertex_index      (0)
{
    /* Every interface has a Release method. A call to it can mean that the object is gone. */
    #define D3D8_METHOD(INTERFACE_TAG, interface_name, METHOD_TAG, method_name, vtable_index, return_type, parameters) \
        if (::strcmp(#method_name, "Release") == 0) \
        { \
            m_release_function_set.insert(APIInterceptor::APIFUNCTION_D3D8_##INTERFACE_TAG##_##METHOD_TAG); \
        }

    #include "interception/hooks/D3D8/d3d8_methods.inl"

    #undef D3D8_METHOD
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::create_swap_chain_resources(const Interception::Core::D3D8Utils::PresentParameters& in_parameters)
{
    Analysis::APIDumpAnalysis::ResourceView              color_view;
    Analysis::APIDumpAnalysis::ResourceView              depth_stencil_view;
    const Analysis::APIDumpAnalysis::ResourceDescription back_buffer_description = D3D8ResourceDescriber::describe_back_buffer(in_parameters);

    /* The device creates the back buffer, and the depth buffer if one is requested, and initially renders to them. */
    m_back_buffer_view             = Analysis::APIDumpAnalysis::ResourceView();
    m_back_buffer_view.resource_id = m_builder_ptr->on_create_resource      (back_buffer_description,
                                                                             "BackBuffer");
    color_view                     = m_back_buffer_view;
    m_auto_depth_stencil_id        = Analysis::APIDumpAnalysis::ResourceId::NONE;

    if (in_parameters.enable_auto_depth_stencil)
    {
        const Analysis::APIDumpAnalysis::ResourceDescription auto_depth_stencil_description = D3D8ResourceDescriber::describe_auto_depth_stencil(in_parameters);

        m_auto_depth_stencil_id = m_builder_ptr->on_create_resource(auto_depth_stencil_description,
                                                                    "AutoDepthStencil");

        depth_stencil_view.resource_id = m_auto_depth_stencil_id;
    }

    m_render_target_tracker.set(color_view,
                                depth_stencil_view,
                                true); /* in_tells_unchanged */

    /* The viewport covers the whole back buffer until the application sets another one. */
    m_builder_ptr->on_set_viewport(get_full_viewport(m_render_target_tracker.get_color_view() ) );
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::forget_resource(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_geometry_tracker.forget(in_id);
}

Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::DescribeFuncPtr Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_describe_func_ptr(const APIInterceptor::APIFunction& in_function)
{
    DescribeFuncPtr result_ptr = nullptr;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATECUBETEXTURE:
        {
            result_ptr = &D3D8ResourceDescriber::describe_cube_texture;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEDEPTHSTENCILSURFACE:
        {
            result_ptr = &D3D8ResourceDescriber::describe_depth_stencil_surface;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEIMAGESURFACE:
        {
            result_ptr = &D3D8ResourceDescriber::describe_image_surface;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEINDEXBUFFER:
        {
            result_ptr = &D3D8ResourceDescriber::describe_index_buffer;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATERENDERTARGET:
        {
            result_ptr = &D3D8ResourceDescriber::describe_render_target;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATETEXTURE:
        {
            result_ptr = &D3D8ResourceDescriber::describe_texture;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXBUFFER:
        {
            result_ptr = &D3D8ResourceDescriber::describe_vertex_buffer;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVOLUMETEXTURE:
        {
            result_ptr = &D3D8ResourceDescriber::describe_volume_texture;

            break;
        }

        default:
        {
            break;
        }
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::Frontends::FixedFunctionState& Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_fixed_function_state()
{
    return m_pipeline_state.get_fixed_function_state();
}

Analysis::APIDumpAnalysis::PrimitiveTopology Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_primitive_topology(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type)
{
    Analysis::APIDumpAnalysis::PrimitiveTopology result = Analysis::APIDumpAnalysis::PrimitiveTopology::UNKNOWN;

    switch (in_type)
    {
        case Interception::Hooks::D3D8::D3DPT_LINELIST:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_LIST;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_LINESTRIP:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_STRIP;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_POINTLIST:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::POINT_LIST;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_TRIANGLEFAN:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_FAN;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_TRIANGLELIST:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_TRIANGLESTRIP:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_STRIP;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::ConstantSemantic Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_transform_semantic(const uint32_t& in_state) const
{
    constexpr uint32_t N_WORLD_MATRICES = 256; /* D3DTS_WORLDMATRIX(n) is D3DTS_WORLD + n, for n up to 255 */

    Analysis::APIDumpAnalysis::ConstantSemantic result = Analysis::APIDumpAnalysis::ConstantSemantic::UNKNOWN;

    if (in_state == Interception::Hooks::D3D8::D3DTS_VIEW)
    {
        result = Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX;
    }
    else
    if (in_state == Interception::Hooks::D3D8::D3DTS_PROJECTION)
    {
        result = Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX;
    }
    else
    if (in_state >= Interception::Hooks::D3D8::D3DTS_WORLD                    &&
        in_state <  Interception::Hooks::D3D8::D3DTS_WORLD + N_WORLD_MATRICES)
    {
        result = Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX;
    }

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::get_vertex_count(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                                                              const uint32_t&                                    in_n_primitives)
{
    /* A list takes new vertices for every primitive; a strip or a fan takes one for every primitive after the first. */
    constexpr uint32_t N_VERTICES_PER_LINE     = 2;
    constexpr uint32_t N_VERTICES_PER_TRIANGLE = 3;

    uint32_t result = 0;

    switch (in_type)
    {
        case Interception::Hooks::D3D8::D3DPT_LINELIST:
        {
            result = in_n_primitives * N_VERTICES_PER_LINE;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_LINESTRIP:
        {
            result = (in_n_primitives != 0) ? in_n_primitives + N_VERTICES_PER_LINE - 1
                                            : 0;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_POINTLIST:
        {
            result = in_n_primitives;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_TRIANGLEFAN:
        case Interception::Hooks::D3D8::D3DPT_TRIANGLESTRIP:
        {
            result = (in_n_primitives != 0) ? in_n_primitives + N_VERTICES_PER_TRIANGLE - 1
                                            : 0;

            break;
        }

        case Interception::Hooks::D3D8::D3DPT_TRIANGLELIST:
        {
            result = in_n_primitives * N_VERTICES_PER_TRIANGLE;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_clear(const CallView& in_view)
{
    constexpr size_t COLOR_ARGUMENT   = 4;
    constexpr size_t COUNT_ARGUMENT   = 1;
    constexpr size_t DEPTH_ARGUMENT   = 5;
    constexpr size_t FLAGS_ARGUMENT   = 3;
    constexpr size_t STENCIL_ARGUMENT = 6;

    const uint32_t flags = in_view.get_u32(FLAGS_ARGUMENT);

    if (in_view.succeeded() )
    {
        const uint32_t color   = in_view.get_u32(COLOR_ARGUMENT);
        const float    depth   = in_view.get_f32(DEPTH_ARGUMENT);
        const uint32_t stencil = in_view.get_u32(STENCIL_ARGUMENT);

        Analysis::APIDumpAnalysis::ClearOperation clear = ClearOperationFactory::create(((flags & Interception::Hooks::D3D8::D3DCLEAR_TARGET)  != 0),
                                                                                        ((flags & Interception::Hooks::D3D8::D3DCLEAR_ZBUFFER) != 0),
                                                                                        ((flags & Interception::Hooks::D3D8::D3DCLEAR_STENCIL) != 0),
                                                                                        color,
                                                                                        depth,
                                                                                        stencil);

        /* Without rectangles, the clear covers the viewport. */
        clear.has_rectangles         = (in_view.get_u32(COUNT_ARGUMENT) != 0);
        clear.is_limited_to_viewport = true;

        m_builder_ptr->on_clear(clear);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_copy_rects(const CallView& in_view)
{
    constexpr size_t DESTINATION_ARGUMENT = 4;
    constexpr size_t SOURCE_ARGUMENT      = 1;

    const ObjectTracker::TrackedObject* destination_ptr = m_tracker.get_object_ptr(in_view.get_pointer(DESTINATION_ARGUMENT) );
    const ObjectTracker::TrackedObject* source_ptr      = m_tracker.get_object_ptr(in_view.get_pointer(SOURCE_ARGUMENT) );

    if (in_view.succeeded() == false)
    {
        /* Nothing was copied. */
    }
    else
    if (destination_ptr == nullptr ||
        source_ptr      == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        m_builder_ptr->on_copy(source_ptr->view,
                               destination_ptr->view);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_create_device(const CallView& in_view)
{
    constexpr size_t DEVICE_ARGUMENT                  = 6;
    constexpr size_t PRESENTATION_PARAMETERS_ARGUMENT = 5;

    Interception::Core::D3D8Utils::PresentParameters parameters;
    bool                                             is_ok      = false;

    /* The device fills in what the application left unspecified (the size of the window). */
    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_out_chunk_ptr(PRESENTATION_PARAMETERS_ARGUMENT);

    if (chunk_ptr == nullptr)
    {
        chunk_ptr = in_view.get_chunk_ptr(PRESENTATION_PARAMETERS_ARGUMENT);
    }

    const bool has_succeeded = in_view.succeeded();

    if (has_succeeded)
    {
        const uint64_t device_pointer = in_view.get_out_pointer(DEVICE_ARGUMENT);

        if (device_pointer != 0       &&
            chunk_ptr      != nullptr)
        {
            is_ok = Interception::Core::D3D8Utils::parse_present_parameters(chunk_ptr->data(),
                                                                            chunk_ptr->size(),
                                                                           &parameters);
        }
    }

    if (is_ok == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        /* A new device starts with the default state. Shader handles belong to the device. */
        m_pipeline_state.reset   ();
        m_pixel_shader_map.clear ();
        m_vertex_shader_map.clear();

        create_swap_chain_resources(parameters);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_create_pixel_shader(const CallView& in_view)
{
    constexpr size_t FUNCTION_EXTRA  = 0;
    constexpr size_t HANDLE_ARGUMENT = 2;

    const Capture::APIDumpLoader::DataChunk* function_ptr = in_view.get_extra_chunk_ptr(FUNCTION_EXTRA);
    const uint32_t                           handle       = in_view.get_out_u32        (HANDLE_ARGUMENT);

    if (in_view.succeeded() == false)
    {
        /* Nothing was made. */
    }
    else
    if (function_ptr == nullptr ||
        handle       == 0)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        Analysis::APIDumpAnalysis::ResourceDescription description;
        const std::string                              shader_name = "PixelShader " + std::to_string(handle);

        description.content_hash     = Utils::Hashing::StableHasher::hash_bytes(function_ptr->data(),
                                                                                function_ptr->size() );
        description.has_content_hash = true;
        description.kind             = Analysis::APIDumpAnalysis::ResourceKind::SHADER;
        description.shader_stage     = Analysis::APIDumpAnalysis::ShaderStage::FRAGMENT;
        description.size_in_bytes    = function_ptr->size();

        m_pixel_shader_map[handle] = m_builder_ptr->on_create_resource(description,
                                                                       shader_name);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_create_resource(const CallView&        in_view,
                                                                                const DescribeFuncPtr& in_describe_func_ptr)
{
    /* The created object is returned through the last argument of the method. */
    const size_t   object_argument = in_view.get_n_head_arguments() - 1;
    const uint64_t address         = in_view.get_out_pointer(object_argument);

    Interception::Hooks::D3D8::ArgumentInfo info          = {};
    bool                                    is_info_known = false;

    if (address != 0)
    {
        is_info_known = Interception::Core::D3D8Utils::get_argument_info(in_view.get_call().func,
                                                                         static_cast<uint32_t>(object_argument),
                                                                        &info);
    }

    if (in_view.succeeded() == false)
    {
        /* The call did not create an object, so there is nothing to follow. */
    }
    else
    if (is_info_known == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const Analysis::APIDumpAnalysis::ResourceDescription resource_description = in_describe_func_ptr(in_view);
        const std::string                                    resource_name        = m_namer.get_name(address,
                                                                                                      info.object_type_name_ptr);
        ObjectTracker::TrackedObject                         tracked_object;

        tracked_object.owns_resource    = true;
        tracked_object.view.resource_id = m_builder_ptr->on_create_resource(resource_description,
                                                                            resource_name);

        m_tracker.track(address,
                        tracked_object);

        register_buffer(in_view,
                        tracked_object.view.resource_id);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_create_vertex_shader(const CallView& in_view)
{
    constexpr size_t DECLARATION_EXTRA = 0;
    constexpr size_t FUNCTION_EXTRA    = 1;
    constexpr size_t HANDLE_ARGUMENT   = 3;

    const Capture::APIDumpLoader::DataChunk* declaration_ptr = in_view.get_extra_chunk_ptr(DECLARATION_EXTRA);
    const Capture::APIDumpLoader::DataChunk* function_ptr    = in_view.get_extra_chunk_ptr(FUNCTION_EXTRA);
    const uint32_t                           handle          = in_view.get_out_u32        (HANDLE_ARGUMENT);

    if (in_view.succeeded() == false)
    {
        /* Nothing was made. */
    }
    else
    if (declaration_ptr == nullptr ||
        handle          == 0)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        Utils::Hashing::StableHasher                   code_hasher;
        Analysis::APIDumpAnalysis::ResourceDescription description;
        VertexShader                                   shader;
        const std::string                              shader_name = "VertexShader " + std::to_string(handle);

        /* A shader without a function is a vertex declaration for the fixed-function pipeline. */
        code_hasher.add_bytes(declaration_ptr->data(),
                              declaration_ptr->size() );

        if (function_ptr != nullptr)
        {
            code_hasher.add_bytes(function_ptr->data(),
                                  function_ptr->size() );

            description.size_in_bytes = function_ptr->size();
        }

        /* A hash over the declaration and the function together. It identifies the shader. */
        description.content_hash     = code_hasher.get_digest();
        description.has_content_hash = true;
        description.kind             = Analysis::APIDumpAnalysis::ResourceKind::SHADER;
        description.shader_stage     = Analysis::APIDumpAnalysis::ShaderStage::VERTEX;

        shader.declaration_hash = Utils::Hashing::StableHasher::hash_bytes(declaration_ptr->data(),
                                                                           declaration_ptr->size() );
        shader.id               = m_builder_ptr->on_create_resource            (description,
                                                                                shader_name);

        m_vertex_shader_map[handle] = shader;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_delete_pixel_shader(const CallView& in_view)
{
    constexpr size_t HANDLE_ARGUMENT = 1;

    const auto shader_iterator = m_pixel_shader_map.find(in_view.get_u32(HANDLE_ARGUMENT) );

    if (shader_iterator != m_pixel_shader_map.end() )
    {
        m_builder_ptr->on_destroy_resource(shader_iterator->second);

        m_pixel_shader_map.erase(shader_iterator);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_delete_vertex_shader(const CallView& in_view)
{
    constexpr size_t HANDLE_ARGUMENT = 1;

    const auto shader_iterator = m_vertex_shader_map.find(in_view.get_u32(HANDLE_ARGUMENT) );

    if (shader_iterator != m_vertex_shader_map.end() )
    {
        m_builder_ptr->on_destroy_resource(shader_iterator->second.id);

        m_vertex_shader_map.erase(shader_iterator);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_draw(const CallView& in_view,
                                                                     const bool&     in_is_indexed)
{
    /* DrawIndexedPrimitive has the type, the first index, the number of vertices, the start index and the number of
     * primitives; DrawPrimitive has the type, the first vertex and the number of primitives.
     */
    constexpr size_t INDEXED_N_PRIMITIVES_ARGUMENT   = 5;
    constexpr size_t PRIMITIVE_TYPE_ARGUMENT         = 1;
    constexpr size_t UNINDEXED_N_PRIMITIVES_ARGUMENT = 3;

    const uint32_t                                    n_primitives = in_view.get_u32((in_is_indexed) ? INDEXED_N_PRIMITIVES_ARGUMENT
                                                                                                     : UNINDEXED_N_PRIMITIVES_ARGUMENT);
    const Interception::Hooks::D3D8::D3DPRIMITIVETYPE type         = static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_view.get_u32(PRIMITIVE_TYPE_ARGUMENT) );

    if (in_view.succeeded() )
    {
        const Analysis::APIDumpAnalysis::PrimitiveTopology topology     = get_primitive_topology(type);
        const uint32_t                                     vertex_count = get_vertex_count      (type,
                                                                                                 n_primitives);
        auto                                               description  = m_pipeline_state.describe_draw(topology,
                                                                                                         vertex_count,
                                                                                                         in_is_indexed);

        if (m_builder_ptr->wants_geometry() )
        {
            description.geometry_ptr = sample_geometry(in_view,
                                                       in_is_indexed,
                                                       description.vertex_count);
        }

        m_builder_ptr->on_draw(description);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_draw_user_pointer(const CallView& in_view,
                                                                                  const bool&     in_is_indexed)
{
    /* DrawPrimitiveUP has the type, the number of primitives, the vertices and their stride; DrawIndexedPrimitiveUP has
     * the type, the first index, the number of vertices, the number of primitives, the indices, their format, the
     * vertices and their stride.
     */
    constexpr size_t INDEXED_N_PRIMITIVES_ARGUMENT   = 4;
    constexpr size_t INDEXED_STRIDE_ARGUMENT         = 8;
    constexpr size_t PRIMITIVE_TYPE_ARGUMENT         = 1;
    constexpr size_t UNINDEXED_N_PRIMITIVES_ARGUMENT = 2;
    constexpr size_t UNINDEXED_STRIDE_ARGUMENT       = 4;

    const uint32_t                                    n_primitives = in_view.get_u32((in_is_indexed) ? INDEXED_N_PRIMITIVES_ARGUMENT
                                                                                                     : UNINDEXED_N_PRIMITIVES_ARGUMENT);
    const uint32_t                                    stride       = in_view.get_u32((in_is_indexed) ? INDEXED_STRIDE_ARGUMENT
                                                                                                     : UNINDEXED_STRIDE_ARGUMENT);
    const Interception::Hooks::D3D8::D3DPRIMITIVETYPE type         = static_cast<Interception::Hooks::D3D8::D3DPRIMITIVETYPE>(in_view.get_u32(PRIMITIVE_TYPE_ARGUMENT) );

    if (in_view.succeeded() )
    {
        const Analysis::APIDumpAnalysis::PrimitiveTopology topology     = get_primitive_topology(type);
        const uint32_t                                     vertex_count = get_vertex_count      (type,
                                                                                                 n_primitives);

        auto description = m_pipeline_state.describe_user_pointer_draw(topology,
                                                                       vertex_count,
                                                                       in_is_indexed,
                                                                       stride);

        if (m_builder_ptr->wants_geometry() )
        {
            description.geometry_ptr = sample_user_pointer_geometry(in_view,
                                                                    in_is_indexed,
                                                                    description.vertex_count,
                                                                    stride);
        }

        m_builder_ptr->on_draw(description);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_get_back_buffer(const CallView& in_view)
{
    constexpr size_t BACK_BUFFER_ARGUMENT = 3;

    const uint64_t address = in_view.get_out_pointer(BACK_BUFFER_ARGUMENT);

    if (in_view.succeeded() == false)
    {
        /* Nothing to follow. */
    }
    else
    if (address == 0)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ObjectTracker::TrackedObject tracked_object;

        /* The analysis treats all back buffers of a swap chain as one image. */
        tracked_object.view = m_back_buffer_view;

        m_tracker.track(address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_get_cube_map_surface(const CallView& in_view)
{
    constexpr size_t FACE_ARGUMENT    = 1;
    constexpr size_t LEVEL_ARGUMENT   = 2;
    constexpr size_t SURFACE_ARGUMENT = 3;
    constexpr size_t TEXTURE_ARGUMENT = 0;

    const uint64_t                      surface_address = in_view.get_out_pointer (SURFACE_ARGUMENT);
    const ObjectTracker::TrackedObject* texture_ptr     = m_tracker.get_object_ptr(in_view.get_pointer(TEXTURE_ARGUMENT) );

    if (in_view.succeeded() == false)
    {
        /* Nothing to follow. */
    }
    else
    if (surface_address == 0       ||
        texture_ptr     == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ObjectTracker::TrackedObject tracked_object;

        tracked_object.view.layer       = in_view.get_u32(FACE_ARGUMENT);
        tracked_object.view.mip_level   = in_view.get_u32(LEVEL_ARGUMENT);
        tracked_object.view.resource_id = texture_ptr->view.resource_id;

        m_tracker.track(surface_address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_get_depth_stencil_surface(const CallView& in_view)
{
    constexpr size_t SURFACE_ARGUMENT = 1;

    const uint64_t address = in_view.get_out_pointer(SURFACE_ARGUMENT);

    if (in_view.succeeded() == false)
    {
        /* Nothing to follow. */
    }
    else
    if (address                          == 0                                           ||
        m_render_target_tracker.get_depth_stencil_view().resource_id == Analysis::APIDumpAnalysis::ResourceId::NONE)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ObjectTracker::TrackedObject tracked_object;

        tracked_object.view = m_render_target_tracker.get_depth_stencil_view();

        m_tracker.track(address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_get_render_target(const CallView& in_view)
{
    constexpr size_t SURFACE_ARGUMENT = 1;

    const uint64_t address = in_view.get_out_pointer(SURFACE_ARGUMENT);

    if (in_view.succeeded() == false)
    {
        /* Nothing to follow. */
    }
    else
    if (address == 0)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ObjectTracker::TrackedObject tracked_object;

        tracked_object.view = m_render_target_tracker.get_color_view();

        m_tracker.track(address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_get_surface_level(const CallView& in_view)
{
    constexpr size_t LEVEL_ARGUMENT   = 1;
    constexpr size_t SURFACE_ARGUMENT = 2;
    constexpr size_t TEXTURE_ARGUMENT = 0;

    const uint64_t                      surface_address = in_view.get_out_pointer (SURFACE_ARGUMENT);
    const ObjectTracker::TrackedObject* texture_ptr     = m_tracker.get_object_ptr(in_view.get_pointer(TEXTURE_ARGUMENT) );

    if (in_view.succeeded() == false)
    {
        /* Nothing to follow. */
    }
    else
    if (surface_address == 0       ||
        texture_ptr     == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ObjectTracker::TrackedObject tracked_object;

        tracked_object.view.mip_level   = in_view.get_u32(LEVEL_ARGUMENT);
        tracked_object.view.resource_id = texture_ptr->view.resource_id;

        m_tracker.track(surface_address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_lock(const CallView& in_view)
{
    constexpr size_t BUFFER_N_BYTES_ARGUMENT = 2;
    constexpr size_t BUFFER_OFFSET_ARGUMENT  = 1;
    constexpr size_t OBJECT_ARGUMENT         = 0;

    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(in_view.get_pointer(OBJECT_ARGUMENT) );

    if (object_ptr != nullptr)
    {
        const uint32_t n_bytes = in_view.get_u32(BUFFER_N_BYTES_ARGUMENT);
        const uint32_t offset  = in_view.get_u32(BUFFER_OFFSET_ARGUMENT);

        m_geometry_tracker.on_lock(object_ptr->view.resource_id,
                                   offset,
                                   n_bytes);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_reset(const CallView& in_view)
{
    constexpr size_t PRESENTATION_PARAMETERS_ARGUMENT = 1;

    bool                                             is_ok      = false;
    Interception::Core::D3D8Utils::PresentParameters parameters;

    /* The device fills in what the application left unspecified (the size of the window). */
    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_out_chunk_ptr(PRESENTATION_PARAMETERS_ARGUMENT);

    if (chunk_ptr == nullptr)
    {
        chunk_ptr = in_view.get_chunk_ptr(PRESENTATION_PARAMETERS_ARGUMENT);
    }

    if (in_view.succeeded() == false)
    {
        /* The device is as it was. */
    }
    else
    {
        if (chunk_ptr != nullptr)
        {
            is_ok = Interception::Core::D3D8Utils::parse_present_parameters(chunk_ptr->data(),
                                                                            chunk_ptr->size(),
                                                                           &parameters);
        }

        if (is_ok == false)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            /* The resources the device created itself are gone, and the device state is the default again. Render targets
             * created by the application are not destroyed here, because they do not belong to the device.
             */
            m_builder_ptr->on_destroy_resource(m_back_buffer_view.resource_id);
            m_builder_ptr->on_destroy_resource(m_auto_depth_stencil_id);

            m_pipeline_state.reset();

            create_swap_chain_resources(parameters);
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_set_indices(const CallView& in_view)
{
    constexpr size_t BASE_VERTEX_INDEX_ARGUMENT = 2;
    constexpr size_t BUFFER_ARGUMENT            = 1;

    const uint64_t                      address    = in_view.get_pointer     (BUFFER_ARGUMENT);
    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(address);

    if (address == 0)
    {
        m_pipeline_state.set_index_buffer(Analysis::APIDumpAnalysis::ResourceId::NONE);
    }
    else
    if (object_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        m_base_vertex_index = in_view.get_u32(BASE_VERTEX_INDEX_ARGUMENT);

        m_pipeline_state.set_index_buffer(object_ptr->view.resource_id);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_set_pixel_shader(const CallView& in_view)
{
    constexpr size_t HANDLE_ARGUMENT = 1;

    const uint32_t handle          = in_view.get_u32        (HANDLE_ARGUMENT);
    const auto     shader_iterator = m_pixel_shader_map.find(handle);

    if (handle == 0)
    {
        m_pipeline_state.set_pixel_shader(Analysis::APIDumpAnalysis::ResourceId::NONE);
    }
    else
    if (shader_iterator == m_pixel_shader_map.end() )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        m_pipeline_state.set_pixel_shader(shader_iterator->second);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_set_render_target(const CallView& in_view)
{
    constexpr size_t DEPTH_STENCIL_ARGUMENT = 2;
    constexpr size_t RENDER_TARGET_ARGUMENT = 1;

    const uint64_t                      depth_stencil_address = in_view.get_pointer     (DEPTH_STENCIL_ARGUMENT);
    const ObjectTracker::TrackedObject* depth_stencil_ptr     = m_tracker.get_object_ptr(depth_stencil_address);
    const uint64_t                      render_target_address = in_view.get_pointer     (RENDER_TARGET_ARGUMENT);
    const ObjectTracker::TrackedObject* render_target_ptr     = m_tracker.get_object_ptr(render_target_address);

    if (in_view.succeeded() == false)
    {
        /* Nothing was changed. */
    }
    else
    if ((render_target_address != 0        &&
         render_target_ptr     == nullptr) ||
        (depth_stencil_address != 0        &&
         depth_stencil_ptr     == nullptr) )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        /* A surface that is not passed stays unchanged. */
        const Analysis::APIDumpAnalysis::ResourceView color_view         = (render_target_ptr != nullptr) ? render_target_ptr->view
                                                                                                            : m_render_target_tracker.get_color_view();
        const Analysis::APIDumpAnalysis::ResourceView depth_stencil_view = (depth_stencil_ptr != nullptr) ? depth_stencil_ptr->view
                                                                                                            : m_render_target_tracker.get_depth_stencil_view();

        m_render_target_tracker.set(color_view,
                                    depth_stencil_view,
                                    true); /* in_tells_unchanged */

        /* The viewport covers the whole new render target. */
        if (render_target_ptr != nullptr)
        {
            m_builder_ptr->on_set_viewport(get_full_viewport(m_render_target_tracker.get_color_view() ) );
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_set_stream_source(const CallView& in_view)
{
    constexpr size_t BUFFER_ARGUMENT = 2;
    constexpr size_t STREAM_ARGUMENT = 1;
    constexpr size_t STRIDE_ARGUMENT = 3;

    const uint64_t                      address    = in_view.get_pointer     (BUFFER_ARGUMENT);
    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(address);

    if (address == 0)
    {
        const uint32_t stream = in_view.get_u32(STREAM_ARGUMENT);

        if (m_pipeline_state.set_stream_source(stream,
                                               Analysis::APIDumpAnalysis::ResourceId::NONE,
                                               0) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
    else
    if (object_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const uint32_t stream = in_view.get_u32(STREAM_ARGUMENT);
        const uint32_t stride = in_view.get_u32(STRIDE_ARGUMENT);

        if (m_pipeline_state.set_stream_source(stream,
                                               object_ptr->view.resource_id,
                                               stride) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_set_vertex_shader(const CallView& in_view)
{
    constexpr size_t HANDLE_ARGUMENT = 1;

    const uint32_t handle          = in_view.get_u32         (HANDLE_ARGUMENT);
    const auto     shader_iterator = m_vertex_shader_map.find(handle);

    /* The handle is either the handle of a vertex shader that was created, or an FVF code. */
    if (shader_iterator != m_vertex_shader_map.end() )
    {
        m_pipeline_state.set_vertex_shader(shader_iterator->second.id,
                                           shader_iterator->second.declaration_hash);
    }
    else
    {
        m_pipeline_state.set_vertex_shader(handle);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_set_viewport(const CallView& in_view)
{
    constexpr size_t VIEWPORT_ARGUMENT = 1;

    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(VIEWPORT_ARGUMENT);
    bool                                     is_parsed = false;
    Interception::Hooks::D3D8::D3DVIEWPORT8  viewport  = {};

    if (chunk_ptr != nullptr)
    {
        is_parsed = Interception::Core::D3D8Utils::parse_viewport(chunk_ptr->data(),
                                                                  chunk_ptr->size(),
                                                                 &viewport);
    }

    if (is_parsed == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        Analysis::APIDumpAnalysis::Viewport model_viewport;

        model_viewport.height    = static_cast<float>(viewport.Height);
        model_viewport.max_depth = viewport.MaxZ;
        model_viewport.min_depth = viewport.MinZ;
        model_viewport.width     = static_cast<float>(viewport.Width);
        model_viewport.x         = static_cast<float>(viewport.X);
        model_viewport.y         = static_cast<float>(viewport.Y);

        m_builder_ptr->on_set_viewport(model_viewport);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_unlock(const CallView& in_view,
                                                                       const size_t&   in_n_data_argument,
                                                                       const uint32_t& in_layer,
                                                                       const uint32_t& in_mip_level)
{
    constexpr size_t OBJECT_ARGUMENT = 0;

    Analysis::APIDumpAnalysis::DataReference data;
    bool                                     is_data_found = false;
    const ObjectTracker::TrackedObject*      object_ptr    = m_tracker.get_object_ptr(in_view.get_pointer(OBJECT_ARGUMENT) );

    if (object_ptr != nullptr)
    {
        is_data_found = in_view.get_data_reference(in_n_data_argument,
                                                  &data);
    }

    if (is_data_found == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        Analysis::APIDumpAnalysis::ResourceView destination = object_ptr->view;

        /* A buffer or a texture is addressed as one resource. For a texture, the level and the face select the part that is written. */
        if (object_ptr->owns_resource)
        {
            destination.layer     = in_layer;
            destination.mip_level = in_mip_level;
        }

        const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(in_n_data_argument);

        if (chunk_ptr != nullptr)
        {
            m_geometry_tracker.on_unlock(object_ptr->view.resource_id,
                                         chunk_ptr->data(),
                                         chunk_ptr->size());
        }

        m_builder_ptr->on_upload(destination,
                                 data);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::handle_update_texture(const CallView& in_view)
{
    constexpr size_t DESTINATION_ARGUMENT = 2;
    constexpr size_t SOURCE_ARGUMENT      = 1;

    const ObjectTracker::TrackedObject* destination_ptr = m_tracker.get_object_ptr(in_view.get_pointer(DESTINATION_ARGUMENT) );
    const ObjectTracker::TrackedObject* source_ptr      = m_tracker.get_object_ptr(in_view.get_pointer(SOURCE_ARGUMENT) );

    if (in_view.succeeded() == false)
    {
        /* Nothing was copied. */
    }
    else
    if (destination_ptr == nullptr ||
        source_ptr      == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        m_builder_ptr->on_copy(source_ptr->view,
                               destination_ptr->view);
    }
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::is_pre_transformed_vertex_shader(const uint32_t& in_vertex_shader)
{
    constexpr uint32_t HANDLE_BIT = 0x80000000u; /* set in a shader handle, not in a format */

    return ((in_vertex_shader & HANDLE_BIT) == 0 &&
            FixedFunctionVertexFormat::is_pre_transformed(in_vertex_shader) );
}

bool Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::is_state_function(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_LIGHTENABLE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETINDICES:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETLIGHT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERTARGET:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETSTREAMSOURCE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURESTAGESTATE:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVIEWPORT:
        {
            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                 const Capture::APIDumpLoader::Workload&      in_workload)
{
    const DescribeFuncPtr describe_func_ptr = get_describe_func_ptr(in_call.func);
    const uint32_t        n_arguments       = Interception::Core::D3D8Utils::get_n_arguments(in_call.func);
    const CallView view(in_call,
                        in_workload,
                        n_arguments);

    register_names(view);

    {
        Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES;

        if (D3D8ResolutionHazards::classify(in_call.func,
                                            static_cast<uint32_t>  (in_call.arg_in_vec.size() ),
                                            in_call.arg_in_vec.data(),
                                           &hazard) )
        {
            m_resolution_hazard_counter.count(hazard);
        }
    }

    bool       is_ignored    = false;
    const bool is_state_call = is_state_function(in_call.func);

    if (is_state_call)
    {
        const bool has_succeeded = view.succeeded();

        is_ignored = (has_succeeded == false);
    }

    /* A call that failed changed nothing. */
    if (is_ignored)
    {
        /* Nothing to do. */
    }
    else
    if (describe_func_ptr != nullptr)
    {
        handle_create_resource(view,
                               describe_func_ptr);
    }
    else
    {
        switch (in_call.func)
        {
            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3D8_CREATEDEVICE:
            {
                handle_create_device(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DCUBETEXTURE8_GETCUBEMAPSURFACE:
            {
                handle_get_cube_map_surface(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETBACKBUFFER:
            {
                handle_get_back_buffer(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETDEPTHSTENCILSURFACE:
            {
                handle_get_depth_stencil_surface(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETRENDERTARGET:
            {
                handle_get_render_target(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
            {
                handle_clear(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE:
            {
                handle_draw(view,
                            true); /* in_is_indexed */

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE:
            {
                handle_draw(view,
                            false); /* in_is_indexed */

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_LIGHTENABLE:
            {
                handle_light_enable(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT:
            {
                /* Every present counts as a frame, as in the other tools, whether or not the device could show it. */
                m_builder_ptr->on_frame_presented();

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETINDICES:
            {
                handle_set_indices(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETLIGHT:
            {
                handle_set_light(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE:
            {
                handle_set_render_state(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERTARGET:
            {
                handle_set_render_target(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETSTREAMSOURCE:
            {
                handle_set_stream_source(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURE:
            {
                handle_set_texture(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTEXTURESTAGESTATE:
            {
                handle_set_texture_stage_state(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM:
            {
                handle_set_transform(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER:
            {
                handle_set_vertex_shader(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVIEWPORT:
            {
                handle_set_viewport(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_GETSURFACELEVEL:
            {
                handle_get_surface_level(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
            {
                handle_copy_rects(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEPIXELSHADER:
            {
                handle_create_pixel_shader(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER:
            {
                handle_create_vertex_shader(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEPIXELSHADER:
            {
                handle_delete_pixel_shader(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DELETEVERTEXSHADER:
            {
                handle_delete_vertex_shader(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP:
            {
                handle_draw_user_pointer(view,
                                         true); /* in_is_indexed */

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP:
            {
                handle_draw_user_pointer(view,
                                         false); /* in_is_indexed */

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_RESET:
            {
                handle_reset(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETPIXELSHADER:
            {
                handle_set_pixel_shader(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_UPDATETEXTURE:
            {
                handle_update_texture(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_LOCK:
            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_LOCK:
            {
                handle_lock(view);

                break;
            }

            /* The dumper stores what the application wrote to a buffer or a texture with the unlock call. The data follows
             * the arguments of the method.
             */
            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK:
            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK:
            {
                handle_unlock(view,
                              view.get_n_head_arguments(),
                              0,
                              0);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DCUBETEXTURE8_UNLOCKRECT:
            {
                constexpr size_t FACE_ARGUMENT  = 1;
                constexpr size_t LEVEL_ARGUMENT = 2;

                const uint32_t face      = view.get_u32(FACE_ARGUMENT);
                const uint32_t mip_level = view.get_u32(LEVEL_ARGUMENT);

                handle_unlock(view,
                              view.get_n_head_arguments(),
                              face,
                              mip_level);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_UNLOCKRECT:
            {
                handle_unlock(view,
                              view.get_n_head_arguments(),
                              0,
                              0);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT:
            {
                constexpr size_t LEVEL_ARGUMENT = 1;

                const uint32_t mip_level = view.get_u32(LEVEL_ARGUMENT);

                handle_unlock(view,
                              view.get_n_head_arguments(),
                              0,
                              mip_level);

                break;
            }

            default:
            {
                if (m_release_function_set.find(in_call.func) != m_release_function_set.end() )
                {
                    handle_release(view);
                }
                else
                if (view.has_failed() )
                {
                    /* A call that failed changed nothing. */
                }
                else
                {
                    /* A call that is not followed may be left out only if the policy allows it. Any other unfollowed call ends the process. */
                    const char* unsupported_reason_ptr = D3D8FunctionPolicy::get_unsupported_reason_ptr(in_call.func);

                    if (unsupported_reason_ptr != nullptr)
                    {
                        UnsupportedUse::report(in_call.func,
                                               unsupported_reason_ptr);
                    }
                    else
                    if (D3D8FunctionPolicy::is_left_out_by_design(in_call.func) == false)
                    {
                        UnsupportedUse::report(in_call.func,
                                               "the front end does not know what the call does, and the policy does not say that it may be left out");
                    }
                }

                break;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::register_buffer(const CallView&                              in_view,
                                                                         const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    constexpr size_t FORMAT_ARGUMENT = 3;
    constexpr size_t LENGTH_ARGUMENT = 1;

    switch (in_view.get_call().func)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEINDEXBUFFER:
        {
            const uint32_t format = in_view.get_u32(FORMAT_ARGUMENT);
            const uint32_t length = in_view.get_u32(LENGTH_ARGUMENT);

            m_geometry_tracker.on_create_index_buffer(in_id,
                                                      length,
                                                      format);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXBUFFER:
        {
            const uint32_t length = in_view.get_u32(LENGTH_ARGUMENT);

            m_geometry_tracker.on_create_vertex_buffer(in_id,
                                                       length);

            break;
        }

        default:
        {
            break;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::register_names(const CallView& in_view)
{
    const auto& call = in_view.get_call();

    if (call.func == APIInterceptor::APIFUNCTION_D3D8_DIRECT3DCREATE8)
    {
        m_namer.register_object(in_view.get_returned_pointer(),
                                "Direct3D8",
                                true); /* in_is_new */
    }
    else
    {
        const bool has_succeeded = in_view.succeeded();

        if (has_succeeded)
        {
            const auto* method_info_ptr = Interception::Core::D3D8Utils::get_method_info_ptr(call.func);

            if (method_info_ptr != nullptr)
            {
                /* Create* methods always return new objects. Get* methods return existing objects, which may be unknown to the front end. */
                constexpr char CREATE_PREFIX[] = "Create";

                const bool is_create = (::strncmp(method_info_ptr->method_name_ptr,
                                                  CREATE_PREFIX,
                                                  sizeof(CREATE_PREFIX) - 1) == 0);

                for (uint32_t n_argument = 0;
                              n_argument < call.arg_out_vec.size();
                            ++n_argument)
                {
                    Interception::Hooks::D3D8::ArgumentInfo info = {};

                    const bool is_info_known = Interception::Core::D3D8Utils::get_argument_info(call.func,
                                                                                                n_argument,
                                                                                               &info);

                    if (is_info_known)
                    {
                        if (info.kind == Interception::Hooks::D3D8::ValueKind::OBJECT_OUT)
                        {
                            const uint64_t address = in_view.get_out_pointer(n_argument);

                            m_namer.register_object(address,
                                                    info.object_type_name_ptr,
                                                    is_create);
                        }
                    }
                }
            }
        }
    }
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::sample_geometry(const CallView& in_view,
                                                                                                                                     const bool&     in_is_indexed,
                                                                                                                                     const uint32_t& in_vertex_count)
{
    /* DrawIndexedPrimitive has the start index as its fifth argument, DrawPrimitive the first vertex as its third; the vertices are those of the
     * first stream.
     */
    constexpr size_t   INDEXED_START_INDEX_ARGUMENT  = 4;
    constexpr uint32_t MAIN_STREAM                   = 0;
    constexpr size_t   UNINDEXED_START_VERTEX_ARGUMENT = 2;

    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;
    D3D8GeometryTracker::DrawRange                                   range;

    range.base_vertex_index = (in_is_indexed) ? m_base_vertex_index
                                              : 0;
    range.first             = in_view.get_u32((in_is_indexed) ? INDEXED_START_INDEX_ARGUMENT
                                                              : UNINDEXED_START_VERTEX_ARGUMENT);
    range.fvf               = m_pipeline_state.get_fvf();
    range.index_buffer_id   = (in_is_indexed) ? m_pipeline_state.get_index_buffer_id()
                                              : Analysis::APIDumpAnalysis::ResourceId::NONE;
    range.is_indexed        = in_is_indexed;
    range.vertex_count      = in_vertex_count;

    const bool has_stream = m_pipeline_state.get_stream_source(MAIN_STREAM,
                                                              &range.vertex_buffer_id,
                                                              &range.stride);

    if (has_stream)
    {
        result_ptr = m_geometry_tracker.sample(range);
    }

    return result_ptr;
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::D3D8Frontend::sample_user_pointer_geometry(const CallView& in_view,
                                                                                                                                                  const bool&     in_is_indexed,
                                                                                                                                                  const uint32_t& in_vertex_count,
                                                                                                                                                  const uint32_t& in_stride)
{
    /* The dumper stores data with the call, after the arguments: the indices (if the draw has them), then the vertices.
     * The format of the indices is the seventh argument of DrawIndexedPrimitiveUP.
     */
    constexpr size_t INDEX_FORMAT_ARGUMENT = 6;

    const size_t                             n_indices_extra = (in_is_indexed) ? 1
                                                                               : 0;
    const Capture::APIDumpLoader::DataChunk* indices_ptr     = (in_is_indexed) ? in_view.get_extra_chunk_ptr(0)
                                                                               : nullptr;
    const Capture::APIDumpLoader::DataChunk* vertices_ptr    = in_view.get_extra_chunk_ptr(n_indices_extra);

    D3D8GeometryTracker::MemoryRange                                 range;
    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;

    range.fvf          = m_pipeline_state.get_fvf();
    range.is_indexed   = in_is_indexed;
    range.stride       = in_stride;
    range.vertex_count = in_vertex_count;

    if (in_is_indexed)
    {
        range.index_stride = D3D8GeometryTracker::get_index_stride(in_view.get_u32(INDEX_FORMAT_ARGUMENT) );
    }

    if (indices_ptr != nullptr)
    {
        range.index_data_ptr = indices_ptr->data();
        range.index_n_bytes  = indices_ptr->size();
    }

    if (vertices_ptr != nullptr)
    {
        range.vertex_data_ptr = vertices_ptr->data();
        range.vertex_n_bytes  = vertices_ptr->size();
    }

    /* An indexed draw with no data of indices has no vertices to read. */
    if (vertices_ptr != nullptr &&
        (in_is_indexed == false || indices_ptr != nullptr) )
    {
        result_ptr = D3D8GeometryTracker::sample_memory(range);
    }

    return result_ptr;
}
