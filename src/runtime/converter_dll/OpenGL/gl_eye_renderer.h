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
#include "invisible_window_manager.h"
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
        /* Makes the second eye for an OpenGL application while the application runs.
         *
         * The application renders to the framebuffer of the left eye (see GLEyeTargets). The converter repeats each draw and each clear for the
         * right eye. For an affected draw it moves the camera for each eye: it puts the eye matrix in front of the draw's model-view matrix (see
         * StereoCore::make_eye_shift_matrix()), and it does the same for the left draw, so both eyes move by half the separation. The recipe
         * classifies each draw as affected, unaffected (made for both eyes as it is) or skipped.
         *
         * When the application renders to a texture (it copies the framebuffer into the texture), the converter also copies the framebuffer into
         * a twin of that texture for the right eye. The draws of the right eye bind the twin instead of the texture.
         *
         * Draws between glBegin and glEnd cannot be classified before glEnd. The converter compiles them into a display list that it owns and
         * calls that list for each eye when the block ends. Every call the converter makes is its own call (see OwnCallScope).
         * Not thread-safe: the converter holds its lock around every call.
         */
        class GLEyeRenderer
        {
        public:
            /* Finds the data that an argument refers to (the indices of a draw, the names of textures) in the chunks that the feeder took. */
            typedef std::function<bool(const uint32_t&, const void**, uint32_t*)> GetChunkFunc;

            /* Counters for the log. */
            struct Statistics
            {
                uint64_t n_array_draws       = 0; /* glDrawArrays and glDrawElements, made for both eyes */
                uint64_t n_camera_attached_draws = 0; /* affected draws made as they are because they are attached to the camera */
                uint64_t n_block_draws       = 0; /* blocks from glBegin to glEnd */
                uint64_t n_clears            = 0;
                uint64_t n_list_draws        = 0; /* glCallList calls */
                uint64_t n_skipped_draws     = 0;
                uint64_t n_texture_copies    = 0; /* copies of the framebuffer to a texture, made for both eyes */
                uint64_t n_twin_textures     = 0;
                uint64_t n_unmirrored_calls  = 0; /* calls that change the right eye's pixels or twin textures, and are made for the left only */
            };

            /* The core must outlive the renderer. If in_is_shown_elsewhere is set, the windows of the application are made invisible. The application
             * renders as large as fits the target, an eye of the headset (see GLResolutionScaler); a target of 0 keeps the application's size.
             */
            GLEyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                          const bool&               in_is_shown_elsewhere,
                          const uint32_t&           in_target_width,
                          const uint32_t&           in_target_height);

            /* How many frames behind the depth from take_frame_depth() is. It is 1 when the depth buffer is read into a buffer of the graphics card
             * without waiting (the result is taken at the end of the next frame). It is 0 when the depth is read at once (there are no such buffers,
             * or the small copy of the depth does not exist).
             */
            uint32_t get_depth_lag() const;

            const Statistics& get_statistics() const;

            bool is_ready() const;

            /* Takes the depth of a frame: how deep its passes were, and the x scale of the projection of the last pass. The depth buffer is read
             * before each pass is cleared and at the end of the frame. The frame is the one that just ended, or the one before it (see
             * get_depth_lag()). Returns false if the recipe does not measure depth (see Comfort::measures_depth) or the frame has no depth.
             * The depth of a frame is given once.
             */
            bool take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                  float*                    out_projection_scale_ptr);

            /* The frame is about to be presented. Reads the images of both eyes and returns those of the previous frame. Returns false if there
             * are none (the first frame, or the size changed). The images stay valid while the token inside them is held.
             */
            bool on_frame_ending(GLFrameImages* out_images_ptr);

            /* A call of the application has been made. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments_out,
                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr);

            /* A call is about to be made. Returns true if the application's call is to be made. It returns false if the converter made the call for
             * both eyes or the call is not to be made at all. A call that draws gives its class. A draw inside a display list that the application
             * compiles does not count as a draw here, because the list runs only when it is called. A pushed draw is made with the origin of its
             * viewport moved for each eye (see StereoCore::plan_push_shift()).
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        const bool&                                in_has_draws,
                        const Analysis::Recipe::DrawClass&         in_draw_class,
                        const bool&                                in_is_pushed);

            void set_get_chunk_func(const GetChunkFunc& in_get_chunk_func);

            /* Notes for the log: what was not made for the right eye and what could not be made. Each note is given once. */
            std::vector<std::string> take_notes();

        private:
            static constexpr size_t N_TEXTURE_UNITS = 32;

            /* Calls the function once for each eye, the right eye first, with the target of that eye bound. For an affected draw it also applies the matrix of that eye. */
            void play_eyes(const std::function<void()>&      in_draw_func,
                           const Analysis::Recipe::DrawClass& in_draw_class,
                           const bool&                        in_is_pushed);

            /* Makes the twin of the bound texture, with the same filter and wrap parameters. If in_has_level_size is set, it also allocates the
             * twin's image at in_level with the size and format of that level of the original. Returns false if the twin cannot be made.
             */
            bool make_twin(const uint32_t& in_texture,
                           const bool&     in_has_level_size,
                           const int32_t&  in_level);

            /* Mirrors a copy of the framebuffer into the bound texture: for the right eye the copy goes into the twin of the texture. When the
             * application is made to render larger, this function also makes the copy for the left eye, with the sizes scaled up, and returns true
             * to say that the application's own call is not to be made.
             */
            bool mirror_copy(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Applies a call that sets a parameter of the bound texture to the twin of that texture as well. */
            void mirror_texture_parameter(const APIInterceptor::APIFunction&         in_function,
                                          const uint32_t&                            in_n_arguments,
                                          const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void note_once(const std::string& in_key,
                           const std::string& in_note);

            /* Takes the results of the depth reads that were started and are still pending (see start_depth_read()), oldest first. If
             * in_only_older_frames is set, only the reads started before the current frame are taken. If in_only_the_oldest is set, only one read
             * is taken. Each result is added to the frame it belongs to.
             */
            void collect_depth_reads(const bool& in_only_older_frames,
                                     const bool& in_only_the_oldest);

            /* Forgets the pending depth reads if the targets were recreated since they started: their buffers no longer exist. */
            void forget_lost_depth_reads();

            /* Measures how deep the drawn surfaces are in a depth read (see DepthMeasurer), using the z scale and z shift of the projection. */
            Stereo::Core::DepthRange measure_depths(const float&  in_z_scale,
                                                    const float&  in_z_shift,
                                                    const float*  in_depth_ptr,
                                                    const size_t& in_n_depths);

            /* Reads the depth buffer of the left eye and records how deep the ending pass was. The result goes to the frame when the read is
             * complete (see start_depth_read()).
             */
            void measure_pass_depth();

            /* Reads the depth buffer of the left eye and waits for the result. This is used when there is no buffer of the graphics card to read into. The pass is added to the frame being made. */
            void read_depth_at_once();

            /* Starts reading the depth buffer of the left eye into a buffer of the graphics card (see GLEyeTargets::begin_depth_read()) without
             * waiting. The result is taken at the end of the next frame. If all the buffers are in use, it waits for the oldest read to free one.
             */
            void start_depth_read();

            /* Returns the data that an argument stands for (a chunk or a pointer), or nullptr if there is none. */
            const void* resolve_pointer(const APIInterceptor::APIFunctionArgument& in_argument) const;

            /* Makes the draw call for both eyes. Returns false if its arguments cannot be read; the application then makes the call itself. */
            bool on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                             const Analysis::Recipe::DrawClass&         in_draw_class,
                             const bool&                                in_is_pushed);

            /* Binds the twins (in_are_twins) or the original textures to the texture units, for the draws of one eye. */
            void select_textures(const bool& in_are_twins);

            /* Serves a glReadPixels call of the application from the picture of the left eye when the call reads one pixel and is part of a run of
             * such reads (see PictureReadbackCache). The pixel is written to the memory of the application and the call is not made. Returns false
             * if the call is to be made as it is.
             */
            bool serve_pixel_read(const uint32_t&                            in_n_arguments,
                                  const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Makes the framebuffers if they do not exist and the application has a context and a window. */
            void try_to_prepare();

            std::array<uint32_t, N_TEXTURE_UNITS> m_bound_texture_array;
            std::vector<float>                    m_depth_f32_vec;
            uint32_t                              m_depth_generation;       /* generation of the targets when the pending depth reads began (see GLEyeTargets::get_generation()) */
            uint32_t                              m_depth_lag;              /* see get_depth_lag() */
            Stereo::Core::FrameDepthLedger        m_depth_ledger;
            Stereo::Core::DepthMeasurer           m_depth_measurer;
            uint32_t                              m_active_unit;
            uint32_t                              m_applied_height; /* framebuffer size that the viewport and the scissor box were last set for */
            uint32_t                              m_applied_width;
            Analysis::Recipe::DrawClass           m_block_class;
            bool                                  m_block_has_draws;
            bool                                  m_block_is_pushed;
            uint32_t                              m_block_list;
            bool                                  m_caches_pixel_reads; /* the recipe serves one-pixel reads of a run from one picture that is read once */
            Stereo::Core::StereoCore*             m_core_ptr;
            GLFrameReader                         m_frame_reader;
            GetChunkFunc                          m_get_chunk_func;
            bool                                  m_has_failed;
            InvisibleWindowManager                m_invisible_window_manager;
            bool                                  m_is_in_app_list;
            bool                                  m_is_in_block;
            bool                                  m_is_shown_elsewhere;
            bool                                  m_measures_depth;
            uint32_t                              m_n_failed_attempts;
            std::vector<std::string>              m_note_vec;
            std::vector<std::string>              m_noted_key_vec;
            std::array<float, 16>                 m_pass_projection_f32mat; /* projection of the first affected draw of the pass */
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
