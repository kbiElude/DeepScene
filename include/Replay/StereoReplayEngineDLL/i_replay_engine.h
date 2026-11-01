/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_I_REPLAY_ENGINE_H
#define STEREOREPLAYENGINEDLL_I_REPLAY_ENGINE_H

#include "Replay/StereoReplayEngineDLL/i_call_interceptor.h"
#include "Replay/StereoReplayEngineDLL/i_playback_controller.h"
#include "Replay/StereoReplayEngineDLL/i_replay_observer.h"

namespace Replay
{
    namespace StereoReplayEngineDLL
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
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_I_REPLAY_ENGINE_H */
