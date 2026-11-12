/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_GL_EYE_RENDERER_H

#include "analysis/recipe/types.h"
#include "common/api_function_argument.h"
#include "common/types.h"
#include "stereo/core/stereo_core.h"
#include "gl_eye_targets.h"
#include "gl_frame_reader.h"
#include "gl_resolution_scaler.h"
#include "invisible_windows.h"
#include "types.h"
#include <array>
#include <functional>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes the second eye for an application of OpenGL, as the application runs. The application renders to the framebuffer of the left eye
         * (see GlEyeTargets), and each draw and each clear is made again for the right one, a draw with the camera moved for the eye (the matrix
         * of the eye, in front of the model-view matrix the draw has, see StereoCore::make_eye_shift_matrix()), the left draw as well, so that
         * both eyes are moved half the separation. What the application renders to a texture (a copy of the framebuffer) is copied for the right
         * eye to a twin of the texture, which the draws of the right eye bind in its place. A draw is told to be affected, unaffected (made for both
         * eyes as it is) or skipped by the recipe.
         *
         * The draws of OpenGL between glBegin and glEnd are not known to be affected until glEnd: they are compiled to a display list the converter
         * owns, which is called for each eye when the block ends. Everything the converter calls is its own call (see OwnCallScope). Not thread safe:
         * the converter holds its lock around every call.
         */
        class GlEyeRenderer
        {
        public:
            /* Where the data that an argument refers to (the indices of a draw, the names of textures) is found: the chunks the feeder took. */
            typedef std::function<bool(const uint32_t&, const void**, uint32_t*)> GetChunkFunc;

            /* What the renderer has done, for the log. */
            struct Statistics
            {
                uint64_t n_array_draws       = 0; /* glDrawArrays and glDrawElements, made for both eyes */
                uint64_t n_camera_attached_draws = 0; /* affected draws that were made as they are, as being attached to the camera */
                uint64_t n_block_draws       = 0; /* the blocks of glBegin to glEnd                      */
                uint64_t n_clears            = 0;
                uint64_t n_list_draws        = 0; /* glCallList                                          */
                uint64_t n_skipped_draws     = 0;
                uint64_t n_texture_copies    = 0; /* copies of the framebuffer to a texture, made for both eyes */
                uint64_t n_twin_textures     = 0;
                uint64_t n_unmirrored_calls  = 0; /* calls that change the right eye's pixels or twin textures, and are made for the left only */
            };

            /* The core must outlive the renderer. If the images are shown elsewhere, the windows of the application are made invisible, and the application is made to
             * render as large as fits the target, an eye of the headset (see GlResolutionScaler); no target (0) leaves the size as it is.
             */
            GlEyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                          const bool&               in_is_shown_elsewhere,
                          const uint32_t&           in_target_width,
                          const uint32_t&           in_target_height);

            const Statistics& get_statistics() const;

            bool is_ready() const;

            /* How deep the passes of the frame that ended were (the depth buffer was read before each of them was cleared, and at the end of the frame), and the x scale of the
             * projection of the last of them; false if the recipe does not have the depth measured (see Comfort::measures_depth), or nothing in the frame says it. Starts again. */
            bool take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                  float*                    out_projection_scale_ptr);

            /* The frame is about to be presented: the images of both eyes are read, and those of the frame before are given (false if there are none: the first frame, or a size
             * that changed). They stay where they are while the token in them is held. */
            bool on_frame_ending(GlFrameImages* out_images_ptr);

            /* A call of the application has been made. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments_out,
                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr);

            /* A call of the application is about to be made; true if it is to be (false: it was made for both eyes here, or is not to be made at
             * all). A call that draws says its class. A draw in a display list that the application compiles is not made now, so it is not
             * a draw here. A draw that is pushed is made with the origin of its viewport moved for each eye (see StereoCore::plan_push_shift()).
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        const bool&                                in_has_draws,
                        const Analysis::Recipe::DrawClass&         in_draw_class,
                        const bool&                                in_is_pushed);

            void set_get_chunk_func(const GetChunkFunc& in_get_chunk_func);

            /* What was not made for the right eye, and what could not be made, said once each. */
            std::vector<std::string> take_notes();

        private:
            static constexpr size_t N_TEXTURE_UNITS = 32;

            /* Calls the function with the targets of both eyes bound in turn, the right one first; for an affected draw with the matrix of each eye. */
            void play_eyes(const std::function<void()>&      in_draw_func,
                           const Analysis::Recipe::DrawClass& in_draw_class,
                           const bool&                        in_is_pushed);

            /* Makes the twin of the texture that is bound, with the parameters of it and the size of the level (if it is not told by the call that
             * is to be mirrored); false if it cannot be made.
             */
            bool make_twin(const uint32_t& in_texture,
                           const bool&     in_has_level_size,
                           const int32_t&  in_level);

            /* The copy of the framebuffer to the bound texture, made for the right eye to the twin of the texture; if the application is made to render larger, the copy for the
             * left eye is made here too, with the numbers made larger, and the application's is not to be made (true). */
            bool mirror_copy(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* A call that sets a parameter of the bound texture, made to its twin as well. */
            void mirror_texture_parameter(const APIInterceptor::APIFunction&         in_function,
                                          const uint32_t&                            in_n_arguments,
                                          const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void note_once(const std::string& in_key,
                           const std::string& in_note);

            /* The depth buffer of the left eye is read, and what it says of how deep the pass that is ending is goes to the frame. */
            void measure_pass_depth();

            /* The data that an argument stands for (a chunk, or a pointer); nullptr if it is not there. */
            const void* resolve_pointer(const APIInterceptor::APIFunctionArgument& in_argument) const;

            /* The draw call that the converter makes for both eyes; false if its arguments cannot be read, and it is made by the application. */
            bool on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                             const Analysis::Recipe::DrawClass&         in_draw_class,
                             const bool&                                in_is_pushed);

            /* The textures bound to the units are the twins (or the textures themselves), for the draws of an eye. */
            void select_textures(const bool& in_are_twins);

            /* Makes the framebuffers if the application has a context and a window, and they are not there. */
            void try_to_prepare();

            std::array<uint32_t, N_TEXTURE_UNITS> m_bound_texture_array;
            std::vector<float>                    m_depth_f32_vec;
            Stereo::Core::DepthRange              m_frame_depth;
            float                                 m_frame_projection_scale;
            std::vector<float>                    m_frame_median_vec;
            bool                                  m_has_frame_depth;
            uint32_t                              m_active_unit;
            uint32_t                              m_applied_height; /* of the viewport and the scissor box that were set for the size of the framebuffers */
            uint32_t                              m_applied_width;
            Analysis::Recipe::DrawClass           m_block_class;
            bool                                  m_block_has_draws;
            bool                                  m_block_is_pushed;
            uint32_t                              m_block_list;
            Stereo::Core::StereoCore*             m_core_ptr;
            GlFrameReader                         m_frame_reader;
            GetChunkFunc                          m_get_chunk_func;
            bool                                  m_has_failed;
            InvisibleWindows                      m_invisible_windows;
            bool                                  m_is_in_app_list;
            bool                                  m_is_in_block;
            bool                                  m_is_shown_elsewhere;
            bool                                  m_measures_depth;
            uint32_t                              m_n_failed_attempts;
            std::vector<std::string>              m_note_vec;
            std::vector<std::string>              m_noted_key_vec;
            std::array<float, 16>                 m_pass_projection_f32mat; /* of the first draw of the pass that is affected */
            bool                                  m_pass_has_affected_draws;
            GlResolutionScaler                    m_scaler;
            Statistics                            m_statistics;
            GlEyeTargets                          m_targets;
            std::map<uint32_t, uint32_t>          m_twin_map;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_RENDERER_H */
