/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Common/macros.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"
#include "Stereo/D3D7/screen_parallax.h"
#include "d3d7_eye_renderer.h"
#include "own_call_scope.h"
#include <string.h>

Stereo::ConverterDLL::D3D7EyeRenderer::D3D7EyeRenderer(Stereo::Core::StereoCore* in_core_ptr)
    :m_core_ptr          (in_core_ptr),
     m_n_unmirrored_calls(0)
{
    /* Stub */
}

uint64_t Stereo::ConverterDLL::D3D7EyeRenderer::get_n_failed_twins() const
{
    return m_surface_twins.get_n_failed_twins();
}

uint64_t Stereo::ConverterDLL::D3D7EyeRenderer::get_n_twins() const
{
    return m_surface_twins.get_n_twins();
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

    /* A device renders to the surface that it is made for, until the application sets another. */
    if (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEDEVICE &&
        in_n_arguments > DEVICE_ARGUMENT                                         &&
        in_arguments_ptr[DEVICE_ARGUMENT].type  == APIInterceptor::ARGTYPE_VOID_PTR &&
        in_arguments_ptr[SURFACE_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR &&
        in_arguments_ptr[DEVICE_ARGUMENT].get_ptr() != nullptr)
    {
        m_mirror.render_target_ptr = const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() );
    }
}

void Stereo::ConverterDLL::D3D7EyeRenderer::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                   const uint32_t&                            in_n_arguments,
                                                   const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_ARGUMENTS_BLT      = 1;
    constexpr uint32_t N_ARGUMENTS_LOAD     = 2;
    constexpr uint32_t N_ARGUMENTS_SET      = 2;
    constexpr uint32_t N_ARGUMENTS_TEXTURE  = 3;
    constexpr size_t   DESTINATION_ARGUMENT = 0;
    constexpr size_t   LOAD_DESTINATION     = 1;
    constexpr size_t   STAGE_ARGUMENT       = 1;
    constexpr size_t   TARGET_ARGUMENT      = 1;
    constexpr size_t   TEXTURE_ARGUMENT     = 2;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR:
        {
            play_right_clear(in_n_arguments,
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
                read_u32(in_arguments_ptr[STAGE_ARGUMENT],
                        &stage)                                                        &&
                stage < N_TEXTURE_STAGES                                               &&
                in_arguments_ptr[TEXTURE_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR)
            {
                m_mirror.texture_ptr_array.at(stage) = const_cast<void*>(in_arguments_ptr[TEXTURE_ARGUMENT].get_ptr() );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT:
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST:
        {
            /* What is copied to a surface that has a twin is not copied to the twin yet. */
            if (in_n_arguments >= N_ARGUMENTS_BLT                                              &&
                in_arguments_ptr[DESTINATION_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR &&
                get_twin_ptr(const_cast<void*>(in_arguments_ptr[DESTINATION_ARGUMENT].get_ptr() )) != nullptr)
            {
                m_n_unmirrored_calls++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_LOAD:
        {
            if (in_n_arguments >= N_ARGUMENTS_LOAD                                              &&
                in_arguments_ptr[LOAD_DESTINATION].type == APIInterceptor::ARGTYPE_VOID_PTR      &&
                get_twin_ptr(const_cast<void*>(in_arguments_ptr[LOAD_DESTINATION].get_ptr() )) != nullptr)
            {
                m_n_unmirrored_calls++;
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                                                        const uint32_t&                            in_n_arguments,
                                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                        const float&                               in_picture_width)
{
    DrawArguments  draw_arguments;
    bool           result = true;
    RightEyeScope  scope;

    /* A draw that is not of vertices in the memory of the application is not made for the right eye. */
    if (read_draw_arguments(in_function,
                            in_n_arguments,
                            in_arguments_ptr,
                           &draw_arguments) == false)
    {
        m_n_unmirrored_calls++;

        goto end;
    }

    /* The right eye is drawn first, with the twins in the device; the left eye is drawn after, with what the application set. */
    if (enter_right_eye(draw_arguments.device_ptr,
                       &scope) )
    {
        draw_for_eye(Analysis::RecipeLoader::Eye::RIGHT,
                     draw_arguments,
                     in_picture_width,
                     true); /* in_must_draw */

        leave_right_eye(draw_arguments.device_ptr,
                        scope);
    }
    else
    {
        m_n_unmirrored_calls++;
    }

    result = (draw_for_eye(Analysis::RecipeLoader::Eye::LEFT,
                           draw_arguments,
                           in_picture_width,
                           false) == false); /* in_must_draw */

end:
    return result;
}

void Stereo::ConverterDLL::D3D7EyeRenderer::draw(const DrawArguments& in_draw_arguments,
                                                 const uint8_t*       in_vertices_ptr) const
{
    const OwnCallScope own_call_scope;
    void** const       vtable = *reinterpret_cast<void***>(in_draw_arguments.device_ptr);

    if (in_draw_arguments.is_indexed)
    {
        const auto draw_func = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE]);

        draw_func(static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(in_draw_arguments.device_ptr),
                  static_cast<Interception::Hooks::D3D7::D3DPRIMITIVETYPE>(in_draw_arguments.primitive_type),
                  in_draw_arguments.fvf,
                  const_cast<uint8_t*>(in_vertices_ptr),
                  in_draw_arguments.n_vertices,
                  static_cast<WORD*>(const_cast<void*>(in_draw_arguments.indices_ptr) ),
                  in_draw_arguments.n_indices,
                  in_draw_arguments.flags);
    }
    else
    {
        const auto draw_func = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_DRAWPRIMITIVE>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_DRAWPRIMITIVE]);

        draw_func(static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(in_draw_arguments.device_ptr),
                  static_cast<Interception::Hooks::D3D7::D3DPRIMITIVETYPE>(in_draw_arguments.primitive_type),
                  in_draw_arguments.fvf,
                  const_cast<uint8_t*>(in_vertices_ptr),
                  in_draw_arguments.n_vertices,
                  in_draw_arguments.flags);
    }
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::draw_for_eye(const Analysis::RecipeLoader::Eye& in_eye,
                                                         const DrawArguments&               in_draw_arguments,
                                                         const float&                       in_picture_width,
                                                         const bool&                        in_must_draw)
{
    Stereo::D3D7::ScreenParallax::Parameters parameters;
    Stereo::Core::ScreenShiftPlan            plan;
    bool                                     result         = false;
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
        plan = m_core_ptr->plan_screen_shift(in_eye,
                                             m_surface_twins.is_back_buffer(m_mirror.render_target_ptr),
                                             in_picture_width);

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

        if (plan.kind != Stereo::Core::ScreenShiftPlan::Kind::NONE)
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
            {
                Stereo::D3D7::ScreenParallax::shift_vertices_by(plan.uniform_shift_pixels,
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
    void** const       vtable          = *reinterpret_cast<void***>(in_device_ptr);
    bool               result          = false;
    void* const        twin_target_ptr = (m_mirror.render_target_ptr != nullptr) ? get_twin_ptr(m_mirror.render_target_ptr)
                                                                                 : nullptr;

    const auto get_viewport_func      = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_GETVIEWPORT>    (vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_GETVIEWPORT]);
    const auto set_render_target_func = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETRENDERTARGET>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET]);
    const auto set_texture_func       = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETTEXTURE>     (vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE]);
    const auto set_viewport_func      = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETVIEWPORT>    (vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT]);
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

void Stereo::ConverterDLL::D3D7EyeRenderer::leave_right_eye(void*                inout_device_ptr,
                                                            const RightEyeScope& in_scope) const
{
    const OwnCallScope own_call_scope;
    void** const       vtable = *reinterpret_cast<void***>(inout_device_ptr);

    const auto set_render_target_func = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETRENDERTARGET>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETRENDERTARGET]);
    const auto set_texture_func       = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETTEXTURE>     (vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETTEXTURE]);
    const auto set_viewport_func      = reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_SETVIEWPORT>    (vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_SETVIEWPORT]);
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

void Stereo::ConverterDLL::D3D7EyeRenderer::play_right_clear(const uint32_t&                            in_n_arguments,
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

    uint32_t                                  color           = 0;
    uint32_t                                  count           = 0;
    uint32_t                                  flags           = 0;
    const Interception::Hooks::D3D7::D3DRECT* rects_ptr       = nullptr;
    RightEyeScope                             scope;
    uint32_t                                  stencil         = 0;

    if (in_n_arguments < N_ARGUMENTS_CLEAR                                           ||
        in_arguments_ptr[DEVICE_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR   ||
        in_arguments_ptr[Z_ARGUMENT].type      != APIInterceptor::ARGTYPE_F32        ||
        read_u32(in_arguments_ptr[COUNT_ARGUMENT],
                &count)   == false                                                   ||
        read_u32(in_arguments_ptr[FLAGS_ARGUMENT],
                &flags)   == false                                                   ||
        read_u32(in_arguments_ptr[COLOR_ARGUMENT],
                &color)   == false                                                   ||
        read_u32(in_arguments_ptr[STENCIL_ARGUMENT],
                &stencil) == false)
    {
        m_n_unmirrored_calls++;

        goto end;
    }

    /* The rectangles are in the memory of the application, if it clears parts of the surface. */
    if (count > 0)
    {
        if (in_arguments_ptr[RECTS_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR)
        {
            m_n_unmirrored_calls++;

            goto end;
        }

        rects_ptr = static_cast<const Interception::Hooks::D3D7::D3DRECT*>(in_arguments_ptr[RECTS_ARGUMENT].get_ptr() );
    }

    {
        void* const device_ptr = const_cast<void*>(in_arguments_ptr[DEVICE_ARGUMENT].get_ptr() );

        if (enter_right_eye(device_ptr,
                           &scope) )
        {
            const OwnCallScope own_call_scope;
            void** const       vtable = *reinterpret_cast<void***>(device_ptr);

            reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECT3DDEVICE7_CLEAR>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECT3DDEVICE7_CLEAR])(static_cast<Interception::Hooks::D3D7::IDirect3DDevice7*>(device_ptr),
                                                                                                                                                              count,
                                                                                                                                                              rects_ptr,
                                                                                                                                                              flags,
                                                                                                                                                              color,
                                                                                                                                                              in_arguments_ptr[Z_ARGUMENT].get_fp32(),
                                                                                                                                                              stencil);

            leave_right_eye(device_ptr,
                            scope);
        }
        else
        {
            m_n_unmirrored_calls++;
        }
    }

end:
    return;
}

bool Stereo::ConverterDLL::D3D7EyeRenderer::read_draw_arguments(const APIInterceptor::APIFunction&         in_function,
                                                                const uint32_t&                            in_n_arguments,
                                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                DrawArguments*                             out_draw_arguments_ptr)
{
    constexpr uint32_t DEVICE_ARGUMENT           = 0;
    constexpr uint32_t DRAW_FLAGS_ARGUMENT       = 5;
    constexpr uint32_t FVF_ARGUMENT              = 2;
    constexpr uint32_t INDEXED_FLAGS_ARGUMENT    = 7;
    constexpr uint32_t INDICES_ARGUMENT          = 5;
    constexpr uint32_t N_INDICES_ARGUMENT        = 6;
    constexpr uint32_t N_VERTICES_ARGUMENT       = 4;
    constexpr uint32_t PRIMITIVE_TYPE_ARGUMENT   = 1;
    constexpr uint32_t VERTICES_ARGUMENT         = 3;

    const bool is_indexed = (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE);
    bool       result     = false;

    if (is_indexed == false                                                          &&
        in_function != APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE)
    {
        goto end;
    }

    if (in_n_arguments <= ((is_indexed) ? INDEXED_FLAGS_ARGUMENT
                                        : DRAW_FLAGS_ARGUMENT) )
    {
        goto end;
    }

    if (in_arguments_ptr[DEVICE_ARGUMENT].type   != APIInterceptor::ARGTYPE_VOID_PTR ||
        in_arguments_ptr[VERTICES_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR)
    {
        goto end;
    }

    out_draw_arguments_ptr->device_ptr   = const_cast<void*>(in_arguments_ptr[DEVICE_ARGUMENT].get_ptr() );
    out_draw_arguments_ptr->is_indexed   = is_indexed;
    out_draw_arguments_ptr->vertices_ptr = in_arguments_ptr[VERTICES_ARGUMENT].get_ptr();

    if (read_u32(in_arguments_ptr[PRIMITIVE_TYPE_ARGUMENT],
                &out_draw_arguments_ptr->primitive_type) == false ||
        read_u32(in_arguments_ptr[FVF_ARGUMENT],
                &out_draw_arguments_ptr->fvf)            == false ||
        read_u32(in_arguments_ptr[N_VERTICES_ARGUMENT],
                &out_draw_arguments_ptr->n_vertices)     == false ||
        read_u32(in_arguments_ptr[(is_indexed) ? INDEXED_FLAGS_ARGUMENT
                                               : DRAW_FLAGS_ARGUMENT],
                &out_draw_arguments_ptr->flags)          == false)
    {
        goto end;
    }

    if (is_indexed)
    {
        if (in_arguments_ptr[INDICES_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR ||
            read_u32(in_arguments_ptr[N_INDICES_ARGUMENT],
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

bool Stereo::ConverterDLL::D3D7EyeRenderer::read_u32(const APIInterceptor::APIFunctionArgument& in_argument,
                                                     uint32_t*                                  out_value_ptr)
{
    bool result = true;

    switch (in_argument.type)
    {
        case APIInterceptor::ARGTYPE_I32:
        {
            *out_value_ptr = static_cast<uint32_t>(in_argument.get_i32() );

            break;
        }

        case APIInterceptor::ARGTYPE_U32:
        {
            *out_value_ptr = in_argument.get_u32();

            break;
        }

        default:
        {
            result = false;

            break;
        }
    }

    return result;
}
