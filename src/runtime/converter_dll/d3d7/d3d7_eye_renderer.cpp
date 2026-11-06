/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Common/argument_reader.h"
#include "Common/callbacks.h"
#include "Common/macros.h"
#include "Common/vtable.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/methods.h"
#include "Interception/Core/D3D7Utils/strided_layout.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"
#include "stereo/d3d7/screen_parallax.h"
#include "d3d7_eye_renderer.h"
#include "environment.h"
#include "own_call_scope.h"
#include <string.h>

Stereo::ConverterDLL::D3D7EyeRenderer::D3D7EyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                                                       const D3D7WindowedMode*   in_windowed_mode_ptr)
    :m_core_ptr          (in_core_ptr),
     m_has_pixel_write   (false),
     m_is_right_eye_off  (false),
     m_n_pixel_calls     (0),
     m_n_unmirrored_calls(0),
     m_windowed_mode_ptr (in_windowed_mode_ptr)
{
    /* A debugging aid: what the right eye costs, by leaving it out. */
    m_is_right_eye_off = Environment::is_set("APISTEREO_NO_RIGHT_EYE");
}

uint64_t Stereo::ConverterDLL::D3D7EyeRenderer::get_n_failed_twins() const
{
    return m_surface_twins.get_n_failed_twins();
}

uint64_t Stereo::ConverterDLL::D3D7EyeRenderer::get_n_twins() const
{
    return m_surface_twins.get_n_twins();
}

uint64_t Stereo::ConverterDLL::D3D7EyeRenderer::get_n_pixel_calls() const
{
    return m_n_pixel_calls;
}

uint64_t Stereo::ConverterDLL::D3D7EyeRenderer::get_n_unmirrored_calls() const
{
    return m_n_unmirrored_calls;
}

void* Stereo::ConverterDLL::D3D7EyeRenderer::get_twin_ptr(void* in_surface_ptr) const
{
    return m_surface_twins.get_twin_ptr(in_surface_ptr);
}

void Stereo::ConverterDLL::D3D7EyeRenderer::on_post(const APIInterceptor::APIFunction&         in_function,
                                                    const uint32_t&                            in_n_arguments,
                                                    const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                    const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    constexpr size_t DEVICE_ARGUMENT  = 3;
    constexpr size_t SURFACE_ARGUMENT = 2;

    m_surface_twins.on_post(in_function,
                            in_n_arguments,
                            in_arguments_ptr,
                            in_returned_value_ptr);

    /* The application has written the back buffer, all of it: what it wrote is made for both eyes. */
    if (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK &&
        in_n_arguments > 0                                                          &&
        in_arguments_ptr[0].type == APIInterceptor::ARGTYPE_VOID_PTR                &&
        m_surface_twins.is_back_buffer(const_cast<void*>(in_arguments_ptr[0].get_ptr() )) )
    {
        if (m_has_pixel_write)
        {
            copy_pixel_write_to_both_eyes(const_cast<void*>(in_arguments_ptr[0].get_ptr() ));
        }

        m_has_pixel_write = false;
    }

    /* A device renders to the surface that it is made for, until the application sets another. */
    if (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEDEVICE &&
        in_n_arguments > DEVICE_ARGUMENT                                         &&
        in_arguments_ptr[DEVICE_ARGUMENT].type  == APIInterceptor::ARGTYPE_VOID_PTR &&
        in_arguments_ptr[SURFACE_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR &&
        in_arguments_ptr[DEVICE_ARGUMENT].get_ptr() != nullptr)
    {
        m_mirror.render_target_ptr = const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() );
    }

    /* A viewport is in the pixels of what it is for: another render target may be of other pixels. */
    if (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERTARGET &&
        in_n_arguments > 0                                                                &&
        in_arguments_ptr[0].type == APIInterceptor::ARGTYPE_VOID_PTR                      &&
        m_mirror.has_viewport)
    {
        set_effective_viewport(const_cast<void*>(in_arguments_ptr[0].get_ptr() ));
    }
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                   const uint32_t&                            in_n_arguments,
                                                   const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_ARGUMENTS_BLT      = 1;
    constexpr uint32_t N_ARGUMENTS_LOAD     = 2;
    constexpr uint32_t N_ARGUMENTS_SET      = 2;
    constexpr uint32_t N_ARGUMENTS_TEXTURE  = 3;
    constexpr size_t   DESTINATION_ARGUMENT = 0;
    constexpr size_t   LOAD_DESTINATION     = 1;
    constexpr uint32_t LOCK_FLAG_READONLY   = 0x10; /* DDLOCK_READONLY */
    constexpr size_t   LOCK_FLAGS_ARGUMENT  = 3;
    constexpr size_t   LOCK_RECT_ARGUMENT   = 1;
    constexpr uint32_t N_ARGUMENTS_LOCK     = 4;
    constexpr size_t   STAGE_ARGUMENT       = 1;
    constexpr size_t   TARGET_ARGUMENT      = 1;
    constexpr size_t   TEXTURE_ARGUMENT     = 2;
    constexpr size_t   VIEWPORT_ARGUMENT    = 1;

    bool result = true;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
        {
            result = play_clear(in_n_arguments,
                                in_arguments_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERTARGET:
        {
            if (in_n_arguments >= N_ARGUMENTS_SET                                     &&
                in_arguments_ptr[TARGET_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR)
            {
                m_mirror.render_target_ptr = const_cast<void*>(in_arguments_ptr[TARGET_ARGUMENT].get_ptr() );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE:
        {
            uint32_t stage = 0;

            if (in_n_arguments >= N_ARGUMENTS_TEXTURE                                  &&
                APIInterceptor::read_number(in_arguments_ptr[STAGE_ARGUMENT],
                        &stage)                                                        &&
                stage < N_TEXTURE_STAGES                                               &&
                in_arguments_ptr[TEXTURE_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR)
            {
                m_mirror.texture_ptr_array.at(stage) = const_cast<void*>(in_arguments_ptr[TEXTURE_ARGUMENT].get_ptr() );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETVIEWPORT:
        {
            const void* chunk_ptr     = nullptr;
            uint32_t    n_chunk_bytes = 0;

            /* The viewport is a structure, which the arguments hold as a chunk. */
            if (in_n_arguments > VIEWPORT_ARGUMENT                                                     &&
                in_arguments_ptr[VIEWPORT_ARGUMENT].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID      &&
                APIInterceptor::get_data_chunk(in_arguments_ptr[VIEWPORT_ARGUMENT].get_data_chunk_id(),
                                              &chunk_ptr,
                                              &n_chunk_bytes)                                           &&
                n_chunk_bytes == sizeof(Interception::Hooks::D3D7::D3DVIEWPORT7) )
            {
                m_mirror.has_viewport = true;

                ::memcpy(&m_mirror.viewport,
                         chunk_ptr,
                         sizeof(m_mirror.viewport) );

                /* The viewport of the larger back buffer is the viewport the application sets, as large as the back buffer is. */
                if (get_render_scale() != 1.0f)
                {
                    set_effective_viewport(const_cast<void*>(in_arguments_ptr[0].get_ptr() ));

                    result = false;
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
        {
            if (in_n_arguments >= N_ARGUMENTS_BLT                                              &&
                in_arguments_ptr[DESTINATION_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR)
            {
                void* const destination_ptr = const_cast<void*>(in_arguments_ptr[DESTINATION_ARGUMENT].get_ptr() );

                /* What is copied to a surface that has a twin is not copied to the twin yet. */
                if (get_twin_ptr(destination_ptr) != nullptr)
                {
                    if (mirror_blt(in_function,
                                   in_n_arguments,
                                   in_arguments_ptr) == false)
                    {
                        note_unmirrored(in_function,
                                        "a copy to a surface which has a twin, which could not be repeated");
                    }
                }

                /* A copy to the back buffer is in the pixels of the application, which the back buffer is no longer of. */
                if (m_surface_twins.is_back_buffer(destination_ptr) &&
                    m_windowed_mode_ptr->get_render_scale() != 1.0f)
                {
                    m_n_pixel_calls++;
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
        {
            if (in_n_arguments >= N_ARGUMENTS_LOAD                                              &&
                in_arguments_ptr[LOAD_DESTINATION].type == APIInterceptor::ARGTYPE_VOID_PTR      &&
                get_twin_ptr(const_cast<void*>(in_arguments_ptr[LOAD_DESTINATION].get_ptr() )) != nullptr)
            {
                note_unmirrored(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD,
                                 "a load to a surface which has a twin");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK:
        {
            /* Pixels that the application reads or writes are in the pixels of the application. A write of the whole surface is made for both eyes
             * when it ends (see copy_pixel_write_to_both_eyes()); the rest is counted, if the back buffer is larger.
             */
            if (in_n_arguments >= N_ARGUMENTS_LOCK                                             &&
                in_arguments_ptr[DESTINATION_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR &&
                m_surface_twins.is_back_buffer(const_cast<void*>(in_arguments_ptr[DESTINATION_ARGUMENT].get_ptr() )))
            {
                uint32_t   flags            = 0;
                const bool has_flags        = APIInterceptor::read_number(in_arguments_ptr[LOCK_FLAGS_ARGUMENT],
                                                                         &flags);
                const bool is_whole_surface = (get_chunk_ptr(in_arguments_ptr[LOCK_RECT_ARGUMENT]) == nullptr);

                m_has_pixel_write = (is_whole_surface                      &&
                                     has_flags                             &&
                                     (flags & LOCK_FLAG_READONLY) == 0);

                if (m_has_pixel_write == false)
                {
                    if (m_windowed_mode_ptr->get_render_scale() != 1.0f)
                    {
                        m_n_pixel_calls++;
                    }
                }
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                                                        const uint32_t&                            in_n_arguments,
                                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                        const ChunkFunc&                           in_chunk_func,
                                                        const float&                               in_picture_width)
{
    DrawArguments draw_arguments;
    bool          result         = true;
    RightEyeScope scope;

    /* A draw which the renderer does not know is not made for the right eye. */
    if (read_draw_arguments(in_function,
                            in_n_arguments,
                            in_arguments_ptr,
                            in_chunk_func,
                           &draw_arguments) == false)
    {
        note_unmirrored(in_function,
                        "a draw of a kind that the renderer does not know");

        goto end;
    }

    /* The vertices of a buffer are read, and the draw is then one of vertices in memory, which are moved like the rest. */
    if (draw_arguments.kind == DrawKind::VERTEX_BUFFER &&
        read_vertex_buffer(&draw_arguments)           == false)
    {
        note_unmirrored(in_function,
                        "a draw from a vertex buffer which cannot be read");

        goto end;
    }

    /* The right eye is drawn first, with the twins in the device; the left eye is drawn after, with what the application set. */
    if (m_is_right_eye_off == false &&
        enter_right_eye(draw_arguments.device_ptr,
                       &scope) )
    {
        draw_for_eye(Analysis::Recipe::Eye::RIGHT,
                     draw_arguments,
                     in_picture_width,
                     true); /* in_must_draw */

        leave_right_eye(draw_arguments.device_ptr,
                        scope);
    }
    else
    {
        note_unmirrored(in_function,
                                 "a draw while the device renders to a surface which has no twin");
    }

    result = (draw_for_eye(Analysis::Recipe::Eye::LEFT,
                           draw_arguments,
                           in_picture_width,
                           false) == false); /* in_must_draw */

end:
    return result;
}

std::vector<std::string> Stereo::ConverterDLL::D3D7EyeRenderer::take_unmirrored_notes()
{
    std::vector<std::string> result_vec;

    result_vec.swap(m_unmirrored_note_vec);

    return result_vec;
}

void Stereo::ConverterDLL::D3D7EyeRenderer::copy_pixel_write_to_both_eyes(void* in_back_buffer_ptr) const
{
    constexpr uint32_t BLT_FLAG_WAIT = 0x01000000; /* DDBLT_WAIT */

    void* const twin_ptr       = get_twin_ptr(in_back_buffer_ptr);
    uint32_t display_height = 0;
    uint32_t display_width  = 0;

    if (twin_ptr == nullptr)
    {
        goto end;
    }

    {
        const OwnCallScope                        own_call_scope;
        Interception::Hooks::D3D7::DDSURFACEDESC2 description    = {};
        auto* const                                surface_ptr      = static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_back_buffer_ptr);
        void** const                               surface_vtable   = APIInterceptor::get_vtable_ptr(in_back_buffer_ptr);
        auto* const                                twin_surface_ptr = static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_ptr);
        void** const                               twin_vtable      = APIInterceptor::get_vtable_ptr(twin_ptr);

        description.dwSize = static_cast<DWORD>(sizeof(description) );

        m_windowed_mode_ptr->get_display_size(&display_width,
                                              &display_height);

        if (FAILED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETSURFACEDESC>(surface_vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETSURFACEDESC)(surface_ptr,
                                                                                                                                                                                                       &description) ))
        {
            goto end;
        }

        /* The picture is in the corner of the size of the display mode, if the back buffer is larger: it goes to the twin as large as the whole, and
         * back. Otherwise it is the back buffer as it is that goes to the twin.
         */
        if (display_width  != 0                  &&
            display_height != 0                  &&
            display_width  <  description.dwWidth &&
            display_height <  description.dwHeight)
        {
            RECT picture_rect = {};

            picture_rect.bottom = static_cast<LONG>(display_height);
            picture_rect.right  = static_cast<LONG>(display_width);

            if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_BLT>(twin_vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT)(twin_surface_ptr,
                                                                                                                                                                                         nullptr,
                                                                                                                                                                                         surface_ptr,
                                                                                                                                                                                        &picture_rect,
                                                                                                                                                                                         BLT_FLAG_WAIT,
                                                                                                                                                                                         nullptr) ))
            {
                APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_BLT>(surface_vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT)(surface_ptr,
                                                                                                                                                                                       nullptr,
                                                                                                                                                                                       twin_surface_ptr,
                                                                                                                                                                                       nullptr,
                                                                                                                                                                                       BLT_FLAG_WAIT,
                                                                                                                                                                                       nullptr);
            }
        }
        else
        {
            APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_BLT>(twin_vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT)(twin_surface_ptr,
                                                                                                                                                                                     nullptr,
                                                                                                                                                                                     surface_ptr,
                                                                                                                                                                                     nullptr,
                                                                                                                                                                                     BLT_FLAG_WAIT,
                                                                                                                                                                                     nullptr);
        }
    }

end:
    return;
}

void Stereo::ConverterDLL::D3D7EyeRenderer::draw(const DrawArguments& in_draw_arguments,
                                                 const uint8_t*       in_vertices_ptr) const
{
    const OwnCallScope own_call_scope;
    void** const       vtable = APIInterceptor::get_vtable_ptr(in_draw_arguments.device_ptr);

    auto* const device_ptr = static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(in_draw_arguments.device_ptr);
    const auto  type       = static_cast<Interception::Hooks::D3D7::D3DPRIMITIVETYPE>(in_draw_arguments.primitive_type);

    if (in_draw_arguments.kind == DrawKind::STRIDED)
    {
        /* The arrays are the application's, and the draw is made as it is: positions that a camera moves are not the renderer's to move. */
        auto strided = in_draw_arguments.strided;

        if (in_draw_arguments.is_indexed)
        {
            const auto draw_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED);

            draw_func(device_ptr,
                      type,
                      in_draw_arguments.fvf,
                     &strided,
                      in_draw_arguments.n_vertices,
                      static_cast<WORD*>(const_cast<void*>(in_draw_arguments.indices_ptr) ),
                      in_draw_arguments.n_indices,
                      in_draw_arguments.flags);
        }
        else
        {
            const auto draw_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED);

            draw_func(device_ptr,
                      type,
                      in_draw_arguments.fvf,
                     &strided,
                      in_draw_arguments.n_vertices,
                      in_draw_arguments.flags);
        }
    }
    else
    if (in_draw_arguments.is_indexed)
    {
        const auto draw_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE);

        draw_func(device_ptr,
                  type,
                  in_draw_arguments.fvf,
                  const_cast<uint8_t*>(in_vertices_ptr),
                  in_draw_arguments.n_vertices,
                  static_cast<WORD*>(const_cast<void*>(in_draw_arguments.indices_ptr) ),
                  in_draw_arguments.n_indices,
                  in_draw_arguments.flags);
    }
    else
    {
        const auto draw_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_DRAWPRIMITIVE>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWPRIMITIVE);

        draw_func(device_ptr,
                  type,
                  in_draw_arguments.fvf,
                  const_cast<uint8_t*>(in_vertices_ptr),
                  in_draw_arguments.n_vertices,
                  in_draw_arguments.flags);
    }
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::draw_for_eye(const Analysis::Recipe::Eye& in_eye,
                                                         const DrawArguments&         in_draw_arguments,
                                                         const float&                 in_picture_width,
                                                         const bool&                  in_must_draw)
{
    Stereo::D3D7::ScreenParallax::Parameters parameters;
    Stereo::Core::ScreenShiftPlan            plan;
    bool                                     result         = false;
    float                                    scale          = 1.0f;
    uint32_t                                 vertex_n_bytes = 0;

    /* Vertices which a matrix moves are the matrix's. */
    if ((in_draw_arguments.fvf & Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK) == Interception::Core::D3D7Utils::D3DFVF_XYZRHW)
    {
        vertex_n_bytes = Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(in_draw_arguments.fvf);
    }

    if (vertex_n_bytes                  != 0   &&
        in_draw_arguments.n_vertices    != 0   &&
        in_draw_arguments.vertices_ptr  != nullptr)
    {
        const bool is_presented       = m_surface_twins.is_back_buffer(m_mirror.render_target_ptr);
        const bool reaches_both_sides = Stereo::D3D7::ScreenParallax::reaches_both_sides(static_cast<const uint8_t*>(in_draw_arguments.vertices_ptr),
                                                                                         vertex_n_bytes,
                                                                                         in_draw_arguments.n_vertices,
                                                                                         static_cast<const uint16_t*>(in_draw_arguments.indices_ptr),
                                                                                         in_draw_arguments.n_indices,
                                                                                         in_picture_width);

        plan = m_core_ptr->plan_screen_shift(in_eye,
                                             /* has_extent */ true,
                                             is_presented,
                                             in_picture_width,
                                             reaches_both_sides);

        /* The positions are in the pixels of the application, which a larger back buffer is not made of. */
        scale = get_render_scale();

        switch (plan.kind)
        {
            case Stereo::Core::ScreenShiftPlan::Kind::NONE:
            {
                break;
            }

            case Stereo::Core::ScreenShiftPlan::Kind::PER_VERTEX:
            {
                parameters.convergence_distance  = plan.convergence_distance;
                parameters.eye                   = plan.eye;
                parameters.eye_separation        = plan.eye_separation;
                parameters.field_of_view_degrees = plan.field_of_view_degrees;
                parameters.picture_width         = plan.picture_width;

                if (Stereo::D3D7::ScreenParallax::is_valid(parameters) == false)
                {
                    plan.kind = Stereo::Core::ScreenShiftPlan::Kind::NONE;
                }

                break;
            }

            case Stereo::Core::ScreenShiftPlan::Kind::UNIFORM:
            {
                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(plan.kind);
            }
        }

        if (plan.kind != Stereo::Core::ScreenShiftPlan::Kind::NONE ||
            scale     != 1.0f)
        {
            /* The application's own vertices are not touched: the draw is made with a copy. */
            m_vertex_vec.assign(static_cast<const uint8_t*>(in_draw_arguments.vertices_ptr),
                                static_cast<const uint8_t*>(in_draw_arguments.vertices_ptr) + static_cast<size_t>(in_draw_arguments.n_vertices) * vertex_n_bytes);

            if (plan.kind == Stereo::Core::ScreenShiftPlan::Kind::PER_VERTEX)
            {
                Stereo::D3D7::ScreenParallax::shift_vertices(parameters,
                                                             vertex_n_bytes,
                                                             in_draw_arguments.n_vertices,
                                                             m_vertex_vec.data() );
            }
            else
            if (plan.kind == Stereo::Core::ScreenShiftPlan::Kind::UNIFORM)
            {
                Stereo::D3D7::ScreenParallax::shift_vertices_by(plan.uniform_shift_pixels,
                                                                vertex_n_bytes,
                                                                in_draw_arguments.n_vertices,
                                                                m_vertex_vec.data() );
            }

            if (scale != 1.0f)
            {
                Stereo::D3D7::ScreenParallax::scale_vertices(scale,
                                                             vertex_n_bytes,
                                                             in_draw_arguments.n_vertices,
                                                             m_vertex_vec.data() );
            }

            draw(in_draw_arguments,
                 m_vertex_vec.data() );

            result = true;
        }
    }

    if (result       == false &&
        in_must_draw)
    {
        draw(in_draw_arguments,
             static_cast<const uint8_t*>(in_draw_arguments.vertices_ptr) );

        result = true;
    }

    return result;
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::enter_right_eye(void*          in_device_ptr,
                                                            RightEyeScope* out_scope_ptr) const
{
    const OwnCallScope own_call_scope;
    void** const       vtable          = APIInterceptor::get_vtable_ptr(in_device_ptr);
    bool               result          = false;
    void* const        twin_target_ptr = (m_mirror.render_target_ptr != nullptr) ? get_twin_ptr(m_mirror.render_target_ptr)
                                                                                 : nullptr;

    const auto get_viewport_func      = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_GETVIEWPORT>    (vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_GETVIEWPORT);
    const auto set_render_target_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETRENDERTARGET>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET);
    const auto set_texture_func       = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETTEXTURE>     (vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE);
    const auto set_viewport_func      = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETVIEWPORT>    (vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT);
    Interception::Hooks::D3D7::IDirect3DDevice7* const device = static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(in_device_ptr);

    *out_scope_ptr = RightEyeScope();

    if (twin_target_ptr != nullptr)
    {
        get_viewport_func(device,
                         &out_scope_ptr->viewport);

        /* Setting a render target may set the viewport to the whole of it. */
        set_render_target_func(device,
                               static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_target_ptr),
                               0); /* dwFlags */
        set_viewport_func     (device,
                              &out_scope_ptr->viewport);

        for (uint32_t n_stage = 0;
                      n_stage < N_TEXTURE_STAGES;
                    ++n_stage)
        {
            void* const twin_texture_ptr = (m_mirror.texture_ptr_array.at(n_stage) != nullptr) ? get_twin_ptr(m_mirror.texture_ptr_array.at(n_stage) )
                                                                                               : nullptr;

            if (twin_texture_ptr != nullptr)
            {
                set_texture_func(device,
                                 n_stage,
                                 static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_texture_ptr) );

                out_scope_ptr->is_texture_swapped.at(n_stage) = true;
            }
        }

        result = true;
    }

    return result;
}

float Stereo::ConverterDLL::D3D7EyeRenderer::get_render_scale() const
{
    return (m_surface_twins.is_back_buffer(m_mirror.render_target_ptr) ) ? m_windowed_mode_ptr->get_render_scale()
                                                                         : 1.0f;
}

void Stereo::ConverterDLL::D3D7EyeRenderer::leave_right_eye(void*                inout_device_ptr,
                                                            const RightEyeScope& in_scope) const
{
    const OwnCallScope own_call_scope;
    void** const       vtable = APIInterceptor::get_vtable_ptr(inout_device_ptr);

    const auto set_render_target_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETRENDERTARGET>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET);
    const auto set_texture_func       = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETTEXTURE>     (vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE);
    const auto set_viewport_func      = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETVIEWPORT>    (vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT);
    Interception::Hooks::D3D7::IDirect3DDevice7* const device = static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(inout_device_ptr);

    set_render_target_func(device,
                           static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(m_mirror.render_target_ptr),
                           0); /* dwFlags */
    set_viewport_func     (device,
                           &in_scope.viewport);

    for (uint32_t n_stage = 0;
                  n_stage < N_TEXTURE_STAGES;
                ++n_stage)
    {
        if (in_scope.is_texture_swapped.at(n_stage) )
        {
            set_texture_func(device,
                             n_stage,
                             static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(m_mirror.texture_ptr_array.at(n_stage) ));
        }
    }
}

void Stereo::ConverterDLL::D3D7EyeRenderer::note_unmirrored(const APIInterceptor::APIFunction& in_function,
                                                            const char*                        in_reason_ptr)
{
    constexpr size_t MAX_N_NOTES = 8;

    m_n_unmirrored_calls++;

    /* The first ones are told, to know what they are. */
    if (m_unmirrored_note_vec.size() < MAX_N_NOTES)
    {
        const char* const name_ptr = Interception::Core::D3D7Utils::get_function_name_ptr(in_function);

        m_unmirrored_note_vec.push_back(std::string("Not made for the right eye: ") + ((name_ptr != nullptr) ? name_ptr : "a call") + ", " + in_reason_ptr);
    }
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::mirror_blt(const APIInterceptor::APIFunction&         in_function,
                                                       const uint32_t&                            in_n_arguments,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const
{
    constexpr uint32_t N_ARGUMENTS_BLT      = 6;
    constexpr uint32_t N_ARGUMENTS_BLT_FAST = 6;
    constexpr size_t   DESTINATION_ARGUMENT = 0;

    const bool is_fast = (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST);
    bool       result  = false;

    /* The copy is made to the twin of the surface, from the twin of the surface it is made from if that has one, and as it is otherwise. */
    if (in_n_arguments >= ((is_fast) ? N_ARGUMENTS_BLT_FAST
                                     : N_ARGUMENTS_BLT)                                        &&
        in_arguments_ptr[DESTINATION_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        const size_t source_argument = (is_fast) ? 3 : 2;
        const size_t rect_argument   = (is_fast) ? 4 : 3;
        void* const  destination_ptr = const_cast<void*>(in_arguments_ptr[DESTINATION_ARGUMENT].get_ptr() );
        void* const  twin_ptr        = get_twin_ptr(destination_ptr);
        uint32_t flags = 0;
        uint32_t x     = 0;
        uint32_t y     = 0;

        if (twin_ptr                               != nullptr                         &&
            in_arguments_ptr[source_argument].type == APIInterceptor::ARGTYPE_VOID_PTR)
        {
            void*             source_ptr      = const_cast<void*>(in_arguments_ptr[source_argument].get_ptr() );
            void* const       source_twin_ptr = (source_ptr != nullptr) ? get_twin_ptr(source_ptr)
                                                                        : nullptr;
            const void* const rect_ptr        = get_chunk_ptr(in_arguments_ptr[rect_argument]);

            const OwnCallScope own_call_scope;
            void** const       vtable = APIInterceptor::get_vtable_ptr(twin_ptr);

            if (source_twin_ptr != nullptr)
            {
                source_ptr = source_twin_ptr;
            }

            if (is_fast)
            {
                if (APIInterceptor::read_number(in_arguments_ptr[1],
                            &x)     &&
                    APIInterceptor::read_number(in_arguments_ptr[2],
                            &y)     &&
                    APIInterceptor::read_number(in_arguments_ptr[5],
                            &flags))
                {
                    APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_BLTFAST>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLTFAST)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_ptr),
                                                                                                                                                                              x,
                                                                                                                                                                              y,
                                                                                                                                                                              static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(source_ptr),
                                                                                                                                                                              static_cast<const RECT*>(rect_ptr),
                                                                                                                                                                              flags);

                    result = true;
                }
            }
            else
            if (APIInterceptor::read_number(in_arguments_ptr[4],
                        &flags) )
            {
                const void* const destination_rect_ptr = get_chunk_ptr(in_arguments_ptr[1]);
                const void* const fx_ptr               = get_chunk_ptr(in_arguments_ptr[5]);

                APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_BLT>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_BLT)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_ptr),
                                                                                                                                                                  static_cast<const RECT*>(destination_rect_ptr),
                                                                                                                                                                  static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(source_ptr),
                                                                                                                                                                  static_cast<const RECT*>(rect_ptr),
                                                                                                                                                                  flags,
                                                                                                                                                                  static_cast<const Interception::Hooks::D3D7::DDBLTFX*>(fx_ptr) );


                result = true;
            }
        }
    }

    return result;
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::play_clear(const uint32_t&                            in_n_arguments,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_ARGUMENTS_CLEAR   = 7;
    constexpr size_t   COLOR_ARGUMENT      = 4;
    constexpr size_t   COUNT_ARGUMENT      = 1;
    constexpr size_t   DEVICE_ARGUMENT     = 0;
    constexpr size_t   FLAGS_ARGUMENT      = 3;
    constexpr size_t   RECTS_ARGUMENT      = 2;
    constexpr size_t   STENCIL_ARGUMENT    = 6;
    constexpr size_t   Z_ARGUMENT          = 5;

    uint32_t                                        color      = 0;
    uint32_t                                        count      = 0;
    void*                                           device_ptr = nullptr;
    uint32_t                                        flags      = 0;
    bool                                            is_scaled  = false;
    bool                                            result     = true;
    std::vector<Interception::Hooks::D3D7::D3DRECT> rect_vec;
    const Interception::Hooks::D3D7::D3DRECT*       rects_ptr  = nullptr;
    const float                                     scale      = get_render_scale();
    RightEyeScope                                   scope;
    uint32_t                                        stencil    = 0;

    if (in_n_arguments < N_ARGUMENTS_CLEAR                                           ||
        in_arguments_ptr[DEVICE_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR   ||
        in_arguments_ptr[Z_ARGUMENT].type      != APIInterceptor::ARGTYPE_F32        ||
        APIInterceptor::read_number(in_arguments_ptr[COUNT_ARGUMENT],
                &count)   == false                                                   ||
        APIInterceptor::read_number(in_arguments_ptr[FLAGS_ARGUMENT],
                &flags)   == false                                                   ||
        APIInterceptor::read_number(in_arguments_ptr[COLOR_ARGUMENT],
                &color)   == false                                                   ||
        APIInterceptor::read_number(in_arguments_ptr[STENCIL_ARGUMENT],
                &stencil) == false)
    {
        note_unmirrored(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR,
                                 "a clear with arguments which are not as expected");

        goto end;
    }

    device_ptr = const_cast<void*>(in_arguments_ptr[DEVICE_ARGUMENT].get_ptr() );

    /* The rectangles are in the memory of the application, if it clears parts of the surface. */
    if (count > 0)
    {
        if (in_arguments_ptr[RECTS_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR)
        {
            note_unmirrored(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR,
                                 "a clear with rectangles which are not in memory");

            goto end;
        }

        rects_ptr = static_cast<const Interception::Hooks::D3D7::D3DRECT*>(in_arguments_ptr[RECTS_ARGUMENT].get_ptr() );

        /* They are in the pixels of the application: the larger back buffer has more of them. */
        if (scale != 1.0f)
        {
            rect_vec.assign(rects_ptr,
                            rects_ptr + count);

            for (auto& rect : rect_vec)
            {
                rect.x1 = static_cast<LONG>(static_cast<float>(rect.x1) * scale + 0.5f);
                rect.x2 = static_cast<LONG>(static_cast<float>(rect.x2) * scale + 0.5f);
                rect.y1 = static_cast<LONG>(static_cast<float>(rect.y1) * scale + 0.5f);
                rect.y2 = static_cast<LONG>(static_cast<float>(rect.y2) * scale + 0.5f);
            }

            rects_ptr = rect_vec.data();
            is_scaled = true;
        }
    }

    {
        const auto clear_func = [&](const Interception::Hooks::D3D7::D3DRECT* in_rects_ptr)
        {
            const OwnCallScope own_call_scope;
            void** const       vtable = APIInterceptor::get_vtable_ptr(device_ptr);

            APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_CLEAR>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_CLEAR)(static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(device_ptr),
                                                                                                                                                              count,
                                                                                                                                                              in_rects_ptr,
                                                                                                                                                              flags,
                                                                                                                                                              color,
                                                                                                                                                              in_arguments_ptr[Z_ARGUMENT].get_fp32(),
                                                                                                                                                              stencil);
        };

        if (m_is_right_eye_off == false &&
            enter_right_eye(device_ptr,
                           &scope) )
        {
            clear_func(rects_ptr);

            leave_right_eye(device_ptr,
                            scope);
        }
        else
        {
            note_unmirrored(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR,
                                 "a clear while the device renders to a surface which has no twin");
        }

        /* The call of the application is for rectangles of its own pixels: the clear of the left eye is made with those of the back buffer. */
        if (is_scaled)
        {
            clear_func(rects_ptr);

            result = false;
        }
    }

end:
    return result;
}

void Stereo::ConverterDLL::D3D7EyeRenderer::set_effective_viewport(void* in_device_ptr) const
{
    const OwnCallScope                      own_call_scope;
    const float                             scale          = get_render_scale();
    Interception::Hooks::D3D7::D3DVIEWPORT7 viewport       = m_mirror.viewport;
    void** const                            vtable   = APIInterceptor::get_vtable_ptr(in_device_ptr);

    if (scale != 1.0f)
    {
        viewport.dwHeight = static_cast<DWORD>(static_cast<float>(viewport.dwHeight) * scale + 0.5f);
        viewport.dwWidth  = static_cast<DWORD>(static_cast<float>(viewport.dwWidth)  * scale + 0.5f);
        viewport.dwX      = static_cast<DWORD>(static_cast<float>(viewport.dwX)      * scale + 0.5f);
        viewport.dwY      = static_cast<DWORD>(static_cast<float>(viewport.dwY)      * scale + 0.5f);
    }

    APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETVIEWPORT>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT)(static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(in_device_ptr),
                                                                                                                                                                 &viewport);
}

const void* Stereo::ConverterDLL::D3D7EyeRenderer::get_chunk_ptr(const APIInterceptor::APIFunctionArgument& in_argument)
{
    const void* result_ptr = nullptr;
    uint32_t    n_bytes    = 0;

    if (in_argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        /* A pointer that was null is captured as a chunk with nothing in it. */
        if (APIInterceptor::get_data_chunk(in_argument.get_data_chunk_id(),
                                          &result_ptr,
                                          &n_bytes) == false ||
            n_bytes == 0)
        {
            result_ptr = nullptr;
        }
    }

    return result_ptr;
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::read_draw_arguments(const APIInterceptor::APIFunction&         in_function,
                                                                const uint32_t&                            in_n_arguments,
                                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                const ChunkFunc&                           in_chunk_func,
                                                                DrawArguments*                             out_draw_arguments_ptr)
{
    constexpr uint32_t DEVICE_ARGUMENT         = 0;
    constexpr uint32_t FVF_ARGUMENT            = 2;
    constexpr uint32_t INDICES_ARGUMENT        = 5; /* of the indexed draws */
    constexpr uint32_t N_INDICES_ARGUMENT      = 6;
    constexpr uint32_t N_VERTICES_ARGUMENT     = 4;
    constexpr uint32_t PRIMITIVE_TYPE_ARGUMENT = 1;
    constexpr uint32_t START_VERTEX_ARGUMENT   = 3; /* of the draws from a vertex buffer */
    constexpr uint32_t STRIDED_ARGUMENT        = 3; /* the structure, as a chunk */
    constexpr uint32_t VERTEX_BUFFER_ARGUMENT  = 2;
    constexpr uint32_t VERTICES_ARGUMENT       = 3;

    uint32_t flags_argument        = 0;
    bool     result                = false;
    uint32_t strided_data_argument = 0; /* the arrays of a strided draw follow the arguments of the call, as a chunk */

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE:
        {
            out_draw_arguments_ptr->is_indexed = true;
            out_draw_arguments_ptr->kind       = DrawKind::VERTICES_IN_MEMORY;
            flags_argument                     = 7;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED:
        {
            out_draw_arguments_ptr->is_indexed = true;
            out_draw_arguments_ptr->kind       = DrawKind::STRIDED;
            flags_argument                     = 7;
            strided_data_argument              = 9; /* the indices come before them */

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB:
        {
            out_draw_arguments_ptr->is_indexed = true;
            out_draw_arguments_ptr->kind       = DrawKind::VERTEX_BUFFER;
            flags_argument                     = 7;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE:
        {
            out_draw_arguments_ptr->kind = DrawKind::VERTICES_IN_MEMORY;
            flags_argument               = 5;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED:
        {
            out_draw_arguments_ptr->kind = DrawKind::STRIDED;
            flags_argument               = 5;
            strided_data_argument        = 6;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB:
        {
            out_draw_arguments_ptr->kind = DrawKind::VERTEX_BUFFER;
            flags_argument               = 5;

            break;
        }

        default:
        {
            goto end;
        }
    }

    if (in_n_arguments <= flags_argument                                                   ||
        in_arguments_ptr[DEVICE_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR)
    {
        goto end;
    }

    out_draw_arguments_ptr->device_ptr = const_cast<void*>(in_arguments_ptr[DEVICE_ARGUMENT].get_ptr() );

    if (APIInterceptor::read_number(in_arguments_ptr[PRIMITIVE_TYPE_ARGUMENT],
                                   &out_draw_arguments_ptr->primitive_type) == false ||
        APIInterceptor::read_number(in_arguments_ptr[N_VERTICES_ARGUMENT],
                                   &out_draw_arguments_ptr->n_vertices)     == false ||
        APIInterceptor::read_number(in_arguments_ptr[flags_argument],
                                   &out_draw_arguments_ptr->flags)          == false)
    {
        goto end;
    }

    switch (out_draw_arguments_ptr->kind)
    {
        case DrawKind::STRIDED:
        {
            const void* structure_ptr     = nullptr;
            uint32_t    n_structure_bytes = 0;
            const void* arrays_ptr        = nullptr;
            uint32_t    n_arrays_bytes    = 0;

            if (in_n_arguments <= strided_data_argument                                                          ||
                in_arguments_ptr[STRIDED_ARGUMENT].type        != APIInterceptor::ARGTYPE_DATA_CHUNK_ID           ||
                in_arguments_ptr[strided_data_argument].type   != APIInterceptor::ARGTYPE_DATA_CHUNK_ID           ||
                APIInterceptor::read_number(in_arguments_ptr[FVF_ARGUMENT],
                                           &out_draw_arguments_ptr->fvf)                                  == false ||
                in_chunk_func(in_arguments_ptr[STRIDED_ARGUMENT].get_data_chunk_id(),
                                              &structure_ptr,
                                              &n_structure_bytes)                                         == false ||
                in_chunk_func(in_arguments_ptr[strided_data_argument].get_data_chunk_id(),
                                              &arrays_ptr,
                                              &n_arrays_bytes)                                            == false ||
                Interception::Core::D3D7Utils::StridedLayout::rebuild(static_cast<const uint8_t*>(structure_ptr),
                                                                      n_structure_bytes,
                                                                      static_cast<const uint8_t*>(arrays_ptr),
                                                                      n_arrays_bytes,
                                                                     &out_draw_arguments_ptr->strided)    == false)
            {
                goto end;
            }

            break;
        }

        case DrawKind::VERTEX_BUFFER:
        {
            if (in_arguments_ptr[VERTEX_BUFFER_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR ||
                APIInterceptor::read_number(in_arguments_ptr[START_VERTEX_ARGUMENT],
                                           &out_draw_arguments_ptr->start_vertex) == false)
            {
                goto end;
            }

            out_draw_arguments_ptr->vertex_buffer_ptr = const_cast<void*>(in_arguments_ptr[VERTEX_BUFFER_ARGUMENT].get_ptr() );

            break;
        }

        case DrawKind::VERTICES_IN_MEMORY:
        {
            if (in_arguments_ptr[VERTICES_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR ||
                APIInterceptor::read_number(in_arguments_ptr[FVF_ARGUMENT],
                                           &out_draw_arguments_ptr->fvf) == false)
            {
                goto end;
            }

            out_draw_arguments_ptr->vertices_ptr = in_arguments_ptr[VERTICES_ARGUMENT].get_ptr();

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(out_draw_arguments_ptr->kind);
        }
    }

    if (out_draw_arguments_ptr->is_indexed)
    {
        if (in_arguments_ptr[INDICES_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR ||
            APIInterceptor::read_number(in_arguments_ptr[N_INDICES_ARGUMENT],
                                       &out_draw_arguments_ptr->n_indices) == false)
        {
            goto end;
        }

        out_draw_arguments_ptr->indices_ptr = in_arguments_ptr[INDICES_ARGUMENT].get_ptr();
    }

    result = true;

end:
    return result;
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::read_vertex_buffer(DrawArguments* inout_draw_arguments_ptr)
{
    constexpr DWORD LOCK_READ_FLAGS = 0x1 | 0x10; /* DDLOCK_WAIT | DDLOCK_READONLY */

    Interception::Hooks::D3D7::D3DVERTEXBUFFERDESC description    = {};
    void*                                          data_ptr       = nullptr;
    DWORD                                          n_bytes        = 0;
    const OwnCallScope                             own_call_scope;
    bool                                           result         = false;
    void* const                                    vertex_buffer_ptr = inout_draw_arguments_ptr->vertex_buffer_ptr;
    void** const                                   vtable            = APIInterceptor::get_vtable_ptr(vertex_buffer_ptr);

    description.dwSize = static_cast<DWORD>(sizeof(description) );

    const auto get_description_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DVERTEXBUFFER7_GETVERTEXBUFFERDESC>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DVERTEXBUFFER7_GETVERTEXBUFFERDESC);
    const auto lock_func            = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DVERTEXBUFFER7_LOCK>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DVERTEXBUFFER7_LOCK);
    const auto unlock_func          = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECT3DVERTEXBUFFER7_UNLOCK>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DVERTEXBUFFER7_UNLOCK);
    auto* const self_ptr            = static_cast<Interception::Hooks::D3D7::IDirect3DVertexBuffer7*>(vertex_buffer_ptr);

    if (get_description_func(self_ptr,
                            &description) < 0)
    {
        goto end;
    }

    {
        const uint32_t vertex_n_bytes = Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(description.dwFVF);
        const size_t   first_byte     = static_cast<size_t>(inout_draw_arguments_ptr->start_vertex) * vertex_n_bytes;
        const size_t   n_draw_bytes   = static_cast<size_t>(inout_draw_arguments_ptr->n_vertices)    * vertex_n_bytes;

        if (vertex_n_bytes == 0)
        {
            goto end;
        }

        if (lock_func(self_ptr,
                      LOCK_READ_FLAGS,
                     &data_ptr,
                     &n_bytes) < 0)
        {
            goto end;
        }

        if (first_byte + n_draw_bytes <= n_bytes)
        {
            m_buffer_vertex_vec.assign(static_cast<const uint8_t*>(data_ptr) + first_byte,
                                       static_cast<const uint8_t*>(data_ptr) + first_byte + n_draw_bytes);

            result = true;
        }

        unlock_func(self_ptr);

        if (result)
        {
            /* From the copy, the indices (which are relative to the first vertex) mean what they meant. */
            inout_draw_arguments_ptr->fvf          = description.dwFVF;
            inout_draw_arguments_ptr->kind         = DrawKind::VERTICES_IN_MEMORY;
            inout_draw_arguments_ptr->start_vertex = 0;
            inout_draw_arguments_ptr->vertices_ptr = m_buffer_vertex_vec.data();
        }
    }

end:
    return result;
}
