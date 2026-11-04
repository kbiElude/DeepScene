/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_STEREO_REPLAY_ENGINE_H
#define STEREO_REPLAY_ENGINEDLL_STEREO_REPLAY_ENGINE_H

#include "replay_engine.h"
#include "Stereo/Replay/EngineDLL/i_replay_engine.h"
#include "paced_frame_executor.h"
#include "stereo_frame_executor.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* The replay engine of the interceptor, with the frames played for the eyes of a recipe, at the pace of the dumped process, and
             * the Direct3D 7 calls executed for two eyes.
             */
            class StereoReplayEngine : public IReplayEngine,
                                       public IPlaybackController
            {
            public:
                StereoReplayEngine();
                ~StereoReplayEngine();

                void                 add_call_interceptor       (ICallInterceptor*                     in_interceptor_ptr) override;
                void                 add_replay_observer        (IReplayObserver*                      in_observer_ptr)    override;
                float                get_eye_separation         ()                                                         const    override;
                IPlaybackController* get_playback_controller_ptr()                                                         override;
                PlaybackStatus       get_status                 ()                                                         const    override;
                bool                 load                       (const char*                           in_filename_ptr,
                                                                 std::string*                          out_error_ptr) override;
                void                 pause                      ()                                                         override;
                void                 play                       ()                                                         override;
                void                 remove_call_interceptor    (ICallInterceptor*                     in_interceptor_ptr) override;
                void                 remove_replay_observer     (IReplayObserver*                      in_observer_ptr)    override;
                void                 reset                      ()                                                         override;
                void                 set_eye_separation         (const float&                          in_eye_separation)  override;
                void                 set_frame_execution        (const FrameExecutionOptions&          in_options)         override;
                void                 set_frame_image_sink       (IFrameImageSink*                      in_sink_ptr)        override;
                void                 set_recipe                 (const Analysis::RecipeLoader::Recipe& in_recipe)          override;
                void                 set_speed                  (const double&                         in_speed)           override;
                void                 set_windows_visible        (const bool&                           in_are_visible)     override;

            private:
                /* Declared in the order the engine needs them: the engine, whose thread uses the others, is the first to go. */
                StereoFrameExecutor                  m_stereo_executor;
                PacedFrameExecutor                   m_paced_executor;
                ::Replay::ReplayEngineDLL::ReplayEngine m_engine;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_STEREO_REPLAY_ENGINE_H */
