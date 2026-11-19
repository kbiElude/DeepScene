/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_LIVE_STEREO_CONVERTER_H
#define STEREO_CONVERTERDLL_LIVE_STEREO_CONVERTER_H

#include "analysis/recipe/types.h"
#include "common/callbacks.h"
#include "stereo/session/types.h"
#include "stereo/core/recipe_stereo_planner.h"
#include "stereo/transport/frame_channel.h"
#include "utils/parallel/srw_mutex.h"
#include "live_call_analyzer.h"
#include "common/api_function_argument.h"
#include "D3D7/d3d7_eye_renderer.h"
#include "D3D7/d3d7_host_memory_exporter.h"
#include "D3D7/d3d7_windowed_mode.h"
#include "D3D8/d3d8_eye_renderer.h"
#include "D3D8/d3d8_host_memory_exporter.h"
#include "D3D8/d3d8_resolution_scaler.h"
#include "D3D8/d3d8_windowed_mode.h"
#include "paced_frame_publisher.h"
#include "OpenGL/gl_eye_renderer.h"
#include "OpenGL/gl_host_memory_exporter.h"
#include "session_log.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The stereo converter of one application. It follows the calls of the application to the device the way the analysis of a dump does
         * (see LiveCallAnalyzer) and uses the baked recipe to decide what each draw is (see RecipeStereoPlanner). It passes the frames that the application
         * is about to present, unchanged, to the viewer, the program that shows the images. The session log (see SessionLog) records what the
         * converter did.
         */
        class LiveStereoConverter
        {
        public:
            /* Returns nullptr if the launcher did not start the process, or if the recipe or the frame channel cannot be opened. */
            static std::unique_ptr<LiveStereoConverter> create_ptr();

            ~LiveStereoConverter();

        private:
            LiveStereoConverter(const Stereo::Session::SessionInfo&              in_session_info,
                                const Analysis::Recipe::Recipe&                  in_recipe,
                                std::unique_ptr<Stereo::Transport::FrameChannel> in_channel_ptr);

            /* Counts a draw under the class that the recipe assigned to it. */
            void count_draw(const Analysis::Recipe::DrawClass& in_draw_class);

            /* Called when a frame of the application ends. Counts the frame, adds the statistics to the log from time to time and reads the settings from the viewer. */
            void count_frame(std::vector<std::string>* out_log_line_vec_ptr);

            /* Returns the statistics line that is written to the log every few frames. */
            std::string make_statistics_line() const;

            /* Called when a frame of a Direct3D 7 application ends. Passes the frame to the viewer (a flip, or a blit to the primary surface), counts it
             * and adds the statistics to the log from time to time.
             */
            void on_d3d7_frame_ending(const APIInterceptor::APIFunction&         in_api_func,
                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                      std::vector<std::string>*                  out_log_line_vec_ptr);

            /* Handles the OpenGL draws of the call that is being analysed: none, one, or all draws of a display list. Each draw is classified by the
             * recipe and counted. Returns true if the call has draws. Sets the class of the call: AFFECTED if any draw is AFFECTED, otherwise the
             * class of its draws. Sets the call to be pushed toward the viewer only if all its draws are pushed.
             */
            bool on_gl_draws(Analysis::Recipe::DrawClass* out_draw_class_ptr,
                             bool*                        out_is_pushed_ptr,
                             std::vector<std::string>*    out_log_line_vec_ptr);

            /* Called before a call is made. Does the work of the pre callback for a call that the application made. */
            void on_pre(const APIInterceptor::APIFunction&         in_api_func,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        bool*                                      out_should_pass_through_ptr);

            /* Called before a frame is presented. Reads the settings from the viewer and adds log lines for the settings that changed. */
            void on_present_starting(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                     void**                                     out_right_surface_ptr_ptr,
                                     std::vector<std::string>*                  out_log_line_vec_ptr);

            /* Returns true if the converter keeps the matrix of the call. The call itself is then not made. */
            bool on_set_transform(const uint32_t&                            in_n_arguments,
                                  const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Passes the frame in the back buffer to the viewer. The back buffer is either the one of the window or the one in the chain of the surface that the flip is called for. */
            void pass_d3d7_frame(void* in_primary_surface_ptr,
                                 void* in_back_buffer_ptr);

            /* Passes the images of both eyes of an OpenGL frame to the viewer. */
            void pass_gl_frame(const GLFrameImages& in_images);

            /* Reads the settings that the viewer sends: the eye separation, the strength of the 3D and the distance where the eyes converge. The viewer
             * can send them again while the application runs.
             */
            void read_channel_settings(std::vector<std::string>* out_log_line_vec_ptr);


            /* A call that sets a color, a normal or a texture coordinate, or gives a vertex that the recorder holds: a block that is being recorded keeps it. No other part of the converter looks at these calls. */
            static void on_attribute_callback(APIInterceptor::APIFunction                in_api_func,
                                              uint32_t                                   in_n_arguments,
                                              const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                              void*                                      in_user_arg_ptr,
                                              bool*                                      out_should_pass_through_ptr);
            static void on_post_callback(APIInterceptor::APIFunction                in_api_func,
                                         void*                                      in_user_arg_ptr,
                                         uint32_t                                   in_n_arguments_out,
                                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);
            static void on_pre_callback (APIInterceptor::APIFunction                in_api_func,
                                         uint32_t                                   in_n_arguments,
                                         const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                         void*                                      in_user_arg_ptr,
                                         bool*                                      out_should_pass_through_ptr);


            /* The state of the converter while the application runs. Lock order: no other lock is taken while its mutex is held. */
            struct LiveState
            {
                std::unique_ptr<Stereo::Core::RecipeStereoPlanner> core_ptr;
                std::unique_ptr<GLEyeRenderer>                     gl_renderer_ptr;
                std::unique_ptr<D3D7EyeRenderer>                   d3d7_renderer_ptr;
                std::atomic<int64_t>                               n_callback_ticks = {0}; /* time spent in the callbacks, in performance-counter ticks. Only one call in N_CALLS_PER_TIMING is timed, and its time is counted that many times
 */
                std::atomic<int64_t>                      n_draw_ticks     = {0}; /* the part of n_callback_ticks spent in draws (Direct3D 7, Direct3D 8 and OpenGL)
 */
                D3D7WindowedMode                          d3d7_windowed_mode;
                uint64_t                                  dump_every         = 0; /* a frame is written to a file every dump_every frames; 0: never
 */
                uint64_t                                  dump_first_frame   = 0; /* frames before this one are not written (see dump_every)
 */
                uint64_t                                  dump_frame         = 0; /* number of the one frame to write to a file; 0: none
 */
                bool                                      is_scene_dump_enabled = false; /* writes up to 60 frames of each scene (frame shape) to files: the first when the scene has settled, the others half a second apart
 */
                uint64_t                                  previous_shape_key = 0;   /* shape key of the frame completed before the last one; the measured depth may belong to that frame
 */
                int64_t                                   scene_run_begin_qpc = 0;  /* when the current run of frames of the scene scene_run_key began, in performance-counter ticks
 */
                uint64_t                                  scene_run_key      = 0;
                int64_t                                   scene_run_last_sample_qpc = 0; /* when the last frame of this run was written, in performance-counter ticks
 */
                uint32_t                                  scene_run_n_samples = 0;  /* number of frames of this run that were written (see is_scene_dump_enabled)
 */
                bool                                      is_overlay_push_forced = false; /* set from the environment, not by the viewer
 */
                LiveCallAnalyzer                          call_analyzer;
                Utils::Parallel::SrwMutex                 mutex;
                Stereo::Core::DepthRange                  last_depth_range;              /* the depth range measured for the last frame
 */
                float                                     last_projection_scale = 0.0f;
                GLFrameImages                             last_gl_images;                 /* images of the last OpenGL frame that was read; valid until the next one is read
 */
                bool                                      has_last_gl_images    = false;
                int64_t                                   last_frame_qpc     = 0;      /* when the previous frame was completed, in performance-counter ticks; 0: no frame yet
 */
                uint64_t                                  n_affected_draws   = 0;
                uint64_t                                  n_frames           = 0;
                uint64_t                                  n_kept_matrices    = 0;
                uint64_t                                  n_mixed_class_calls = 0; /* calls of display lists whose draws have different classes
 */
                uint64_t                                  n_skipped_draws    = 0;
                uint64_t                                  n_unaffected_draws = 0;
                std::set<uint64_t>                        scene_seen_set;                 /* shape keys of the scenes that were written (see is_scene_dump_enabled)
 */
                std::unique_ptr<D3D8EyeRenderer>          d3d8_renderer_ptr;
                std::unique_ptr<D3D8ResolutionScaler>     scaler_ptr;
            };

            std::unique_ptr<Stereo::Transport::FrameChannel> m_channel_ptr;
            std::unique_ptr<PacedFramePublisher>             m_d3d7_paced_frame_publisher_ptr;
            std::unique_ptr<PacedFramePublisher>             m_gl_paced_frame_publisher_ptr;
            std::unique_ptr<PacedFramePublisher>             m_paced_frame_publisher_ptr;
            LiveState                                        m_live_state;
            SessionLog                                       m_log;
            D3D8WindowedMode                                 m_windowed_mode;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_LIVE_STEREO_CONVERTER_H */
