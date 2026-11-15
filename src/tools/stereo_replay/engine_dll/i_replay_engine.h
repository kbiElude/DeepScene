/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H
#define STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H

#include "analysis/recipe/types.h"
#include "replay/replay_engine_dll/i_replay_engine.h"
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
            /* Replays a workload file on its own thread and renders the eyes that a recipe defines. Loading a workload and adding
             * listeners are described in ::Replay::ReplayEngineDLL::IReplayEngine. Create an engine with create_replay_engine().
             */
            class IReplayEngine : public ::Replay::ReplayEngineDLL::IReplayEngine
            {
            public:
                IPlaybackController* get_playback_controller_ptr() override = 0;

                /* Loads the dependencies between the frames of the workload from its .deps file (see APIDumpDependencyExtractor). A seek
                 * uses them to leave out frames whose content nothing after the target reads (see IPlaybackController::play_to_frame).
                 * Call it after the workload is loaded. Returns false, and sets the reason in out_error_ptr, if the file cannot be read
                 * or belongs to another workload.
                 */
                virtual bool set_dependencies_file(const char*  in_filename_ptr,
                                                   std::string* out_error_ptr) = 0;

                /* Both must be set before the replay is played. */
                virtual void set_frame_execution (const FrameExecutionOptions& in_options)  = 0;
                virtual void set_frame_image_sink(IFrameImageSink*             in_sink_ptr) = 0;

                /* Rewrites the matrices of the calls of the buffered frames (see FrameExecutionOptions) as the recipe says. Every call is
                 * analyzed as it is played, so playing is slower. Must be set before the replay is played.
                 */
                virtual void set_recipe(const Analysis::Recipe::Recipe& in_recipe) = 0;

                /* Shows or hides the windows of the replayed application. Hide them when the images are shown some other way; the
                 * application still renders to the hidden windows. Must be set before the replay is played.
                 */
                virtual void set_windows_visible(const bool& in_are_visible) = 0;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_REPLAY_ENGINE_H */
