/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/argument_reader.h"
#include "gl_resolution_hazards.h"

bool Analysis::APIDumpAnalysis::Frontends::GLResolutionHazards::classify(const APIInterceptor::APIFunction&         in_function,
                                                                         const uint32_t&                            in_n_arguments,
                                                                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                         ResolutionHazard*                          out_hazard_ptr)
{
    constexpr uint32_t LINE_WIDTH   = 0x0B21;
    constexpr uint32_t POINT_SIZE   = 0x0B11;
    constexpr uint32_t SCISSOR_BOX  = 0x0C10;
    constexpr uint32_t TEXTURE_HEIGHT = 0x1001;
    constexpr uint32_t TEXTURE_WIDTH  = 0x1000;
    constexpr uint32_t VIEWPORT     = 0x0BA2;

    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLPIXELZOOM:
        {
            *out_hazard_ptr = ResolutionHazard::RASTER_OPERATIONS;
            result          = true;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETTEXIMAGE:
        case APIInterceptor::APIFUNCTION_GL_GLREADPIXELS:
        {
            *out_hazard_ptr = ResolutionHazard::PIXEL_READBACKS;
            result          = true;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETDOUBLEV:
        case APIInterceptor::APIFUNCTION_GL_GLGETFLOATV:
        case APIInterceptor::APIFUNCTION_GL_GLGETINTEGERV:
        {
            uint32_t name = 0;

            /* The application queries the state that the converter enlarged, so the answer it gets is the enlarged value. */
            if (in_n_arguments > 0                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &name)                        &&
                (name == LINE_WIDTH  ||
                 name == POINT_SIZE  ||
                 name == SCISSOR_BOX ||
                 name == VIEWPORT) )
            {
                *out_hazard_ptr = ResolutionHazard::SIZE_QUERIES;
                result          = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLGETTEXLEVELPARAMETERFV:
        case APIInterceptor::APIFUNCTION_GL_GLGETTEXLEVELPARAMETERIV:
        {
            uint32_t name = 0;

            if (in_n_arguments > 2                                       &&
                APIInterceptor::read_number(in_arguments_ptr[2],
                                           &name)                        &&
                (name == TEXTURE_HEIGHT ||
                 name == TEXTURE_WIDTH) )
            {
                *out_hazard_ptr = ResolutionHazard::SIZE_QUERIES;
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
