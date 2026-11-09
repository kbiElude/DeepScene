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
            /* Tells the calls of an OpenGL application which depend on the size of what it renders to in pixels in a way that a converter cannot make match when it renders
             * larger than the application asks, so that the application cannot be made to render larger without it noticing (see ResolutionHazard): what is read or drawn at
             * places in pixels, and what asks how large the viewport is. The state that is in pixels and can be made to match (the viewport, the scissor box, the width of lines,
             * and the copies of the framebuffer to textures) is not here; the converter makes those match.
             */
            class GlResolutionHazards
            {
            public:
                /* Returns true, and which hazard it is, if the call is one. The arguments are those the call was made with. */
                static bool classify(const APIInterceptor::APIFunction&         in_function,
                                     const uint32_t&                            in_n_arguments,
                                     const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                     ResolutionHazard*                          out_hazard_ptr);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_GL_RESOLUTION_HAZARDS_H */
