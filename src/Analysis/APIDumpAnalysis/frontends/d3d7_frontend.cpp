/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/d3d7_frontend.h"
#include "Analysis/APIDumpAnalysis/frontends/clear_operation_factory.h"
#include "Analysis/APIDumpAnalysis/frontends/d3d7_resource_describer.h"
#include "Capture/APIDumpLoader/api_info.h"
#include "Interception/Core/D3D7Utils/argument_info.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/guids.h"
#include "Interception/Core/D3D7Utils/methods.h"
#include "Interception/Core/D3D7Utils/value_readers.h"
#include <algorithm>
#include <string.h>

/* An argument that a draw has not got. */
static constexpr size_t NONE_ARGUMENT = static_cast<size_t>(-1);

Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::D3D7Frontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr        (in_builder_ptr),
     m_n_unfollowed_calls (0),
     m_n_unsupported_calls(0),
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
        {6,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE,        true,  NONE_ARGUMENT},
        {6,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED, true,  NONE_ARGUMENT},
        {6,             NONE_ARGUMENT, APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB,      true,  2},
        {4,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE,               false, NONE_ARGUMENT},
        {4,             2,             APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED,        false, NONE_ARGUMENT},
        {4,             NONE_ARGUMENT, APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB,             false, 2}
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

const Analysis::APIDumpAnalysis::ResourceDescription* Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::find_resource_description_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const
{
    const uint32_t                                        index  = Analysis::APIDumpAnalysis::get_resource_id_value(in_id);
    const Analysis::APIDumpAnalysis::ResourceDescription* result_ptr = nullptr;

    if (index >= 1                                              &&
        index <= m_builder_ptr->get_model().resource_vec.size() )
    {
        result_ptr = &m_builder_ptr->get_model().resource_vec.at(index - 1).description;
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

Analysis::APIDumpAnalysis::Viewport Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_full_viewport(const Analysis::APIDumpAnalysis::ResourceView& in_view) const
{
    const Analysis::APIDumpAnalysis::ResourceDescription* description_ptr = find_resource_description_ptr(in_view.resource_id);
    Analysis::APIDumpAnalysis::Viewport                   result;

    if (description_ptr != nullptr)
    {
        /* Each mip level is half the size of the one before, down to one pixel. */
        result.height = static_cast<float>(std::max<uint32_t>(description_ptr->height >> in_view.mip_level,
                                                              1) );
        result.width  = static_cast<float>(std::max<uint32_t>(description_ptr->width  >> in_view.mip_level,
                                                              1) );
    }

    return result;
}

const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_n_resolution_hazards() const
{
    return m_n_resolution_hazard_map;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_n_unsupported_calls() const
{
    return m_n_unsupported_calls;
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

const Analysis::APIDumpAnalysis::Frontends::ObjectTracker& Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_tracker() const
{
    return m_tracker;
}

Analysis::APIDumpAnalysis::ConstantSemantic Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::get_transform_semantic(const uint32_t& in_state)
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
    constexpr size_t DEPTH_ARGUMENT   = 5;
    constexpr size_t FLAGS_ARGUMENT   = 3;
    constexpr size_t STENCIL_ARGUMENT = 6;

    const uint32_t flags = in_view.get_u32(FLAGS_ARGUMENT);

    m_builder_ptr->on_clear(ClearOperationFactory::create(((flags & Interception::Core::D3D7Utils::D3DCLEAR_TARGET)  != 0),
                                                          ((flags & Interception::Core::D3D7Utils::D3DCLEAR_ZBUFFER) != 0),
                                                          ((flags & Interception::Core::D3D7Utils::D3DCLEAR_STENCIL) != 0),
                                                          in_view.get_u32(COLOR_ARGUMENT),
                                                          in_view.get_f32(DEPTH_ARGUMENT),
                                                          in_view.get_u32(STENCIL_ARGUMENT) ));
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
        ObjectTracker::TrackedObject tracked_object;

        if ( (description.caps.dwCaps & Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE) != 0)
        {
            m_primary_description = resource_description;
        }

        tracked_object.owns_resource    = true;
        tracked_object.view.resource_id = m_builder_ptr->on_create_resource(resource_description,
                                                                            m_namer.get_name(address,
                                                                                             "Surface") );

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

        tracked_object.owns_resource    = true;
        tracked_object.view.resource_id = m_builder_ptr->on_create_resource(D3D7ResourceDescriber::describe_vertex_buffer(description),
                                                                            m_namer.get_name                          (address,
                                                                                                                       "VertexBuffer") );

        m_vertex_buffer_fvf_map[Analysis::APIDumpAnalysis::get_resource_id_value(tracked_object.view.resource_id)] = description.dwFVF;

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

    m_builder_ptr->on_draw(m_pipeline_state.describe_draw(get_primitive_topology(in_view.get_u32(PRIMITIVE_TYPE_ARGUMENT) ),
                                                          count,
                                                          in_layout.is_indexed,
                                                          fvf,
                                                          vertex_buffer_id) );
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
        ObjectTracker::TrackedObject tracked_object;

        tracked_object.owns_resource    = true;
        tracked_object.view.resource_id = m_builder_ptr->on_create_resource(*description_ptr,
                                                                            m_namer.get_name(address,
                                                                                             "Surface") );

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
            tracked_object.owns_resource    = true;
            tracked_object.view.resource_id = m_builder_ptr->on_create_resource(D3D7ResourceDescriber::describe_back_buffer(*description_ptr),
                                                                                m_namer.get_name                           (address,
                                                                                                                            "Surface") );

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

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_light_enable(const CallView& in_view)
{
    constexpr size_t ENABLE_ARGUMENT = 2;
    constexpr size_t INDEX_ARGUMENT  = 1;

    if (m_pipeline_state.set_light_enable(in_view.get_u32(INDEX_ARGUMENT),
                                          in_view.get_i32(ENABLE_ARGUMENT) != 0) == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_release(const CallView& in_view)
{
    constexpr size_t OBJECT_ARGUMENT = 0;

    const uint64_t                      address    = in_view.get_pointer(OBJECT_ARGUMENT);
    const ObjectTracker::TrackedObject* object_ptr = m_tracker.get_object_ptr(address);

    /* The last reference is gone. */
    if (object_ptr                 != nullptr &&
        in_view.get_returned_u32() == 0)
    {
        if (object_ptr->owns_resource)
        {
            m_builder_ptr->on_destroy_resource(object_ptr->view.resource_id);
        }

        m_tracker.forget(address);
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_display_mode(const CallView& in_view)
{
    constexpr size_t HEIGHT_ARGUMENT = 2;
    constexpr size_t WIDTH_ARGUMENT  = 1;

    m_screen_height = in_view.get_u32(HEIGHT_ARGUMENT);
    m_screen_width  = in_view.get_u32(WIDTH_ARGUMENT);
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_light(const CallView& in_view)
{
    constexpr size_t INDEX_ARGUMENT = 1;
    constexpr size_t LIGHT_ARGUMENT = 2;

    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(LIGHT_ARGUMENT);
    uint32_t                                 type      = 0;

    if (chunk_ptr         != nullptr                                      &&
        chunk_ptr->size() == sizeof(Interception::Hooks::D3D7::D3DLIGHT7) )
    {
        /* The type is the first member. */
        memcpy(&type,
               chunk_ptr->data(),
               sizeof(type) );

        if (m_pipeline_state.set_light(in_view.get_u32(INDEX_ARGUMENT),
                                       type) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_render_state(const CallView& in_view)
{
    constexpr size_t STATE_ARGUMENT = 1;
    constexpr size_t VALUE_ARGUMENT = 2;

    if (m_pipeline_state.set_render_state(in_view.get_u32(STATE_ARGUMENT),
                                          in_view.get_u32(VALUE_ARGUMENT) ) == false)
    {
        m_n_unfollowed_calls++;
    }
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

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_texture(const CallView& in_view)
{
    constexpr size_t STAGE_ARGUMENT   = 1;
    constexpr size_t TEXTURE_ARGUMENT = 2;

    const uint64_t                      address     = in_view.get_pointer(TEXTURE_ARGUMENT);
    const ObjectTracker::TrackedObject* texture_ptr = m_tracker.get_object_ptr(address);
    bool                                is_set      = false;

    /* No texture takes the one in the stage away. */
    if (address == 0)
    {
        is_set = m_pipeline_state.set_texture(in_view.get_u32(STAGE_ARGUMENT),
                                              Analysis::APIDumpAnalysis::ResourceView() );
    }
    else
    if (texture_ptr != nullptr)
    {
        is_set = m_pipeline_state.set_texture(in_view.get_u32(STAGE_ARGUMENT),
                                              texture_ptr->view);
    }

    if (is_set == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_texture_stage_state(const CallView& in_view)
{
    constexpr size_t STAGE_ARGUMENT = 1;
    constexpr size_t STATE_ARGUMENT = 2;
    constexpr size_t VALUE_ARGUMENT = 3;

    if (m_pipeline_state.set_texture_stage_state(in_view.get_u32(STAGE_ARGUMENT),
                                                 in_view.get_u32(STATE_ARGUMENT),
                                                 in_view.get_u32(VALUE_ARGUMENT) ) == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::D3D7Frontend::handle_set_transform(const CallView& in_view)
{
    constexpr size_t MATRIX_ARGUMENT = 2;
    constexpr size_t STATE_ARGUMENT  = 1;

    const Capture::APIDumpLoader::DataChunk* chunk_ptr = in_view.get_chunk_ptr(MATRIX_ARGUMENT);
    const uint32_t                           state     = in_view.get_u32      (STATE_ARGUMENT);

    if (chunk_ptr         == nullptr                                      ||
        chunk_ptr->size() != sizeof(Interception::Hooks::D3D7::D3DMATRIX) )
    {
        m_n_unfollowed_calls++;
    }
    else
    if (get_transform_semantic(state) != Analysis::APIDumpAnalysis::ConstantSemantic::UNKNOWN)
    {
        Analysis::APIDumpAnalysis::ConstantUpdate& update = m_transform_update_scratch;

        /* The transforms are state of the device, laid out one after the other. */
        update.location.block_kind  = Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE;
        update.location.byte_offset = static_cast<uint64_t>(state) * sizeof(Interception::Hooks::D3D7::D3DMATRIX);
        update.location.byte_size   = sizeof               (Interception::Hooks::D3D7::D3DMATRIX);
        update.provenance           = Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED;
        update.semantic             = get_transform_semantic(state);

        /* The memory of the matrix of the last call is used again. */
        update.value_u8_vec.assign(chunk_ptr->begin(),
                                   chunk_ptr->end() );

        m_builder_ptr->on_update_constant(update);
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
    const CallView    view(in_call,
                           in_workload,
                           Interception::Core::D3D7Utils::get_n_arguments(in_call.func) );

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
        const auto* method_info_ptr = Interception::Core::D3D7Utils::get_method_info_ptr(call.func);

        /* The exports of the DLL and the Create* methods always return new objects; Get* ones return existing objects, which may be
         * unknown to us.
         */
        const bool is_create = (method_info_ptr == nullptr                                                  ||
                                ::strncmp(method_info_ptr->method_name_ptr,
                                          "Create",
                                          ::strlen("Create") ) == 0                                         ||
                                ::strcmp (method_info_ptr->method_name_ptr,
                                          "DuplicateSurface")  == 0);

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
                    m_namer.register_object(in_view.get_out_pointer(n_argument),
                                            get_created_object_type_name_ptr(in_view,
                                                                             info),
                                            is_create);
                }
            }
        }
    }
}
