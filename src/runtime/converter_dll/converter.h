/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_CONVERTER_H
#define STEREO_CONVERTERDLL_CONVERTER_H

#include "analysis/recipe/types.h"
#include "common/callbacks.h"
#include "stereo/session/types.h"
#include "stereo/core/stereo_core.h"
#include "stereo/transport/frame_channel.h"
#include "call_feeder.h"
#include "common/api_function_argument.h"
#include "D3D7/d3d7_eye_renderer.h"
#include "D3D7/d3d7_host_memory_exporter.h"
#include "D3D7/d3d7_windowed_mode.h"
#include "D3D8/d3d8_eye_renderer.h"
#include "D3D8/d3d8_host_memory_exporter.h"
#include "D3D8/d3d8_resolution_scaler.h"
#include "D3D8/d3d8_windowed_mode.h"
#include "frame_passer.h"
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
        /* The stereo converter of one application. It follows the calls of the application to the device the way the analysis of a dump
         * does (see CallFeeder), tells from the baked recipe what each draw is (see StereoCore), and passes on the frames which the
         * application is about to present as they are, to the program which shows them. The log of the session (see SessionLog)
         * tells what it did.
         */
        class Converter
        {
        public:
            /* Returns nullptr if the process was not started by the launcher, or the recipe or the channel cannot be opened. */
            static std::unique_ptr<Converter> create_ptr();

            ~Converter();

        private:
            Converter(const Stereo::Session::SessionInfo&              in_session_info,
                      const Analysis::Recipe::Recipe&                  in_recipe,
                      std::unique_ptr<Stereo::Transport::FrameChannel> in_channel_ptr);

            /* Counts the draw by what the recipe made of it. */
            void count_draw(const Analysis::Recipe::DrawClass& in_draw_class);

            /* A frame of the application ends: it is counted, the log gets the statistics now and then, and what the program which shows the images says is taken. */
            void count_frame(std::vector<std::string>* out_log_line_vec_ptr);

            /* What the log gets every so many frames. */
            std::string make_statistics_line() const;

            /* A frame of a Direct3D 7 application is ending: it is counted, and the log gets the statistics now and then. Nothing is passed
             * on yet; the draws are only followed.
             */
            void on_d3d7_frame_ending(const APIInterceptor::APIFunction&         in_api_func,
                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                      std::vector<std::string>*                  out_log_line_vec_ptr);

            /* The call that is analysed has made draws of OpenGL (none, one, or those of a display list): each is told what the recipe makes of it, and counted. Returns true
             * if there are any, and says the class that the call is made as: affected if any draw is, the one of the draws otherwise. The call is pushed toward the viewer if
             * all of its draws are.
             */
            bool on_gl_draws(Analysis::Recipe::DrawClass* out_draw_class_ptr,
                             bool*                        out_is_pushed_ptr,
                             std::vector<std::string>*    out_log_line_vec_ptr);

            /* A call is about to be made: the work of the callback, for a call that is the application's. */
            void on_pre(const APIInterceptor::APIFunction&         in_api_func,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                        bool*                                      out_should_pass_through_ptr);

            /* A frame is about to be presented: what the program which shows the images says is taken, and the log lines for what changed
             * are added.
             */
            void on_present_starting(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                     void**                                     out_right_surface_ptr_ptr,
                                     std::vector<std::string>*                  out_log_line_vec_ptr);

            /* Returns true if the converter keeps the matrix of the call, which is then not to be made. */
            bool on_set_transform(const uint32_t&                            in_n_arguments,
                                  const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* The frame in the back buffer (of the window, or of the chain of the surface which a flip is called for) is passed on to the program which shows the images. */
            void pass_d3d7_frame(void* in_primary_surface_ptr,
                                 void* in_back_buffer_ptr);

            /* The images of the eyes of a frame of an OpenGL application are passed on to the program which shows the images. */
            void pass_gl_frame(const GlFrameImages& in_images);

            /* The program which shows the images says how far apart the eyes are, how strong the 3D is and where the eyes converge, and can
             * say it again while the application plays.
             */
            void read_channel_settings(std::vector<std::string>* out_log_line_vec_ptr);


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


            /* What the converter knows of the application as it runs. Lock order: no other lock is taken while it is held. */
            struct LiveState
            {
                std::unique_ptr<Stereo::Core::StereoCore> core_ptr;
                std::unique_ptr<GlEyeRenderer>            gl_renderer_ptr;
                std::unique_ptr<D3D7EyeRenderer>          d3d7_renderer_ptr;
                std::atomic<int64_t>                      n_callback_ticks = {0}; /* spent in the callbacks, in the ticks of the performance counter */
                std::atomic<int64_t>                      n_draw_ticks     = {0}; /* of those, in the draws of Direct3D 7                              */
                D3D7WindowedMode                          d3d7_windowed_mode;
                uint64_t                                  dump_every         = 0; /* a frame is written to a file every so many frames; 0: none */
                uint64_t                                  dump_first_frame   = 0; /* the frames before it are not written (see dump_every)     */
                uint64_t                                  dump_frame         = 0; /* of the frame to write to a file; 0: none */
                bool                                      is_scene_dump_enabled = false; /* the first frame of each kind of frame (scene) is written to a file */
                uint64_t                                  scene_run_length   = 0;   /* how many frames in a row were of the scene of scene_run_key */
                uint64_t                                  scene_run_key      = 0;
                bool                                      is_overlay_push_forced = false; /* from the environment, not the program which shows the images */
                CallFeeder                                feeder;
                std::mutex                                mutex;
                Stereo::Core::DepthRange                  last_depth_range;              /* what was last measured of how deep a frame was */
                float                                     last_projection_scale = 0.0f;
                GlFrameImages                             last_gl_images;                 /* of the last frame of OpenGL that was read; valid until the next one */
                bool                                      has_last_gl_images    = false;
                uint64_t                                  n_affected_draws   = 0;
                uint64_t                                  n_frames           = 0;
                uint64_t                                  n_kept_matrices    = 0;
                uint64_t                                  n_mixed_class_calls = 0; /* calls of display lists that made draws of different classes */
                uint64_t                                  n_skipped_draws    = 0;
                uint64_t                                  n_unaffected_draws = 0;
                std::set<uint64_t>                        scene_seen_set;                 /* the shape keys of the scenes that were written, see is_scene_dump_enabled */
                std::unique_ptr<D3D8EyeRenderer>          renderer_ptr;
                std::unique_ptr<D3D8ResolutionScaler>     scaler_ptr;
            };

            std::unique_ptr<Stereo::Transport::FrameChannel> m_channel_ptr;
            std::unique_ptr<FramePasser>                     m_d3d7_frame_passer_ptr;
            std::unique_ptr<FramePasser>                     m_gl_frame_passer_ptr;
            std::unique_ptr<FramePasser>                     m_frame_passer_ptr;
            LiveState                                        m_live_state;
            SessionLog                                       m_log;
            D3D8WindowedMode                                 m_windowed_mode;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_CONVERTER_H */
