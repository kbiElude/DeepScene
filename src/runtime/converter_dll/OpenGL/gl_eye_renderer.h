/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_EYE_RENDERER_H
#define STEREO_CONVERTERDLL_GL_EYE_RENDERER_H

#include "analysis/recipe/types.h"
#include "common/api_function_argument.h"
#include "common/types.h"
#include "stereo/core/fast_depth_range_measurer.h"
#include "stereo/core/frame_depth_ledger.h"
#include "stereo/core/picture_readback_cache.h"
#include "stereo/core/recipe_stereo_planner.h"
#include "gl_block_recorder.h"
#include "gl_eye_targets.h"
#include "gl_frame_reader.h"
#include "gl_resolution_scaler.h"
#include "gl_texture_twin_table.h"
#include "invisible_window_manager.h"
#include "types.h"
#include <array>
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes the second eye of an OpenGL application live:repeats each draw and clear for the right eye, applying the eye matrix
         * (see RecipeStereoPlanner::make_eye_shift_matrix()) to affected draws of both eyes. Render-to-texture copies go to twin textures.
         * glBegin..glEnd blocks are compiled into an owned display list called per eye at glEnd. Own calls: see OwnCallScope.
         * Not thread-safe: the converter holds its lock around every call. */
        class GLEyeRenderer
        {
        public:
            /* Finds the data an argument refers to (draw indices, texture names) in the chunks the call analyzer took. */
            typedef std::function<bool(const uint32_t&, const void**, uint32_t*)> GetChunkFunc;

            /* Finds the model-view and projection matrices and the matrix mode that the analysis tracked for the application's context; returns false if it has none. */
            typedef std::function<bool(std::array<float, 16>*, std::array<float, 16>*, uint32_t*)> GetMatricesFunc;

            /* Counters for the log. */
            struct Statistics
            {
                uint64_t n_array_draws       = 0; /* glDrawArrays/glDrawElements, both eyes */
                uint64_t n_camera_attached_draws = 0; /* affected draws left unmoved: attached to the camera */
                uint64_t n_block_draws       = 0; /* blocks from glBegin to glEnd */
                uint64_t n_clears            = 0;
                uint64_t n_fallback_blocks   = 0; /* recorded blocks that a call which cannot be recorded handed to a display list */
                uint64_t n_list_draws        = 0; /* glCallList calls */
                uint64_t n_recorded_blocks   = 0; /* blocks made again from the calls that were recorded */
                uint64_t n_skipped_draws     = 0;
                uint64_t n_texture_copies    = 0; /* framebuffer-to-texture copies, both eyes */
                uint64_t n_twin_textures     = 0;
                uint64_t n_unmirrored_calls  = 0; /* calls changing right-eye pixels or twin textures, made for the left only */
            };

            /* The core must outlive the renderer. in_is_shown_elsewhere makes the application's windows invisible. The application renders
             * as large as fits the target eye size (see GLResolutionScaler); 0 keeps its size. */
            GLEyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr,
                          const bool&                        in_is_shown_elsewhere,
                          const uint32_t&                    in_target_width,
                          const uint32_t&                    in_target_height);

            /* Frames the depth from take_frame_depth() lags: 1 for asynchronous reads, 0 for synchronous ones. */
            uint32_t get_depth_lag() const;

            const Statistics& get_statistics() const;

            bool is_ready() const;

            /* A call that sets a color, a normal or a texture coordinate is about to be made. Inside a block that is being recorded it is recorded and not made; returns false then.
             * Any other call that the recording cannot hold hands the block to a display list instead (see fall_back_to_list()), and the call is made. */
            bool on_attribute(const APIInterceptor::APIFunction&         in_function,
                              const uint32_t&                            in_n_arguments,
                              const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Takes the depth of a frame (pass depths, x scale of the last projection); the frame lags by get_depth_lag().
             * Returns false if the recipe does not measure depth (see Comfort::measures_depth) or the frame has none. Given once per frame. */
            bool take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                  float*                    out_projection_scale_ptr);

            /* The frame is about to be presented: reads both eyes and returns the previous frame's images. Returns false if there
             * are none (first frame, size change). The images stay valid while their token is held. */
            bool on_frame_ending(GLFrameImages* out_images_ptr);

            /* A call of the application has been made. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments_out,
                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr);

            /* A call is about to be made. Returns false if the converter made it for both eyes or it is to be dropped. A draw gives its class;
             * draws inside an application display list are not draws here (the list runs when called). Pushed draws move their viewport
             * origin per eye (see RecipeStereoPlanner::plan_push_shift()). */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        const bool&                                in_has_draws,
                        const Analysis::Recipe::DrawClass&         in_draw_class,
                        const bool&                                in_is_pushed);

            void set_get_chunk_func   (const GetChunkFunc&    in_get_chunk_func);
            void set_get_matrices_func(const GetMatricesFunc& in_get_matrices_func);

            /* Whether the blocks between glBegin and glEnd are recorded and made again for each eye, or compiled into a display list that the driver calls. */
            void set_records_blocks(const bool& in_records_blocks);

            /* Log notes: what was not made for the right eye or could not be made. Each note is given once. */
            std::vector<std::string> take_notes();

        private:
            static constexpr size_t N_TEXTURE_UNITS = 32;

            /* Copies the framebuffer into the bound texture with glCopyTexSubImage2D (in_is_sub) or glCopyTexImage2D; in_value_array holds the call's arguments after the target. */
            void copy_framebuffer(const bool&                   in_is_sub,
                                  const uint32_t&               in_target,
                                  const std::array<int32_t, 8>& in_value_array) const;

            /* The block that is being recorded cannot be held by the recording (a call that it does not record came in): the calls so far are given to a display list, which the
             * rest of the block is compiled into, and the block ends as one that was compiled from its glBegin. */
            void fall_back_to_list(const APIInterceptor::APIFunction& in_function);

            /* The recorded block ends: it is made for both eyes. */
            void finish_recorded_block(const bool&                       in_has_draws,
                                       const Analysis::Recipe::DrawClass& in_draw_class,
                                       const bool&                       in_is_pushed);

            /* Looks up the twins of the textures that are bound: once for a draw, which binds them for each eye and again for the application. */
            TextureSelection find_texture_selection() const;

            /* Calls the function per eye, right first, with that eye's target bound (and its matrix applied for an affected draw). */
            void play_eyes(const std::function<void()>&      in_draw_func,
                           const Analysis::Recipe::DrawClass& in_draw_class,
                           const bool&                        in_is_pushed);

            /* Makes the twin of the bound texture with the same filter and wrap parameters; in_has_level_size also allocates in_level
             * at the original's size and format. Returns false if the twin cannot be made. */
            bool make_twin(const uint32_t& in_texture,
                           const bool&     in_has_level_size,
                           const int32_t&  in_level);

            /* Mirrors a framebuffer-to-texture copy into the texture's twin for the right eye. When rendering larger, also makes the
             * scaled left-eye copy and returns true: the application's own call is not to be made. */
            bool mirror_copy(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Applies a call that sets a parameter of the bound texture to the twin of that texture as well. */
            void mirror_texture_parameter(const APIInterceptor::APIFunction&         in_function,
                                          const uint32_t&                            in_n_arguments,
                                          const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            void note_once(const std::string& in_key,
                           const std::string& in_note);

            /* Takes pending depth read results (see start_depth_read()), oldest first, adding each to its frame.
             * in_only_older_frames: only reads started before the current frame; in_only_the_oldest: only one read. */
            void collect_depth_reads(const bool& in_only_older_frames,
                                     const bool& in_only_the_oldest);

            /* Forgets pending depth reads if the targets were recreated (their buffers are gone). */
            void forget_lost_depth_reads();

            /* Measures how deep the drawn surfaces are in a depth read (see FastDepthRangeMeasurer), using the z scale and z shift of the projection. */
            Stereo::Core::DepthRange measure_depths(const float&  in_z_scale,
                                                    const float&  in_z_shift,
                                                    const float*  in_depth_ptr,
                                                    const size_t& in_n_depths);

            /* Reads the left eye's depth and records how deep the ending pass was; the result goes to the frame when the read completes. */
            void measure_pass_depth();

            /* Reads the left eye's depth and waits (no GPU buffer to read into); the pass is added to the current frame. */
            void read_depth_at_once();

            /* Starts an asynchronous depth read (see GLEyeTargets::begin_depth_read()); taken at the end of the next frame.
             * If all buffers are in use, waits for the oldest read. */
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
            void select_textures(const TextureSelection& in_selection,
                                 const bool&             in_are_twins);

            /* Takes the matrices of the draw from the analysis instead of asking the driver. Every so often it also asks the driver and compares; the first
             * difference makes it give up tracking for good (noted in the log). Returns false if the matrices must be read from the driver. */
            bool read_tracked_matrices(std::array<float, 16>* out_modelview_f32mat_ptr,
                                       std::array<float, 16>* out_projection_f32mat_ptr,
                                       int32_t*               out_matrix_mode_ptr);

            /* Records a call of the block. Returns false if the call is not one that is recorded. */
            bool record_call(const APIInterceptor::APIFunction&         in_function,
                             const uint32_t&                            in_n_arguments,
                             const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Serves a one-pixel glReadPixels that is part of a run of such reads from the left eye's picture (see PictureReadbackCache),
             * writing to the application's memory. Returns true if the call is not to be made. */
            bool serve_pixel_read(const uint32_t&                            in_n_arguments,
                                  const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Makes the framebuffers if they do not exist and the application has a context and a window. */
            void try_to_prepare();

            std::array<uint32_t, N_TEXTURE_UNITS> m_bound_texture_array;
            std::vector<float>                    m_depth_f32_vec;
            uint32_t                              m_depth_generation;       /* targets generation when the pending depth reads began (see GLEyeTargets::get_generation()) */
            uint32_t                              m_depth_lag;              /* see get_depth_lag() */
            Stereo::Core::FrameDepthLedger        m_depth_ledger;
            Stereo::Core::FastDepthRangeMeasurer  m_fast_depth_range_measurer;
            uint32_t                              m_active_unit;
            uint32_t                              m_applied_height; /* framebuffer size the viewport and scissor were last set for */
            uint32_t                              m_applied_width;
            Analysis::Recipe::DrawClass           m_block_class;
            bool                                  m_block_has_draws;
            bool                                  m_block_is_pushed;
            uint32_t                              m_block_list;
            GLBlockRecorder                       m_block_recorder;
            bool                                  m_caches_pixel_reads; /* the recipe serves runs of one-pixel reads from one picture */
            Stereo::Core::RecipeStereoPlanner*    m_core_ptr;
            GLFrameReader                         m_frame_reader;
            GetChunkFunc                          m_get_chunk_func;
            GetMatricesFunc                       m_get_matrices_func;
            bool                                  m_has_failed;
            InvisibleWindowManager                m_invisible_window_manager;
            bool                                  m_is_in_app_list;
            bool                                  m_is_in_block;
            bool                                  m_is_recording;      /* a block is being recorded */
            bool                                  m_is_shown_elsewhere;
            bool                                  m_measures_depth;
            uint32_t                              m_n_failed_attempts;
            uint64_t                              m_n_tracked_matrix_reads;
            std::vector<std::string>              m_note_vec;
            std::vector<std::string>              m_noted_key_vec;
            std::array<float, 16>                 m_pass_projection_f32mat; /* projection of the first affected draw of the pass */
            bool                                  m_pass_has_affected_draws;
            Stereo::Core::PictureReadbackCache    m_readback_cache;
            bool                                  m_records_blocks;    /* see set_records_blocks() */
            GLResolutionScaler                    m_scaler;
            Statistics                            m_statistics;
            GLEyeTargets                          m_targets;
            bool                                  m_trusts_tracked_matrices; /* false once the tracked matrices differed from the driver's */
            GLTextureTwinTable                    m_twin_table;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_EYE_RENDERER_H */
