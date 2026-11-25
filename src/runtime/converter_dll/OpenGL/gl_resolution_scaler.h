/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_RESOLUTION_SCALER_H
#define STEREO_CONVERTERDLL_GL_RESOLUTION_SCALER_H

#include "common/api_function_argument.h"
#include "common/types.h"
#include "stereo/core/resolution_policy.h"
#include "stereo/core/recipe_stereo_planner.h"
#include "stereo/core/types.h"
#include "types.h"
#include <array>
#include <functional>
#include <set>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes an OpenGL application render larger than it asks for (see ResolutionPolicy), without its knowledge. The framebuffers of the
         * eyes (see GLEyeTargets) get the size that the policy chooses. Everything the application states in pixels is converted to match:
         * the viewport, the scissor box, the line width and the point size are scaled. A texture that the application gives storage for without
         * pixels (a null pointer, or a buffer of zeros), to fill by copying from the framebuffer, is scaled together with the rectangles of those
         * copies. A glReadPixels of one color
         * pixel in bytes is made at the pixel of the larger framebuffer that lies under the center of the pixel the application names. When the
         * scaler makes a call itself, the application's own call is not made, and the analysis still sees the call as the application issued it.
         *
         * Anything else that cannot be made to match ends the application, through the core, instead of showing wrong images: any other pixel
         * read, drawing pixels, querying the viewport size, and giving pixels to a texture that is rendered to. GLResolutionHazards lists these calls.
         */
        class GLResolutionScaler
        {
        public:
            /* The core must outlive the scaler. Once the size of the application's window is known (see plan()), the scaler enlarges the rendering
             * as far as it fits the target (the size of one eye of the headset), keeping the proportions. It does nothing if there is no target or
             * the target is not larger than the window.
             */
            GLResolutionScaler(const uint32_t&                    in_target_width,
                               const uint32_t&                    in_target_height,
                               Stereo::Core::RecipeStereoPlanner* in_core_ptr,
                               const Stereo::Core::LogFunc&       in_log_func);

            /* The window size of the application as given to plan(), or 0 before plan() is called. */
            uint32_t get_asked_height() const;
            uint32_t get_asked_width () const;

            const Stereo::Core::ResolutionPolicy& get_policy() const;

            bool is_identity() const;

            /* True if the texture has room for more pixels than the application knows about. This happens when the application gave the storage without pixels, or by a copy of the framebuffer. */
            bool is_scaled_texture(const uint32_t& in_texture) const;

            /* A call of the application is about to be made while the given texture is bound (0: none). Returns true if the scaler made the scaled
             * call itself, so the application's call must not be made. Ends the application, through the core, if the call cannot be made to match.
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        const uint32_t&                            in_bound_texture);

            /* Takes the window size and the largest framebuffer or texture size (0: unknown). Chooses the scale and the layout, and returns the
             * framebuffer size and the picture rectangle in it. Ends the application if its images cannot be passed on, even at the smallest scale.
             */
            void plan(const uint32_t&          in_asked_width,
                      const uint32_t&          in_asked_height,
                      const uint32_t&          in_max_extent,
                      uint32_t*                out_frame_width_ptr,
                      uint32_t*                out_frame_height_ptr,
                      Stereo::Core::PixelRect* out_picture_ptr);

            /* Scales the numbers of a copy from the framebuffer to a texture (the arguments of glCopyTexSubImage2D or glCopyTexImage2D). The array
             * holds the target and the level, then for a sub image the offset in the texture, or for glCopyTexImage2D the format, then the corner
             * and the size of the copied rectangle, and for glCopyTexImage2D the border. For a sub image the texture must already be a scaled
             * texture, or the application is ended. A copy to a texture that has no storage yet (glCopyTexImage2D) makes the texture a scaled one.
             */
            void scale_copy(const bool&                  in_is_sub_image,
                            const uint32_t&              in_bound_texture,
                            std::array<int32_t, 8>*      inout_value_array_ptr);

            /* Called when the texture is deleted. */
            void forget_texture(const uint32_t& in_texture);

            /* Returns true if the call (the arguments of a glReadPixels) reads one pixel of the colors, in bytes, into memory of the application
             * (not into a buffer of the driver). This is the only read that is made for the larger framebuffer or from a picture that was read
             * (see PictureReadbackCache). The details of the read are written to out_read_ptr.
             */
            static bool parse_single_pixel_read(const uint32_t&                            in_n_arguments,
                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                SinglePixelRead*                           out_read_ptr);

        private:
            /* Makes a glReadPixels of the application (its arguments) for the larger framebuffer: reads the one pixel the application names at the
             * pixel under its center, into the application's buffer (see ResolutionPolicy::scale_pixel_coordinate(), which also moves a pixel
             * outside the window to the nearest pixel inside it). Returns false and does nothing for any other read (more than one pixel, depth,
             * floats, other colors, into a driver buffer) and when nothing is scaled. If something is scaled, the application is ended for such a read.
             */
            bool make_scaled_read(const uint32_t&                            in_n_arguments,
                                  const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const;

            /* Returns true if the unpacking of pixels starts at the first pixel of the first row and has no row length of its own (nothing is skipped,
             * the rows are as long as the image). Only then is a buffer made for the scaled size read the way the one of the application is.
             */
            bool has_default_unpack_origin() const;

            /* Returns true if the argument is a null pointer (an upload without data). The call has not been analysed yet when it is about to be
             * made, so its data is still in the chunk that the interceptor keeps.
             */
            static bool is_null_pointer(const APIInterceptor::APIFunctionArgument& in_argument);

            /* Returns true if the argument is data of which every byte is zero, and then the number of bytes. Like a null pointer it gives storage
             * without pixels: the program fills the texture by copying, and the zeros never show.
             */
            static bool is_zero_filled(const APIInterceptor::APIFunctionArgument& in_argument,
                                       uint32_t*                                  out_n_bytes_ptr);

            /* Returns true if the texture was made a scaled one only because its storage was given as a buffer of zeros, and nothing has shown yet that
             * the program renders to it. Real pixels given to such a texture make it an ordinary one again (see on_pre).
             */
            bool is_zero_storage_texture(const uint32_t& in_texture) const;

            uint32_t                           m_asked_height;
            uint32_t                           m_asked_width;
            Stereo::Core::RecipeStereoPlanner* m_core_ptr;
            uint32_t                           m_frame_height;   /* the framebuffers: the picture, or the eye image with bars */
            uint32_t                           m_frame_width;
            Stereo::Core::LogFunc              m_log_func;
            uint32_t                           m_max_extent;
            Stereo::Core::PixelRect            m_picture;        /* where the application renders to in the framebuffers */
            Stereo::Core::ResolutionPolicy     m_policy;
            uint32_t                           m_real_height;
            uint32_t                           m_real_width;
            std::set<uint32_t>                 m_scaled_texture_set;
            uint32_t                           m_target_height;
            uint32_t                           m_target_width;
            std::set<uint32_t>                 m_zero_storage_texture_set; /* the scaled textures above that began as zeros and have not been copied into */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_RESOLUTION_SCALER_H */
