/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/d3d8_resolution_hazards.h"
#include "Interception/Hooks/D3D8/types.h"

bool Analysis::APIDumpAnalysis::Frontends::D3D8ResolutionHazards::classify(const APIInterceptor::APIFunction&         in_function,
                                                                           const uint32_t&                            in_n_arguments,
                                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                           ResolutionHazard*                          out_hazard_ptr)
{
    constexpr uint32_t CLEAR_N_RECTS_ARGUMENT_INDEX = 1;
    constexpr uint32_t HANDLE_BIT                   = 0x80000000u; /* a vertex shader handle has it set; what has not is a vertex format */
    constexpr uint32_t RENDER_STATE_ARGUMENT_INDEX  = 1;
    constexpr uint32_t VERTEX_SHADER_ARGUMENT_INDEX = 1;

    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR:
        {
            if (in_n_arguments                                           >  CLEAR_N_RECTS_ARGUMENT_INDEX &&
                in_arguments_ptr[CLEAR_N_RECTS_ARGUMENT_INDEX].get_u32() != 0)
            {
                *out_hazard_ptr = ResolutionHazard::CLEARS_WITH_RECTANGLES;
                result          = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_COPYRECTS:
        {
            *out_hazard_ptr = ResolutionHazard::COPIES_WITH_RECTANGLES;
            result          = true;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEPIXELSHADER:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATEVERTEXSHADER:
        {
            *out_hazard_ptr = ResolutionHazard::SHADER_CREATIONS;
            result          = true;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_GETVIEWPORT:
        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DSURFACE8_GETDESC:
        {
            *out_hazard_ptr = ResolutionHazard::SIZE_QUERIES;
            result          = true;

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE:
        {
            const uint32_t state = (in_n_arguments > RENDER_STATE_ARGUMENT_INDEX) ? in_arguments_ptr[RENDER_STATE_ARGUMENT_INDEX].get_u32()
                                                                                  : 0;

            /* The sizes of points are in pixels. */
            if (state == Interception::Hooks::D3D8::D3DRS_POINTSIZE         ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSIZE_MIN     ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSIZE_MAX     ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSPRITEENABLE ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSCALEENABLE  ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSCALE_A      ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSCALE_B      ||
                state == Interception::Hooks::D3D8::D3DRS_POINTSCALE_C)
            {
                *out_hazard_ptr = ResolutionHazard::POINT_SIZE_STATES;
                result          = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVERTEXSHADER:
        {
            const uint32_t shader = (in_n_arguments > VERTEX_SHADER_ARGUMENT_INDEX) ? in_arguments_ptr[VERTEX_SHADER_ARGUMENT_INDEX].get_u32()
                                                                                    : 0;

            /* Positions in pixels are a vertex format, not a shader. */
            if ((shader & HANDLE_BIT)                                      == 0 &&
                (shader & Interception::Hooks::D3D8::D3DFVF_POSITION_MASK) == Interception::Hooks::D3D8::D3DFVF_XYZRHW)
            {
                *out_hazard_ptr = ResolutionHazard::SCREEN_SPACE_VERTICES;
                result          = true;
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
