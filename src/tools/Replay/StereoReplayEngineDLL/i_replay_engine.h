/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H
#define STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H

#include "Analysis/RecipeLoader/types.h"
#include "Replay/ReplayEngineDLL/i_replay_engine.h"
#include "i_call_interceptor.h"
#include "i_frame_image_sink.h"
#include "i_playback_controller.h"
#include "i_replay_observer.h"

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

                /* The dependencies between the frames of the workload (a .deps file of it; see APIDumpDependencyExtractor), which a seek uses to leave out the
                 * frames whose content nothing after the target reads (see IPlaybackController::play_to_frame). Must be set after the workload is loaded. False,
                 * and why, if the file cannot be read or is of another workload.
                 */
                virtual bool set_dependencies_file(const char*  in_filename_ptr,
                                                   std::string* out_error_ptr) = 0;

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
