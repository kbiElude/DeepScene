/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_RESOLUTION_SCALER_H
#define STEREO_CONVERTERDLL_GL_RESOLUTION_SCALER_H

#include "common/api_function_argument.h"
#include "common/types.h"
#include "stereo/core/resolution_policy.h"
#include "stereo/core/stereo_core.h"
#include "stereo/core/types.h"
#include <array>
#include <functional>
#include <set>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Has an OpenGL application render larger than it asks (see ResolutionPolicy), without its knowing. The framebuffers of the eyes (see GlEyeTargets) are made as
         * large as the policy says, and what the application states in pixels is made to match: the viewport, the scissor box, the width of lines and the size of points
         * are scaled, and so are the textures that it makes room in for what it copies from the framebuffer (a texture whose storage it gives without pixels), with
         * the rectangles of the copies. A call of the application that is made here is not made by it (the converter says so, and the analysis still sees the call as the
         * application made it). What the application does that cannot be made to match (reading pixels, drawing them, asking how large the viewport is, giving pixels to a
         * texture it renders to) ends the application, through the core, instead of showing it wrong; the calls that do that are the ones of GlResolutionHazards.
         */
        class GlResolutionScaler
        {
        public:
            /* Where the data that an argument refers to is found (see GlEyeRenderer::GetChunkFunc). */
            typedef std::function<bool(const uint32_t&, const void**, uint32_t*)> GetChunkFunc;

            /* The core must outlive the scaler. What the application renders is made as large as fits the target (the size of an eye of the headset) with its proportions,
             * once the application's window is known (see plan()); no target, or a target that is not larger than the window, makes the scaler do nothing.
             */
            GlResolutionScaler(const uint32_t&              in_target_width,
                               const uint32_t&              in_target_height,
                               Stereo::Core::StereoCore*    in_core_ptr,
                               const Stereo::Core::LogFunc& in_log_func);

            const Stereo::Core::ResolutionPolicy& get_policy() const;

            bool is_identity() const;

            /* True if the texture has room for more pixels than the application knows of (it gave its storage without pixels, or by a copy of the framebuffer). */
            bool is_scaled_texture(const uint32_t& in_texture) const;

            /* A call of the application is about to be made, while a texture is bound (0: none). Returns true if the call was made here, scaled, and the application's is not to
             * be made. Ends the application, through the core, if the call is one that cannot be made to match.
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        const uint32_t&                            in_bound_texture,
                        const GetChunkFunc&                        in_get_chunk_func);

            /* The window of the application is this large, and the largest that a framebuffer or a texture can be is this (0: not known). Chooses the scale and says the size of
             * the framebuffers; the lines of the log say what was made larger. Ends the application if its images cannot be passed on, even at the least scale.
             */
            void plan(const uint32_t& in_asked_width,
                      const uint32_t& in_asked_height,
                      const uint32_t& in_max_extent,
                      uint32_t*       out_real_width_ptr,
                      uint32_t*       out_real_height_ptr);

            /* The numbers of a copy of the framebuffer to a texture (as the arguments of glCopyTexSubImage2D or glCopyTexImage2D are: the target, the level, then for a sub
             * image the offset in the texture and for the other the format, then the corner of what is copied, its size, and for the other the border) made as large. For a sub
             * image the texture must be a scaled one, or the application is ended. A copy to a texture that has none yet (glCopyTexImage2D) makes it one.
             */
            void scale_copy(const bool&                  in_is_sub_image,
                            const uint32_t&              in_bound_texture,
                            std::array<int32_t, 8>*      inout_value_array_ptr);

            /* The texture is gone. */
            void forget_texture(const uint32_t& in_texture);

        private:
            /* Whether the argument is a pointer that is null (the data of an upload is none). */
            static bool is_null_pointer(const APIInterceptor::APIFunctionArgument& in_argument,
                                        const GetChunkFunc&                        in_get_chunk_func);

            uint32_t                       m_asked_height;
            uint32_t                       m_asked_width;
            Stereo::Core::StereoCore*      m_core_ptr;
            Stereo::Core::LogFunc          m_log_func;
            uint32_t                       m_max_extent;
            Stereo::Core::ResolutionPolicy m_policy;
            uint32_t                       m_real_height;
            uint32_t                       m_real_width;
            std::set<uint32_t>             m_scaled_texture_set;
            uint32_t                       m_target_height;
            uint32_t                       m_target_width;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_RESOLUTION_SCALER_H */
