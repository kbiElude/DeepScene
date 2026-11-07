/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_CONTROL_WINDOW_H
#define STEREO_REPLAY_PLAYER_CONTROL_WINDOW_H

#include "tools/stereo_replay/engine_dll/i_playback_controller.h"
#include "tools/stereo_replay/engine_dll/i_replay_observer.h"
#include "utils/ui_host/i_imgui_window_host.h"
#include <mutex>
#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* The window with the playback controls: play / pause, reset, and what has been replayed so far. It lives apart from the
             * windows of the replay, so that what the replay does to its API state is not touched.
             */
            class ControlWindow : public Utils::UIHost::IImGuiWindowHost,
                                  public Stereo::Replay::EngineDLL::IReplayObserver
            {
            public:
                ControlWindow(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                              const std::string&                              in_title);

                void on_error          (const std::string&                              in_message) override;
                void on_frame_presented(const uint32_t&                                 in_n_frame) override;
                void on_state_changed  (const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                        const Stereo::Replay::EngineDLL::PlaybackState& in_new_state) override;

                /* The replay's window is closed by the user: the quit handler closes this one. */
                void on_window_closed() override;

                /* The window which the replay puts in place decides where this one goes: below it, in the middle. Any thread. */
                void on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement) override;

                /* The number of frames of the workload, for the slider that seeks; none (0, the default) leaves the slider out. Before the window runs. */
                void set_n_frames(const uint64_t& in_n_frames);

                /* Adds the slider of the eye separation; for a replay which has a recipe. */
                void show_eye_separation();

            protected:
                void draw_contents        (Utils::UIHost::ContentExtent* inout_extent_ptr) override;
                bool handle_message       (const HWND&                   in_window,
                                           const UINT&                   in_message,
                                           const WPARAM&                 in_wparam,
                                           const LPARAM&                 in_lparam,
                                           LRESULT*                      out_result_ptr)   override;
                void on_resized_to_content()                                               override;

            private:
                /* The placement of the window of the replay. Lock order: no other lock is taken while it is held. */
                struct ReplayWindow
                {
                    bool                                       is_known = false;
                    std::mutex                                 mutex;
                    Stereo::Replay::EngineDLL::WindowPlacement placement;
                };


                void place_below_replay_window();

                /* Seeks to the frame, or asks first if that is back in time (see m_has_pending_seek). */
                void request_seek(const uint32_t& in_n_frame,
                                  const uint32_t& in_current_frame);

                static Utils::UIHost::WindowOptions get_window_options(const std::string& in_title);


                Stereo::Replay::EngineDLL::IPlaybackController* m_controller_ptr;
                int                                             m_go_to_frame;
                bool                                            m_has_eye_separation;
                bool                                            m_has_pending_seek;
                bool                                            m_is_back_warning_off;
                bool                                            m_is_placed;
                bool                                            m_is_redo_on_release;
                bool                                            m_is_slider_active;
                Stereo::Replay::EngineDLL::WindowPlacement      m_last_placement;
                uint64_t                                        m_n_frames;
                uint32_t                                        m_pending_seek_frame;
                ReplayWindow                                    m_replay_window;
                int                                             m_slider_frame;
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_CONTROL_WINDOW_H */
