/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_REPLAY_ENGINE_H
#define STEREO_REPLAY_ENGINEDLL_REPLAY_ENGINE_H

#include "Capture/APIDumpLoader/api_info.h"
#include "Stereo/Replay/EngineDLL/i_replay_engine.h"
#include "call_walker.h"
#include "executor_registry.h"
#include "eye_matrix_binder.h"
#include "frame_buffer.h"
#include "frame_rewriter.h"
#include "listener_list.h"
#include "frame_pacer.h"
#include "i_call_executor.h"
#include "object_remapper.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        struct CompletedFrame;

        namespace Frontends
        {
            class AnalysisSession;
        }
    }
}

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
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
                struct EyeSnapshot
                {
                    bool                 has_pixels = false;
                    std::vector<uint8_t> pixel_u8_vec;
                };

                /* State shared with the replay thread. */
                struct Control
                {
                    std::condition_variable cv;
                    std::string             error_message;
                    bool                    is_reset_requested = false;
                    bool                    is_stop_requested  = false;
                    std::mutex              mutex;
                    std::atomic<uint64_t>   n_calls_executed{0};   /* written by the replay thread, read by any */
                    std::atomic<uint32_t>   n_frames_presented{0};
                    bool                    should_play        = false;
                    PlaybackState           state              = PlaybackState::IDLE;
                };

                /* The back buffer left by the last execution of each eye, for frames that carry it over: an execution starts from the
                 * one of its own eye. Replay thread only.
                 */
                struct EyeHistory
                {
                    std::vector<uint8_t>       base_u8_vec; /* before the first execution of the frame, for an eye which has no history yet */
                    std::array<EyeSnapshot, 2> snapshot_array; /* left, right */
                };

                /* Lock order: Control::mutex is never held while a listener is called. */
                struct Listeners
                {
                    std::vector<ICallInterceptor*> interceptor_ptr_vec;
                    std::mutex                     mutex;
                    std::atomic<size_t>            n_interceptors{0}; /* the number above, to look at without the lock */
                };


                bool begin_frame_execution    (const bool&                                      in_uses_history,
                                               const uint32_t&                                  in_n_execution);
                bool capture_execution        (const uint32_t&                                  in_n_frame,
                                               const uint32_t&                                  in_n_execution,
                                               const bool&                                      in_uses_history);
                void complete_frame           (bool*                                            out_is_done_ptr);
                bool execute_buffered_frame   (const uint32_t&                                  in_n_frame,
                                               bool*                                            out_has_workload_ended_ptr);
                bool execute_call             (const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                               const IChunkSource&                              in_chunk_source);
                bool execute_rewritten_call   (const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                               const IChunkSource&                              in_chunk_source,
                                               const size_t&                                    in_n_call,
                                               const uint32_t&                                  in_n_execution);
                void fail                     (const std::string&                               in_message);
                bool get_eye_history_need     (const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                               bool*                                            out_needs_history_ptr,
                                               std::string*                                     out_error_ptr) const;
                /* Whether the texture that the application has at the address (in the API) is one that each eye has a copy of. */
                bool is_per_eye_texture       (const Capture::APIDumpLoader::APIGroup&          in_group,
                                               const uint64_t&                                  in_address) const;
                void notify_error             (const std::string&                               in_message);
                void notify_frame_presented   (const uint32_t&                                  in_n_frame);
                void notify_idle              ();
                void notify_window_closed     ();
                void notify_window_placed     (const WindowPlacement&                           in_placement);
                void play_until_frame_boundary();
                bool read_back_buffer         (std::vector<uint8_t>*                            out_pixel_u8_vec_ptr,
                                               FrameImage*                                      out_image_ptr);
                bool read_next_call           (const Capture::APIDumpLoader::DumpedAPICall**    out_call_ptr_ptr);
                void reset_executors          (const bool&                                      in_is_final);
                bool rewind                   ();
                void set_state                (const PlaybackState&                             in_state);
                void thread_main              ();
                bool write_back_buffer        (const std::vector<uint8_t>&                      in_pixel_u8_vec);

                mutable Control                                                        m_control;
                EyeHistory                                                             m_eye_history;
                ExecutorRegistry                                                       m_executors;
                std::unique_ptr<EyeMatrixBinder>                                       m_matrix_binder_ptr;
                ListenerList<IReplayObserver>                                          m_observers;
                FrameBuffer                                                            m_frame_buffer; /* the frame being played, in the replay thread */
                std::atomic<float>                                                     m_eye_separation; /* set by any thread; read once for each frame */
                FrameExecutionOptions                                                  m_frame_execution;
                float                                                                  m_frame_eye_separation; /* the one the frame being played is made with */
                IFrameImageSink*                                                       m_image_sink_ptr;
                bool                                                                   m_is_loaded;
                mutable Listeners                                                      m_listeners;
                FramePacer                                                             m_pacer;
                std::unique_ptr<Analysis::RecipeApplier::RecipeApplier>                m_recipe_applier_ptr;
                ObjectRemapper                                                         m_remapper;
                std::unique_ptr<FrameRewriter>                                         m_rewriter_ptr;
                std::unique_ptr<Analysis::APIDumpAnalysis::Frontends::AnalysisSession> m_session_ptr;
                std::thread                                                            m_thread;
                CallWalker                                                             m_walker;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_REPLAY_ENGINE_H */
