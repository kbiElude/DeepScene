/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_GL_RESOLUTION_HAZARDS_H
#define APIDUMPANALYSIS_FRONTENDS_GL_RESOLUTION_HAZARDS_H

#include "analysis/api_dump_analysis/types.h"
#include "common/api_function_argument.h"
#include "common/callbacks.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* Identifies the calls of an OpenGL application that depend on the pixel size of its render target in a way the converter cannot compensate
             * for when it renders larger than the application asked for (see ResolutionHazard). Such calls would reveal the larger size to the application:
             * reads and draws at pixel positions, and queries of the viewport size. State in pixels that the converter can keep consistent (the viewport,
             * the scissor box, the line width and the copies of the framebuffer to textures) is not listed here.
             */
            class GLResolutionHazards
            {
            public:
                /* Returns true and sets the hazard if the call is one. The arguments are the ones the call was made with. */
                static bool classify(const APIInterceptor::APIFunction&         in_function,
                                     const uint32_t&                            in_n_arguments,
                                     const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                     ResolutionHazard*                          out_hazard_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_GL_RESOLUTION_HAZARDS_H */
