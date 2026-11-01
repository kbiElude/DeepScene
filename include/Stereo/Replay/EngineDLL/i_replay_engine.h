/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H
#define STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H

#include "Stereo/Replay/EngineDLL/i_call_interceptor.h"
#include "Stereo/Replay/EngineDLL/i_frame_image_sink.h"
#include "Stereo/Replay/EngineDLL/i_playback_controller.h"
#include "Stereo/Replay/EngineDLL/i_replay_observer.h"
#include "Analysis/RecipeLoader/types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays a workload file on its own thread. Create one with create_replay_engine(). */
            class IReplayEngine
            {
            public:
                virtual ~IReplayEngine() = default;

                /* The listeners must outlive the engine, or be removed first. */
                virtual void add_call_interceptor(ICallInterceptor* in_interceptor_ptr) = 0;
                virtual void add_replay_observer (IReplayObserver*  in_observer_ptr)    = 0;

                virtual IPlaybackController* get_playback_controller_ptr() = 0;

                /* Opens the workload; the replay waits for play(). Returns false (and says why) if the file cannot be read. */
                virtual bool load(const char*  in_filename_ptr,
                                  std::string* out_error_ptr) = 0;

                virtual void remove_call_interceptor(ICallInterceptor* in_interceptor_ptr) = 0;
                virtual void remove_replay_observer (IReplayObserver*  in_observer_ptr)    = 0;

                /* Both must be set before the replay is played. */
                virtual void set_frame_execution (const FrameExecutionOptions& in_options)  = 0;
                virtual void set_frame_image_sink(IFrameImageSink*             in_sink_ptr) = 0;

                /* Changes the matrices of the calls of the buffered frames (see FrameExecutionOptions) as the recipe says. Frames are told
                 * apart as they are played, so every call is analyzed, and playing is slower. Must be set before the replay is played.
                 */
                virtual void set_recipe(const Analysis::RecipeLoader::Recipe& in_recipe) = 0;

                /* Hides the windows of the replayed application (it still renders to them), for when the images are shown some other way.
                 * Must be set before the replay is played.
                 */
                virtual void set_windows_visible(const bool& in_are_visible) = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H */
