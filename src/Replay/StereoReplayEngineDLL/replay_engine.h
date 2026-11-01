/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_REPLAY_ENGINE_H
#define STEREOREPLAYENGINEDLL_REPLAY_ENGINE_H

#include "Capture/APIDumpLoader/api_info.h"
#include "Replay/StereoReplayEngineDLL/i_replay_engine.h"
#include "frame_pacer.h"
#include "i_call_executor.h"
#include "object_remapper.h"
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Walks the workload on a thread of its own, and hands each call to the executor of its API. Pauses at frame
         * boundaries only, so that a paused replay is always between two frames.
         */
        class ReplayEngine : public IReplayEngine,
                             public IPlaybackController
        {
        public:
            ReplayEngine();
            ~ReplayEngine();

            void                 add_call_interceptor       (ICallInterceptor* in_interceptor_ptr) override;
            void                 add_replay_observer        (IReplayObserver*  in_observer_ptr)    override;
            IPlaybackController* get_playback_controller_ptr()                                     override;
            PlaybackStatus       get_status                 ()                                     const                               override;
            bool                 load                       (const char*       in_filename_ptr,
                                                             std::string*      out_error_ptr) override;
            void                 pause                      ()                                     override;
            void                 play                       ()                                     override;
            void                 remove_call_interceptor    (ICallInterceptor* in_interceptor_ptr) override;
            void                 remove_replay_observer     (IReplayObserver*  in_observer_ptr)    override;
            void                 reset                      ()                                     override;
            void                 set_speed                  (const double&     in_speed)           override;

        private:
            /* State shared with the replay thread. */
            struct Control
            {
                std::condition_variable cv;
                std::string             error_message;
                bool                    is_reset_requested = false;
                bool                    is_stop_requested  = false;
                std::mutex              mutex;
                uint64_t                n_calls_executed   = 0;
                uint32_t                n_frames_presented = 0;
                bool                    should_play        = false;
                PlaybackState           state              = PlaybackState::IDLE;
            };

            /* Lock order: Control::mutex is never held while a listener is called. */
            struct Listeners
            {
                std::vector<ICallInterceptor*> interceptor_ptr_vec;
                std::mutex                     mutex;
                std::vector<IReplayObserver*>  observer_ptr_vec;
            };

            bool                          execute_call             (const Capture::APIDumpLoader::DumpedAPICall& in_call);
            void                          fail                     (const std::string&                  in_message);
            std::vector<ICallExecutor*>   get_executor_ptr_vec     () const;
            std::vector<IReplayObserver*> get_observer_ptr_vec     () const;
            void                          notify_error             (const std::string&                  in_message);
            void                          notify_frame_presented   (const uint32_t&                     in_n_frame);
            void                          notify_idle              ();
            void                          notify_window_placed     (const WindowPlacement&              in_placement);
            void                          play_until_frame_boundary();
            void                          reset_executors          (const bool&                         in_is_final);
            bool                          rewind                   ();
            void                          set_state                (const PlaybackState&                in_state);
            void                          thread_main              ();

            mutable Control                                                    m_control;
            std::map<Capture::APIDumpLoader::APIGroup, std::shared_ptr<ICallExecutor> > m_executor_ptr_map;
            bool                                                             m_is_loaded;
            mutable Listeners                                                        m_listeners;
            FramePacer                                                         m_pacer;
            ObjectRemapper                                                     m_remapper;
            std::thread                                                      m_thread;
            CallWalker                                                       m_walker;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_REPLAY_ENGINE_H */
