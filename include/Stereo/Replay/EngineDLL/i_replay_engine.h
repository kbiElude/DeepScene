/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H
#define STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H

#include "Analysis/RecipeLoader/types.h"
#include "Replay/ReplayEngineDLL/i_replay_engine.h"
#include "Stereo/Replay/EngineDLL/i_call_interceptor.h"
#include "Stereo/Replay/EngineDLL/i_frame_image_sink.h"
#include "Stereo/Replay/EngineDLL/i_playback_controller.h"
#include "Stereo/Replay/EngineDLL/i_replay_observer.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays a workload file on its own thread, for the eyes of a recipe (see the replay engine of the interceptor for loading and
             * for the listeners). Create one with create_replay_engine().
             */
            class IReplayEngine : public ::Replay::ReplayEngineDLL::IReplayEngine
            {
            public:
                IPlaybackController* get_playback_controller_ptr() override = 0;

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
