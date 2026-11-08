/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EYE_TARGETS_H
#define STEREO_CONVERTERDLL_GL_EYE_TARGETS_H

#include "analysis/recipe/types.h"
#include "gl_extension_functions.h"
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The two framebuffers (color and depth with stencil, of renderbuffers) that an application of OpenGL renders to instead of its window, one for
         * each eye. The application is given the one of the left eye and never the window, which is not drawn to, so that it does not matter if the
         * window can be seen. All calls are the converter's own and are made with the context of the application current.
         */
        class GlEyeTargets
        {
        public:
            GlEyeTargets();

            /* Makes the framebuffer of the eye the one that is drawn to and read from; false if there are none. */
            bool bind_eye(const Analysis::Recipe::Eye& in_eye);

            /* Copies the image of the left eye to the window (its back buffer), if the blit is there, and makes the framebuffer of the left eye current again. */
            void blit_left_to_window();

            /* Makes the framebuffers for the context, in the size, if there are none of that size for it: of another size the old ones are deleted, and
             * the application renders to the left one from now on. Returns false if they cannot be made.
             */
            bool ensure(const void*     in_context_ptr,
                        const uint32_t& in_width,
                        const uint32_t& in_height);

            const void* get_context_ptr() const;

            uint32_t get_height() const;

            uint32_t get_width() const;

            bool is_ready() const;

            /* Reads the image of the eye, rows from the bottom up, as R, G, B, A of 8 bits; the left eye is current afterward. */
            bool read_eye(const Analysis::Recipe::Eye& in_eye,
                          std::vector<uint8_t>*        out_rgba_u8_vec_ptr);

        private:
            struct EyeTarget
            {
                uint32_t color_renderbuffer = 0;
                uint32_t depth_renderbuffer = 0;
                uint32_t framebuffer        = 0;
            };

            bool make_target(EyeTarget* out_target_ptr);

            void release_targets();

            const void*          m_context_ptr;
            GlExtensionFunctions m_functions;
            bool                 m_has_functions;
            uint32_t             m_height;
            bool                 m_is_ready;
            EyeTarget            m_left;
            EyeTarget            m_right;
            uint32_t             m_width;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_TARGETS_H */
