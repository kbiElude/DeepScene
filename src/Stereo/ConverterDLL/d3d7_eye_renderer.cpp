/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Common/macros.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Core/D3D7Utils/vertex_format.h"
#include "Interception/Hooks/D3D7/types.h"
#include "Stereo/D3D7/screen_parallax.h"
#include "d3d7_eye_renderer.h"
#include "own_call_scope.h"
#include <string.h>

Stereo::ConverterDLL::D3D7EyeRenderer::D3D7EyeRenderer(Stereo::Core::StereoCore* in_core_ptr)
    :m_core_ptr(in_core_ptr)
{
    /* Stub */
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

bool Stereo::ConverterDLL::D3D7EyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                                                        const uint32_t&                            in_n_arguments,
                                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                        const float&                               in_picture_width)
{
    DrawArguments                                  draw_arguments;
    Stereo::D3D7::ScreenParallax::Parameters       parameters;
    Stereo::Core::ScreenShiftPlan                  plan;
    bool                                           result         = true;
    uint32_t                                       vertex_n_bytes = 0;

    if (read_draw_arguments(in_function,
                            in_n_arguments,
                            in_arguments_ptr,
                           &draw_arguments) == false)
    {
        goto end;
    }

    /* Vertices which a matrix moves are the matrix's. */
    if ((draw_arguments.fvf & Interception::Core::D3D7Utils::D3DFVF_POSITION_MASK) != Interception::Core::D3D7Utils::D3DFVF_XYZRHW)
    {
        goto end;
    }

    vertex_n_bytes = Interception::Core::D3D7Utils::VertexFormat::get_n_vertex_bytes(draw_arguments.fvf);

    if (vertex_n_bytes == 0                ||
        draw_arguments.n_vertices == 0     ||
        draw_arguments.vertices_ptr == nullptr)
    {
        goto end;
    }

    plan = m_core_ptr->plan_screen_shift(Analysis::RecipeLoader::Eye::LEFT,
                                         false, /* in_is_presented */
                                         in_picture_width);

    switch (plan.kind)
    {
        case Stereo::Core::ScreenShiftPlan::Kind::NONE:
        {
            goto end;
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
                goto end;
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

    /* The application's own vertices are not touched: the draw is made with a copy. */
    m_vertex_vec.assign(static_cast<const uint8_t*>(draw_arguments.vertices_ptr),
                        static_cast<const uint8_t*>(draw_arguments.vertices_ptr) + static_cast<size_t>(draw_arguments.n_vertices) * vertex_n_bytes);

    if (plan.kind == Stereo::Core::ScreenShiftPlan::Kind::PER_VERTEX)
    {
        Stereo::D3D7::ScreenParallax::shift_vertices(parameters,
                                                     vertex_n_bytes,
                                                     draw_arguments.n_vertices,
                                                     m_vertex_vec.data() );
    }
    else
    {
        Stereo::D3D7::ScreenParallax::shift_vertices_by(plan.uniform_shift_pixels,
                                                        vertex_n_bytes,
                                                        draw_arguments.n_vertices,
                                                        m_vertex_vec.data() );
    }

    draw(draw_arguments,
         m_vertex_vec.data() );

    result = false;

end:
    return result;
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
