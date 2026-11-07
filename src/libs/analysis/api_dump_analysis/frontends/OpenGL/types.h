/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the OpenGL front end keeps of a context.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H

#include <array>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* A rectangle of the framebuffer, in pixels from its lower left corner, as glViewport and glScissor say it. */
            struct GlBox
            {
                int32_t height = 0;
                int32_t width  = 0;
                int32_t x      = 0;
                int32_t y      = 0;
            };

            /* The state of one OpenGL context, from the calls that set it (the values a context starts with are those of the specification). Follows GlBox. */
            struct GlContextState
            {
                std::array<float, 4> clear_color_fp32vec4 = {};
                double               clear_depth          = 1.0;
                uint32_t             clear_stencil        = 0;
                bool                 is_scissor_enabled   = false;
                GlBox                scissor_box;
                GlBox                viewport;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_TYPES_H */
