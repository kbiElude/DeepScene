/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_CONVERTER_H
#define STEREO_CONVERTERDLL_CONVERTER_H

#include "Analysis/RecipeLoader/types.h"
#include "Common/callbacks.h"
#include "Stereo/Session/types.h"
#include "Stereo/Core/stereo_core.h"
#include "Stereo/Transport/frame_channel.h"
#include "call_feeder.h"
#include "d3d7_eye_renderer.h"
#include "d3d8_eye_renderer.h"
#include "d3d8_host_memory_exporter.h"
#include "d3d8_resolution_scaler.h"
#include "d3d8_windowed_mode.h"
#include "frame_passer.h"
#include "session_log.h"
#include <memory>
#include <mutex>
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
                      const Analysis::RecipeLoader::Recipe&            in_recipe,
                      std::unique_ptr<Stereo::Transport::FrameChannel> in_channel_ptr);

            /* Counts the draw by what the recipe made of it. */
            void count_draw(const Analysis::RecipeLoader::DrawClass& in_draw_class);

            /* What the log gets every so many frames. */
            std::string make_statistics_line() const;

            /* A frame of a Direct3D 7 application is ending: it is counted, and the log gets the statistics now and then. Nothing is passed
             * on yet; the draws are only followed.
             */
            void on_d3d7_frame_ending(std::vector<std::string>* out_log_line_vec_ptr);

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
                std::unique_ptr<D3D7EyeRenderer>          d3d7_renderer_ptr;
                CallFeeder                                feeder;
                std::mutex                                mutex;
                uint64_t                                  n_affected_draws   = 0;
                uint64_t                                  n_frames           = 0;
                uint64_t                                  n_kept_matrices    = 0;
                uint64_t                                  n_skipped_draws    = 0;
                uint64_t                                  n_unaffected_draws = 0;
                std::unique_ptr<D3D8EyeRenderer>          renderer_ptr;
                std::unique_ptr<D3D8ResolutionScaler>     scaler_ptr;
            };

            std::unique_ptr<Stereo::Transport::FrameChannel> m_channel_ptr;
            std::unique_ptr<FramePasser>                     m_frame_passer_ptr;
            LiveState                                        m_live_state;
            SessionLog                                       m_log;
            D3D8WindowedMode                                 m_windowed_mode;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_CONVERTER_H */
