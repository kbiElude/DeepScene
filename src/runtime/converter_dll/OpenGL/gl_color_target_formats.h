/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_COLOR_TARGET_FORMATS_H
#define STEREO_CONVERTERDLL_GL_COLOR_TARGET_FORMATS_H

#include "types.h"
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The color format of the eye render targets: the one with exactly the channels of the window of the application, as its pixel format reports them (what
         * the driver gave, not what the application asked for). An application that is given no alpha bits then gets targets that have none, and reads alpha as 1.
         */
        class GLColorTargetFormats
        {
        public:
            /* Sets out_internal_format_ptr to the renderbuffer format that has exactly the channels of in_layout. Returns false if there is none. */
            static bool find_internal_format(const GLColorLayout& in_layout,
                                             uint32_t*            out_internal_format_ptr);

            /* Reads the layout of the pixel format that is set on in_device_ptr (an HDC). Returns false if no pixel format is set on it. */
            static bool read_layout(const void*    in_device_ptr,
                                    GLColorLayout* out_layout_ptr);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_COLOR_TARGET_FORMATS_H */
