/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/d3d7_frontend.h"
#include "analysis/api_dump_analysis/frontends/clear_operation_factory.h"
#include "analysis/api_dump_analysis/frontends/d3d7_resource_describer.h"
#include "Capture/APIDumpLoader/api_info.h"
#include "Interception/Core/D3D7Utils/argument_info.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/guids.h"
#include "Interception/Core/D3D7Utils/methods.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"
#include "Interception/Core/D3D7Utils/value_readers.h"
#include <algorithm>
#include <string.h>

/* An argument that a draw has not got. */
static constexpr size_t NONE_ARGUMENT = static_cast<size_t>(-1);

Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::D3D7Frontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :FixedFunctionFrontend(in_builder_ptr),
     m_screen_height      (0),
     m_screen_width       (0)
{
    /* Release is a method of every interface, and tells that an object may be gone. */
    #define D3D7_METHOD(INTERFACE_TAG, interface_name, METHOD_TAG, method_name, vtable_index, return_type, parameters) \
        if (::strcmp(#method_name, "Release") == 0) \
        { \
            m_release_function_set.insert(APIInterceptor::APIFUNCTION_D3D7_##INTERFACE_TAG##_##METHOD_TAG); \
        }

    #include "Interception/Hooks/D3D7/d3d7_methods.inl"

    #undef D3D7_METHOD
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::apply_render_target(const Analysis::APIDumpAnalysis::ResourceView& in_target)
{
    const auto                              depth_iterator = m_attached_depth_map.find(Analysis::APIDumpAnalysis::get_resource_id_value(in_target.resource_id) );
    Analysis::APIDumpAnalysis::ResourceView depth_stencil_view;

    if (depth_iterator != m_attached_depth_map.end() )
    {
        depth_stencil_view = depth_iterator->second;
    }

    const bool is_target_changed = ( (m_color_view == in_target) == false);
    const bool is_changed        = (is_target_changed                               ||
                                    (m_depth_stencil_view == depth_stencil_view) == false);

    m_color_view         = in_target;
    m_depth_stencil_view = depth_stencil_view;

    if (is_changed)
    {
        Analysis::APIDumpAnalysis::AttachmentSet attachments;

        attachments.color_view_vec.push_back(m_color_view);

        attachments.depth_stencil_view = m_depth_stencil_view;

        m_builder_ptr->on_set_attachments(attachments);
    }

    /* The viewport is the whole of a new render target, until the application says otherwise. */
    if (is_target_changed)
    {
        m_builder_ptr->on_set_viewport(get_full_viewport(m_color_view) );
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::count_resolution_hazards(const CallView& in_view)
{
    constexpr size_t BLT_DESTINATION_RECT_ARGUMENT     = 1;
    constexpr size_t BLT_FAST_SOURCE_RECT_ARGUMENT     = 4;
    constexpr size_t BLT_SOURCE_RECT_ARGUMENT          = 3;
    constexpr size_t CLEAR_N_RECTS_ARGUMENT            = 1;
    constexpr size_t LOAD_DESTINATION_POINT_ARGUMENT   = 2;
    constexpr size_t LOAD_SOURCE_RECT_ARGUMENT         = 4;

    /* A rectangle or a point is in pixels. */
    const auto has_data_func = [&](const size_t& in_n_argument)
    {
        const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(in_n_argument);

        return (chunk_ptr != nullptr &&
                chunk_ptr->empty() == false);
    };

    switch (in_view.get_call().func)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
        {
            if (in_view.get_u32(CLEAR_N_RECTS_ARGUMENT) != 0)
            {
                m_n_resolution_hazard_map[Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES]++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_GETVIEWPORT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_GETDISPLAYMODE:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETSURFACEDESC:
        {
            m_n_resolution_hazard_map[Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES]++;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
        {
            if (has_data_func(LOAD_DESTINATION_POINT_ARGUMENT) ||
                has_data_func(LOAD_SOURCE_RECT_ARGUMENT) )
            {
                m_n_resolution_hazard_map[Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES]++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
        {
            if (has_data_func(BLT_DESTINATION_RECT_ARGUMENT) ||
                has_data_func(BLT_SOURCE_RECT_ARGUMENT) )
            {
                m_n_resolution_hazard_map[Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES]++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
        {
            if (has_data_func(BLT_FAST_SOURCE_RECT_ARGUMENT) )
            {
                m_n_resolution_hazard_map[Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES]++;
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

const Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::DrawLayout* Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::find_draw_layout_ptr(const APIInterceptor::APIFunction& in_function)
{
    /* The type of the primitives is the first argument of all of them. */
    static const DrawLayout draw_layout_array[] =
    {
        {6,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE,        true,  NONE_ARGUMENT, NONE_ARGUMENT},
        {6,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED, true,  3,             NONE_ARGUMENT},
        {6,             NONE_ARGUMENT, APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB,      true,  NONE_ARGUMENT, 2},
        {4,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE,               false, NONE_ARGUMENT, NONE_ARGUMENT},
        {4,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED,        false, 3,             NONE_ARGUMENT},
        {4,             NONE_ARGUMENT, APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB,             false, NONE_ARGUMENT, 2}
    };

    const DrawLayout* result_ptr = nullptr;

    for (const auto& draw_layout : draw_layout_array)
    {
        if (draw_layout.function == in_function)
        {
            result_ptr = &draw_layout;

            break;
        }
    }

    return result_ptr;
}

const char* Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_created_object_type_name_ptr(const CallView&                                in_view,
                                                                                                 const Interception::Hooks::D3D7::ArgumentInfo& in_info)
{
    uint32_t    n_iid_argument = 0;
    const char* result_ptr     = in_info.object_type_name_ptr;

    /* What is asked for decides the type of the object. */
    if (Interception::Core::D3D7Utils::get_iid_argument_index(in_view.get_call().func,
                                                             &n_iid_argument) )
    {
        const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(n_iid_argument);

        result_ptr = nullptr;

        if (chunk_ptr != nullptr)
        {
            result_ptr = Interception::Core::D3D7Utils::get_object_type_name_ptr_for_iid(chunk_ptr->data(),
                                                                                         chunk_ptr->size() );
        }
    }

    return (result_ptr != nullptr) ? result_ptr
                                   : "Object";
}

Analysis::APIDumpAnalysis::PrimitiveTopology Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_primitive_topology(const uint32_t& in_type)
{
    struct TopologyEntry
    {
        Analysis::APIDumpAnalysis::PrimitiveTopology topology;
        uint32_t                                     type;
    };

    /* D3DPT_POINTLIST to D3DPT_TRIANGLEFAN. */
    static const TopologyEntry topology_entry_array[] =
    {
        {Analysis::APIDumpAnalysis::PrimitiveTopology::POINT_LIST,     Interception::Hooks::D3D7::D3DPT_POINTLIST},
        {Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_LIST,      Interception::Hooks::D3D7::D3DPT_LINELIST},
        {Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_STRIP,     Interception::Hooks::D3D7::D3DPT_LINESTRIP},
        {Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST,  Interception::Hooks::D3D7::D3DPT_TRIANGLELIST},
        {Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_STRIP, Interception::Hooks::D3D7::D3DPT_TRIANGLESTRIP},
        {Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_FAN,   Interception::Hooks::D3D7::D3DPT_TRIANGLEFAN}
    };

    Analysis::APIDumpAnalysis::PrimitiveTopology result = Analysis::APIDumpAnalysis::PrimitiveTopology::UNKNOWN;

    for (const auto& topology_entry : topology_entry_array)
    {
        if (topology_entry.type == in_type)
        {
            result = topology_entry.topology;

            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::forget_resource(const Analysis::APIDumpAnalysis::ResourceId& in_id)
{
    m_geometry_tracker.forget(in_id);
}

Analysis::APIDumpAnalysis::Frontends::FixedFunctionState& Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_fixed_function_state()
{
    return m_pipeline_state.get_fixed_function_state();
}

Analysis::APIDumpAnalysis::ConstantSemantic Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_transform_semantic(const uint32_t& in_state) const
{
    Analysis::APIDumpAnalysis::ConstantSemantic result = Analysis::APIDumpAnalysis::ConstantSemantic::UNKNOWN;

    switch (in_state)
    {
        case Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_PROJECTION:
        {
            result = Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX;

            break;
        }

        case Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW:
        {
            result = Analysis::APIDumpAnalysis::ConstantSemantic::VIEW_MATRIX;

            break;
        }

        case Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD:
        case Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD1:
        case Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD2:
        case Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_WORLD3:
        {
            result = Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_MATRIX;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_add_attached_surface(const CallView& in_view)
{
    constexpr size_t ATTACHED_ARGUMENT = 1;
    constexpr size_t SURFACE_ARGUMENT  = 0;

    const ObjectTracker::TrackedObject* attached_ptr = m_tracker.get_object_ptr(in_view.get_pointer(ATTACHED_ARGUMENT) );
    const ObjectTracker::TrackedObject* surface_ptr  = m_tracker.get_object_ptr(in_view.get_pointer(SURFACE_ARGUMENT) );

    if (attached_ptr == nullptr ||
        surface_ptr  == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const Analysis::APIDumpAnalysis::ResourceDescription* description_ptr = find_resource_description_ptr(attached_ptr->view.resource_id);

        /* A depth buffer attached to a surface is the one that is rendered with when the surface is what is rendered to. */
        if (description_ptr != nullptr &&
            description_ptr->is_depth_stencil)
        {
            m_attached_depth_map[Analysis::APIDumpAnalysis::get_resource_id_value(surface_ptr->view.resource_id)] = attached_ptr->view;

            if (surface_ptr->view.resource_id == m_color_view.resource_id)
            {
                apply_render_target(m_color_view);
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_clear(const CallView& in_view)
{
    constexpr size_t COLOR_ARGUMENT   = 4;
    constexpr size_t COUNT_ARGUMENT   = 1;
    constexpr size_t DEPTH_ARGUMENT   = 5;
    constexpr size_t FLAGS_ARGUMENT   = 3;
    constexpr size_t STENCIL_ARGUMENT = 6;

    const uint32_t color   = in_view.get_u32(COLOR_ARGUMENT);
    const float    depth   = in_view.get_f32(DEPTH_ARGUMENT);
    const uint32_t flags   = in_view.get_u32(FLAGS_ARGUMENT);
    const uint32_t stencil = in_view.get_u32(STENCIL_ARGUMENT);

    Analysis::APIDumpAnalysis::ClearOperation clear = ClearOperationFactory::create(((flags & Interception::Core::D3D7Utils::D3DCLEAR_TARGET)  != 0),
                                                                                    ((flags & Interception::Core::D3D7Utils::D3DCLEAR_ZBUFFER) != 0),
                                                                                    ((flags & Interception::Core::D3D7Utils::D3DCLEAR_STENCIL) != 0),
                                                                                    color,
                                                                                    depth,
                                                                                    stencil);

    /* Without rectangles, the clear is of the viewport. */
    clear.has_rectangles         = (in_view.get_u32(COUNT_ARGUMENT) != 0);
    clear.is_limited_to_viewport = true;

    m_builder_ptr->on_clear(clear);
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_create_device(const CallView& in_view)
{
    constexpr size_t DEVICE_ARGUMENT  = 3;
    constexpr size_t SURFACE_ARGUMENT = 2;

    const ObjectTracker::TrackedObject* target_ptr = m_tracker.get_object_ptr(in_view.get_pointer(SURFACE_ARGUMENT) );

    if (in_view.get_out_pointer(DEVICE_ARGUMENT) == 0 ||
        target_ptr                               == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        /* A new device starts from the defaults, and renders to the surface to begin with. */
        m_pipeline_state.reset();

        m_color_view = Analysis::APIDumpAnalysis::ResourceView();

        apply_render_target(target_ptr->view);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_create_surface(const CallView& in_view)
{
    constexpr size_t DESCRIPTION_ARGUMENT = 1;
    constexpr size_t SURFACE_ARGUMENT     = 2;

    const Capture::APIDumpLoader::DataChunk*          chunk_ptr = in_view.get_chunk_ptr(DESCRIPTION_ARGUMENT);
    const uint64_t                                    address   = in_view.get_out_pointer(SURFACE_ARGUMENT);
    Interception::Core::D3D7Utils::SurfaceDescription description;
    bool                                              is_parsed = false;

    if (chunk_ptr != nullptr)
    {
        is_parsed = Interception::Core::D3D7Utils::parse_surface_description(chunk_ptr->data(),
                                                                             chunk_ptr->size(),
                                                                            &description);
    }

    if (address   == 0 ||
        is_parsed == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const auto                   resource_description = D3D7ResourceDescriber::describe_surface(description,
                                                                                                    m_screen_width,
                                                                                                    m_screen_height);
        const std::string            surface_name         = m_namer.get_name(address,
                                                                             "Surface");
        ObjectTracker::TrackedObject tracked_object;

        if ( (description.caps.dwCaps & Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE) != 0)
        {
            m_primary_description = resource_description;
        }

        tracked_object.owns_resource    = true;
        tracked_object.view.resource_id = m_builder_ptr->on_create_resource(resource_description,
                                                                            surface_name);

        m_tracker.track(address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_create_vertex_buffer(const CallView& in_view)
{
    constexpr size_t DESCRIPTION_ARGUMENT   = 1;
    constexpr size_t VERTEX_BUFFER_ARGUMENT = 2;

    const Capture::APIDumpLoader::DataChunk*       chunk_ptr = in_view.get_chunk_ptr(DESCRIPTION_ARGUMENT);
    const uint64_t                                 address   = in_view.get_out_pointer(VERTEX_BUFFER_ARGUMENT);
    Interception::Hooks::D3D7::D3DVERTEXBUFFERDESC description = {};

    if (address   == 0                    ||
        chunk_ptr == nullptr              ||
        chunk_ptr->size() != sizeof(description) )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ObjectTracker::TrackedObject tracked_object;

        memcpy(&description,
               chunk_ptr->data(),
               sizeof(description) );

        {
            const Analysis::APIDumpAnalysis::ResourceDescription resource_description = D3D7ResourceDescriber::describe_vertex_buffer(description);
            const std::string                                    vertex_buffer_name   = m_namer.get_name(address,
                                                                                                         "VertexBuffer");

            tracked_object.owns_resource    = true;
            tracked_object.view.resource_id = m_builder_ptr->on_create_resource(resource_description,
                                                                                vertex_buffer_name);

            m_vertex_buffer_fvf_map[Analysis::APIDumpAnalysis::get_resource_id_value(tracked_object.view.resource_id)] = description.dwFVF;

            m_geometry_tracker.on_create_vertex_buffer(tracked_object.view.resource_id,
                                                       resource_description.size_in_bytes);
        }

        m_tracker.track(address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_draw(const CallView&   in_view,
                                                                     const DrawLayout& in_layout)
{
    constexpr size_t PRIMITIVE_TYPE_ARGUMENT = 1;

    const uint32_t                      count        = in_view.get_u32(in_layout.count_argument);
    uint32_t                            fvf          = 0;
    Analysis::APIDumpAnalysis::ResourceId vertex_buffer_id = Analysis::APIDumpAnalysis::ResourceId::NONE;

    if (in_layout.fvf_argument != NONE_ARGUMENT)
    {
        fvf = in_view.get_u32(in_layout.fvf_argument);
    }

    if (in_layout.vertex_buffer_argument != NONE_ARGUMENT)
    {
        const ObjectTracker::TrackedObject* buffer_ptr = m_tracker.get_object_ptr(in_view.get_pointer(in_layout.vertex_buffer_argument) );

        if (buffer_ptr == nullptr)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            const auto fvf_iterator = m_vertex_buffer_fvf_map.find(Analysis::APIDumpAnalysis::get_resource_id_value(buffer_ptr->view.resource_id) );

            vertex_buffer_id = buffer_ptr->view.resource_id;

            if (fvf_iterator != m_vertex_buffer_fvf_map.end() )
            {
                fvf = fvf_iterator->second;
            }
        }
    }

    if ( (fvf & Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK) == Interception::Core::D3D7Utils::D3DFVF_XYZRHW)
    {
        m_n_resolution_hazard_map[Analysis::APIDumpAnalysis::ResolutionHazard::SCREEN_SPACE_VERTICES]++;
    }

    const Analysis::APIDumpAnalysis::PrimitiveTopology topology = get_primitive_topology(in_view.get_u32(PRIMITIVE_TYPE_ARGUMENT) );

    auto description = m_pipeline_state.describe_draw(topology,
                                                      count,
                                                      in_layout.is_indexed,
                                                      fvf,
                                                      vertex_buffer_id);

    if (m_builder_ptr->wants_geometry() )
    {
        description.geometry_ptr = sample_geometry(in_view,
                                                   in_layout,
                                                   fvf,
                                                   vertex_buffer_id);
    }

    m_builder_ptr->on_draw(description);
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_duplicate_surface(const CallView& in_view)
{
    constexpr size_t DUPLICATE_ARGUMENT = 2;
    constexpr size_t SOURCE_ARGUMENT    = 1;

    const uint64_t                      address    = in_view.get_out_pointer(DUPLICATE_ARGUMENT);
    const ObjectTracker::TrackedObject* source_ptr = m_tracker.get_object_ptr(in_view.get_pointer(SOURCE_ARGUMENT) );

    const Analysis::APIDumpAnalysis::ResourceDescription* description_ptr = (source_ptr != nullptr) ? find_resource_description_ptr(source_ptr->view.resource_id)
                                                                                                    : nullptr;

    if (address         == 0       ||
        description_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const std::string            surface_name = m_namer.get_name(address,
                                                                     "Surface");
        ObjectTracker::TrackedObject tracked_object;

        tracked_object.owns_resource    = true;
        tracked_object.view.resource_id = m_builder_ptr->on_create_resource(*description_ptr,
                                                                            surface_name);

        m_builder_ptr->on_copy(source_ptr->view,
                               tracked_object.view);

        m_tracker.track(address,
                        tracked_object);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_get_attached_surface(const CallView& in_view)
{
    constexpr size_t ATTACHED_ARGUMENT = 2;
    constexpr size_t CAPS_ARGUMENT     = 1;
    constexpr size_t SURFACE_ARGUMENT  = 0;

    const uint64_t                           address   = in_view.get_out_pointer(ATTACHED_ARGUMENT);
    const Capture::APIDumpLoader::DataChunk* caps_ptr  = in_view.get_chunk_ptr(CAPS_ARGUMENT);
    const ObjectTracker::TrackedObject*      self_ptr  = m_tracker.get_object_ptr(in_view.get_pointer(SURFACE_ARGUMENT) );
    uint32_t                                 caps      = 0;

    if (caps_ptr != nullptr &&
        caps_ptr->size() >= sizeof(caps) )
    {
        memcpy(&caps,
               caps_ptr->data(),
               sizeof(caps) );
    }

    if (address == 0)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (m_tracker.get_object_ptr(address) != nullptr)
    {
        /* The surface is known already: the chain keeps giving the same ones. */
    }
    else
    if (self_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER) != 0)
    {
        const Analysis::APIDumpAnalysis::ResourceDescription* description_ptr = find_resource_description_ptr(self_ptr->view.resource_id);
        ObjectTracker::TrackedObject                           tracked_object;

        if (description_ptr == nullptr)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            const Analysis::APIDumpAnalysis::ResourceDescription back_buffer_description = D3D7ResourceDescriber::describe_back_buffer(*description_ptr);
            const std::string                                    surface_name            = m_namer.get_name(address,
                                                                                                            "Surface");

            tracked_object.owns_resource    = true;
            tracked_object.view.resource_id = m_builder_ptr->on_create_resource(back_buffer_description,
                                                                                surface_name);

            m_tracker.track(address,
                            tracked_object);
        }
    }
    else
    if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_MIPMAP) != 0)
    {
        ObjectTracker::TrackedObject tracked_object;

        /* The next level of the texture is a part of it. */
        tracked_object.view = self_ptr->view;

        tracked_object.view.mip_level++;

        m_tracker.track(address,
                        tracked_object);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_display_mode(const CallView& in_view)
{
    constexpr size_t HEIGHT_ARGUMENT = 2;
    constexpr size_t WIDTH_ARGUMENT  = 1;

    m_screen_height = in_view.get_u32(HEIGHT_ARGUMENT);
    m_screen_width  = in_view.get_u32(WIDTH_ARGUMENT);
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_render_target(const CallView& in_view)
{
    constexpr size_t SURFACE_ARGUMENT = 1;

    const ObjectTracker::TrackedObject* target_ptr = m_tracker.get_object_ptr(in_view.get_pointer(SURFACE_ARGUMENT) );

    if (target_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        apply_render_target(target_ptr->view);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_viewport(const CallView& in_view)
{
    constexpr size_t VIEWPORT_ARGUMENT = 1;

    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(VIEWPORT_ARGUMENT);
    Interception::Hooks::D3D7::D3DVIEWPORT7  viewport  = {};

    if (chunk_ptr         == nullptr              ||
        chunk_ptr->size() != sizeof(viewport) )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        Analysis::APIDumpAnalysis::Viewport model_viewport;

        memcpy(&viewport,
               chunk_ptr->data(),
               sizeof(viewport) );

        model_viewport.height    = static_cast<float>(viewport.dwHeight);
        model_viewport.max_depth = viewport.dvMaxZ;
        model_viewport.min_depth = viewport.dvMinZ;
        model_viewport.width     = static_cast<float>(viewport.dwWidth);
        model_viewport.x         = static_cast<float>(viewport.dwX);
        model_viewport.y         = static_cast<float>(viewport.dwY);

        m_builder_ptr->on_set_viewport(model_viewport);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_unlock(const CallView& in_view)
{
    constexpr size_t OBJECT_ARGUMENT = 0;

    Analysis::APIDumpAnalysis::DataReference data;
    bool                                     is_data_found = false;
    const ObjectTracker::TrackedObject*      object_ptr    = m_tracker.get_object_ptr(in_view.get_pointer(OBJECT_ARGUMENT) );

    /* What the application has written is stored with the unlock: the data follows the arguments of the method. */
    if (object_ptr != nullptr)
    {
        is_data_found = in_view.get_data_reference(in_view.get_n_head_arguments(),
                                                  &data);
    }

    if (is_data_found == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(in_view.get_n_head_arguments() );

        if (chunk_ptr != nullptr)
        {
            m_geometry_tracker.on_unlock(object_ptr->view.resource_id,
                                         chunk_ptr->data(),
                                         chunk_ptr->size() );
        }

        m_builder_ptr->on_upload(object_ptr->view,
                                 data);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_write_to_surface(const CallView& in_view,
                                                                                 const size_t&   in_destination_argument,
                                                                                 const size_t&   in_source_argument)
{
    const ObjectTracker::TrackedObject* destination_ptr = m_tracker.get_object_ptr(in_view.get_pointer(in_destination_argument) );
    const uint64_t                      source_address  = in_view.get_pointer(in_source_argument);
    const ObjectTracker::TrackedObject* source_ptr      = m_tracker.get_object_ptr(source_address);

    if (destination_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (source_address == 0)
    {
        /* A fill with a color or a depth: the surface is written, with nothing that is in the workload. */
        m_builder_ptr->on_upload(destination_ptr->view,
                                 Analysis::APIDumpAnalysis::DataReference() );
    }
    else
    if (source_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        m_builder_ptr->on_copy(source_ptr->view,
                               destination_ptr->view);
    }
}

bool Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::is_unsupported_function(const APIInterceptor::APIFunction& in_function)
{
    /* State blocks record calls and play them later, which the front end does not do; the rest change what is on a surface or in a
     * buffer without data that the workload holds.
     */
    static const char* const unsupported_method_name_ptr_array[] =
    {
        "ApplyStateBlock",
        "BeginStateBlock",
        "BltBatch",
        "CaptureStateBlock",
        "CreateStateBlock",
        "DeleteStateBlock",
        "EndStateBlock",
        "GetDC",
        "MultiplyTransform",
        "ProcessVertices",
        "ProcessVerticesStrided",
        "ReleaseDC",
        "SetSurfaceDesc"
    };

    const Interception::Hooks::D3D7::MethodInfo* method_info_ptr = Interception::Core::D3D7Utils::get_method_info_ptr(in_function);
    bool                                         result          = false;

    if (method_info_ptr != nullptr)
    {
        for (const char* method_name_ptr : unsupported_method_name_ptr_array)
        {
            if (::strcmp(method_info_ptr->method_name_ptr,
                         method_name_ptr) == 0)
            {
                result = true;

                break;
            }
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                 const Capture::APIDumpLoader::Workload&      in_workload)
{
    const DrawLayout* draw_layout_ptr = find_draw_layout_ptr(in_call.func);
    const bool        is_release      = (m_release_function_set.find(in_call.func) != m_release_function_set.end() );
    const uint32_t    n_arguments     = Interception::Core::D3D7Utils::get_n_arguments(in_call.func);
    const CallView    view(in_call,
                           in_workload,
                           n_arguments);

    register_names           (view);
    count_resolution_hazards (view);

    /* A call which failed changed nothing. */
    if (is_release)
    {
        handle_release(view);
    }
    else
    if (view.succeeded() == false)
    {
        /* Nothing to do. */
    }
    else
    if (draw_layout_ptr != nullptr)
    {
        handle_draw(view,
                    *draw_layout_ptr);
    }
    else
    {
        switch (in_call.func)
        {
            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEDEVICE:
            {
                handle_create_device(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEVERTEXBUFFER:
            {
                handle_create_vertex_buffer(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
            {
                handle_clear(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LIGHTENABLE:
            {
                handle_light_enable(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
            {
                constexpr size_t DESTINATION_ARGUMENT = 1;
                constexpr size_t SOURCE_ARGUMENT      = 3;

                handle_write_to_surface(view,
                                        DESTINATION_ARGUMENT,
                                        SOURCE_ARGUMENT);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETLIGHT:
            {
                handle_set_light(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE:
            {
                handle_set_render_state(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERTARGET:
            {
                handle_set_render_target(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE:
            {
                handle_set_texture(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURESTAGESTATE:
            {
                handle_set_texture_stage_state(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTRANSFORM:
            {
                handle_set_transform(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETVIEWPORT:
            {
                handle_set_viewport(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DVERTEXBUFFER7_UNLOCK:
            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK:
            {
                handle_unlock(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE:
            {
                handle_create_surface(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_DUPLICATESURFACE:
            {
                handle_duplicate_surface(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE:
            {
                handle_set_display_mode(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE:
            {
                handle_add_attached_surface(view);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
            {
                constexpr size_t DESTINATION_ARGUMENT = 0;
                constexpr size_t SOURCE_ARGUMENT      = 2;

                handle_write_to_surface(view,
                                        DESTINATION_ARGUMENT,
                                        SOURCE_ARGUMENT);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
            {
                constexpr size_t DESTINATION_ARGUMENT = 0;
                constexpr size_t SOURCE_ARGUMENT      = 3;

                handle_write_to_surface(view,
                                        DESTINATION_ARGUMENT,
                                        SOURCE_ARGUMENT);

                break;
            }

            case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE:
            {
                handle_get_attached_surface(view);

                break;
            }

            default:
            {
                if (is_unsupported_function(in_call.func) )
                {
                    m_n_unsupported_calls++;
                }

                break;
            }
        }
    }

    /* Every flip is a frame, like the other tools count them, whether or not the device could show it. */
    if (Capture::APIDumpLoader::is_frame_boundary(in_call) )
    {
        m_builder_ptr->on_frame_presented();
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::register_names(const CallView& in_view)
{
    const auto& call = in_view.get_call();

    if (in_view.succeeded() )
    {
        constexpr char CREATE_PREFIX[] = "Create";

        const auto* method_info_ptr = Interception::Core::D3D7Utils::get_method_info_ptr(call.func);
        bool        is_create       = true;

        /* The exports of the DLL and the Create* methods always return new objects; Get* ones return existing objects, which may be
         * unknown to us.
         */
        if (method_info_ptr != nullptr)
        {
            const bool is_create_method     = (::strncmp(method_info_ptr->method_name_ptr,
                                                         CREATE_PREFIX,
                                                         sizeof(CREATE_PREFIX) - 1) == 0);
            const bool is_duplicate_surface = (::strcmp (method_info_ptr->method_name_ptr,
                                                         "DuplicateSurface")        == 0);

            is_create = (is_create_method     ||
                         is_duplicate_surface);
        }

        for (uint32_t n_argument = 0;
                      n_argument < call.arg_out_vec.size();
                    ++n_argument)
        {
            Interception::Hooks::D3D7::ArgumentInfo info = {};

            const bool is_info_known = Interception::Core::D3D7Utils::get_argument_info(call.func,
                                                                                        n_argument,
                                                                                       &info);

            if (is_info_known)
            {
                if (info.kind == Interception::Hooks::D3D7::ValueKind::OBJECT_OUT)
                {
                    const uint64_t address       = in_view.get_out_pointer(n_argument);
                    const char*    type_name_ptr = get_created_object_type_name_ptr(in_view,
                                                                                    info);

                    m_namer.register_object(address,
                                            type_name_ptr,
                                            is_create);
                }
            }
        }
    }
}

std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::sample_geometry(const CallView&                              in_view,
                                                                                                                                     const DrawLayout&                            in_layout,
                                                                                                                                     const uint32_t&                              in_fvf,
                                                                                                                                     const Analysis::APIDumpAnalysis::ResourceId& in_vertex_buffer_id)
{
    /* What the dumper stored with the call follows its arguments: the indices, if the draw has them, and then the vertices. A strided
     * draw has the arrays of the vertices in place of them; the stride of the positions is in the structure of the strides (a pointer
     * and a stride for each kind of vertex data, in the layout of a 32-bit process), and the first of the arrays is the positions.
     */
    constexpr size_t N_STRIDED_ARRAYS       = 12;
    constexpr size_t POSITION_STRIDE_OFFSET = sizeof(uint32_t);
    constexpr size_t START_VERTEX_ARGUMENT  = 3;
    constexpr size_t STRIDED_DATA_N_BYTES   = N_STRIDED_ARRAYS * 2 * sizeof(uint32_t);
    constexpr size_t STRIDED_HEAD_N_BYTES   = sizeof(uint32_t) * (1 + N_STRIDED_ARRAYS);

    const uint32_t                                                   position_mask = (in_fvf & Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK);
    D3D7GeometryTracker::DrawRange                                   range;
    std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> result_ptr;

    /* The positions of pre-transformed vertices are in pixels, not in the space of the model; they are sampled as that (x, y and the reciprocal
     * of the depth), for what the analysis tells of how deep the surfaces of a scene are.
     */
    const bool has_position = (position_mask != 0);

    const size_t                             n_vertices_extra = (in_layout.is_indexed) ? 1
                                                                                       : 0;
    const Capture::APIDumpLoader::DataChunk* indices_ptr      = (in_layout.is_indexed) ? in_view.get_extra_chunk_ptr(0)
                                                                                       : nullptr;
    const Capture::APIDumpLoader::DataChunk* vertices_ptr     = in_view.get_extra_chunk_ptr(n_vertices_extra);
    bool                                     is_source_found  = false;

    range.is_pre_transformed = (position_mask == Interception::Core::D3D7Utils::D3DFVF_XYZRHW);
    range.vertex_count       = in_view.get_u32(in_layout.count_argument);

    if (indices_ptr != nullptr)
    {
        range.index_data_ptr = indices_ptr->data();
        range.index_n_bytes  = indices_ptr->size();
    }

    if (has_position == false ||
        (in_layout.is_indexed && range.index_data_ptr == nullptr) )
    {
        /* Nothing to sample. */
    }
    else
    if (in_layout.vertex_buffer_argument != NONE_ARGUMENT)
    {
        const std::vector<uint8_t>* bytes_ptr = m_geometry_tracker.get_vertex_buffer_bytes_ptr(in_vertex_buffer_id);

        if (bytes_ptr != nullptr)
        {
            range.stride          = Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(in_fvf);
            range.vertex_data_ptr = bytes_ptr->data();
            range.vertex_n_bytes  = bytes_ptr->size();

            /* The start is what is added to the indices, or the first vertex. */
            if (in_layout.is_indexed)
            {
                range.base_vertex_index = in_view.get_u32(START_VERTEX_ARGUMENT);
            }
            else
            {
                range.first = in_view.get_u32(START_VERTEX_ARGUMENT);
            }

            is_source_found = true;
        }
    }
    else
    if (in_layout.strided_argument != NONE_ARGUMENT)
    {
        const Capture::APIDumpLoader::DataChunk* strides_ptr = in_view.get_chunk_ptr(in_layout.strided_argument);

        if (strides_ptr  != nullptr                         &&
            vertices_ptr != nullptr                         &&
            strides_ptr->size()  == STRIDED_DATA_N_BYTES    &&
            vertices_ptr->size() >= STRIDED_HEAD_N_BYTES)
        {
            uint32_t n_arrays         = 0;
            uint32_t n_position_bytes = 0;

            ::memcpy(&n_arrays,
                     vertices_ptr->data(),
                     sizeof(n_arrays) );
            ::memcpy(&n_position_bytes,
                     vertices_ptr->data() + sizeof(uint32_t),
                     sizeof(n_position_bytes) );
            ::memcpy(&range.stride,
                     strides_ptr->data() + POSITION_STRIDE_OFFSET,
                     sizeof(range.stride) );

            if (n_arrays                                == N_STRIDED_ARRAYS &&
                STRIDED_HEAD_N_BYTES + n_position_bytes <= vertices_ptr->size() )
            {
                range.vertex_data_ptr = vertices_ptr->data() + STRIDED_HEAD_N_BYTES;
                range.vertex_n_bytes  = n_position_bytes;
                is_source_found       = true;
            }
        }
    }
    else
    if (vertices_ptr != nullptr)
    {
        range.stride          = Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(in_fvf);
        range.vertex_data_ptr = vertices_ptr->data();
        range.vertex_n_bytes  = vertices_ptr->size();
        is_source_found       = true;
    }

    if (is_source_found)
    {
        result_ptr = D3D7GeometryTracker::sample(range);
    }

    return result_ptr;
}
