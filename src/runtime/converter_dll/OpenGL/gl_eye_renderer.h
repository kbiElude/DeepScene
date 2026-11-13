/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_GL_EYE_RENDERER_H

#include "analysis/recipe/types.h"
#include "common/api_function_argument.h"
#include "common/types.h"
#include "stereo/core/depth_measurer.h"
#include "stereo/core/frame_depth_ledger.h"
#include "stereo/core/picture_readback_cache.h"
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
         * (see GLEyeTargets), and each draw and each clear is made again for the right one, a draw with the camera moved for the eye (the matrix
         * of the eye, in front of the model-view matrix the draw has, see StereoCore::make_eye_shift_matrix()), the left draw as well, so that
         * both eyes are moved half the separation. What the application renders to a texture (a copy of the framebuffer) is copied for the right
         * eye to a twin of the texture, which the draws of the right eye bind in its place. A draw is told to be affected, unaffected (made for both
         * eyes as it is) or skipped by the recipe.
         *
         * The draws of OpenGL between glBegin and glEnd are not known to be affected until glEnd: they are compiled to a display list the converter
         * owns, which is called for each eye when the block ends. Everything the converter calls is its own call (see OwnCallScope). Not thread safe:
         * the converter holds its lock around every call.
         */
        class GLEyeRenderer
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
             * render as large as fits the target, an eye of the headset (see GLResolutionScaler); no target (0) leaves the size as it is.
             */
            GLEyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                          const bool&               in_is_shown_elsewhere,
                          const uint32_t&           in_target_width,
                          const uint32_t&           in_target_height);

            /* How many frames behind the depth that take_frame_depth() gives is: 1 if the depth buffer is read to a buffer of the graphics card, which is not waited for (the
             * result of a read is taken at the end of the frame after it), 0 if it is read at once (no buffers of the graphics card, or the small copy of the depth is not there).
             */
            uint32_t get_depth_lag() const;

            const Statistics& get_statistics() const;

            bool is_ready() const;

            /* How deep the passes of a frame were (the depth buffer was read before each of them was cleared, and at the end of the frame), and the x scale of the projection of
             * the last of them: of the frame that ended, or of the one before (see get_depth_lag()); false if the recipe does not have the depth measured (see
             * Comfort::measures_depth), or nothing in the frame says it. A frame's depth is given once. */
            bool take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                  float*                    out_projection_scale_ptr);

            /* The frame is about to be presented: the images of both eyes are read, and those of the frame before are given (false if there are none: the first frame, or a size
             * that changed). They stay where they are while the token in them is held. */
            bool on_frame_ending(GLFrameImages* out_images_ptr);

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

            /* Takes the results of the reads of the depth buffer that were started and are on their way (see start_depth_read()), oldest first: the ones that were started before the
             * frame that is being made, or all of them; one only, or all that qualify. Each result is added to the frame it is of.
             */
            void collect_depth_reads(const bool& in_only_older_frames,
                                     const bool& in_only_the_oldest);

            /* The reads that were on their way to buffers of the graphics card that were made again since are forgotten: they are lost with the buffers. */
            void forget_lost_depth_reads();

            /* The depths of a read of the depth buffer looked at: how deep the surfaces that were drawn are (see DepthMeasurer), for a projection with the z scale and shift. */
            Stereo::Core::DepthRange measure_depths(const float&  in_z_scale,
                                                    const float&  in_z_shift,
                                                    const float*  in_depth_ptr,
                                                    const size_t& in_n_depths);

            /* The depth buffer of the left eye is read, and what it says of how deep the pass that is ending is goes to the frame (when its result is in: see
             * start_depth_read()).
             */
            void measure_pass_depth();

            /* Reads the depth buffer of the left eye and waits for it, when there is no buffer of the graphics card to read it to: the pass is added to the frame that is being made. */
            void read_depth_at_once();

            /* Starts a read of the depth buffer of the left eye to a buffer of the graphics card (see GLEyeTargets::begin_depth_read()): the result is taken when the frame ends,
             * in the frame after it. If all the buffers are in use the oldest read is waited for, to have one free.
             */
            void start_depth_read();

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

            /* A glReadPixels of the application, given from the picture of the left eye if it is of one pixel and comes in a run of such reads (see PictureReadbackCache): the pixel
             * is written to the memory of the application, and the call is not to be made. False if the read is to be made as it is.
             */
            bool serve_pixel_read(const uint32_t&                            in_n_arguments,
                                  const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Makes the framebuffers if the application has a context and a window, and they are not there. */
            void try_to_prepare();

            std::array<uint32_t, N_TEXTURE_UNITS> m_bound_texture_array;
            std::vector<float>                    m_depth_f32_vec;
            uint32_t                              m_depth_generation;       /* of the targets that the reads that are on their way were begun with (see GLEyeTargets::get_generation()) */
            uint32_t                              m_depth_lag;              /* see get_depth_lag() */
            Stereo::Core::FrameDepthLedger        m_depth_ledger;
            Stereo::Core::DepthMeasurer           m_depth_measurer;
            uint32_t                              m_active_unit;
            uint32_t                              m_applied_height; /* of the viewport and the scissor box that were set for the size of the framebuffers */
            uint32_t                              m_applied_width;
            Analysis::Recipe::DrawClass           m_block_class;
            bool                                  m_block_has_draws;
            bool                                  m_block_is_pushed;
            uint32_t                              m_block_list;
            bool                                  m_caches_pixel_reads; /* the recipe has the reads of one pixel in a run given from a picture that is read once */
            Stereo::Core::StereoCore*             m_core_ptr;
            GLFrameReader                         m_frame_reader;
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
            Stereo::Core::PictureReadbackCache    m_readback_cache;
            GLResolutionScaler                    m_scaler;
            Statistics                            m_statistics;
            GLEyeTargets                          m_targets;
            std::map<uint32_t, uint32_t>          m_twin_map;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_RENDERER_H */
