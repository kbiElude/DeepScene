/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_CONTROL_WINDOW_H
#define STEREO_REPLAY_PLAYER_CONTROL_WINDOW_H

#include "tools/stereo_replay/engine_dll/i_playback_controller.h"
#include "tools/stereo_replay/engine_dll/i_replay_observer.h"
#include "utils/ui/imgui_player_window_base.h"
#include <mutex>
#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* The window with the playback controls: play and pause, reset, and the progress of the replay. It is a separate window
             * from the replay windows, so it does not touch the API state of the replay.
             */
            class ControlWindow : public Utils::UI::ImGuiPlayerWindowBase,
                                  public Stereo::Replay::EngineDLL::IReplayObserver
            {
            public:
                ControlWindow(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                              const std::string&                              in_title);

                void on_error          (const std::string&                              in_message) override;
                void on_frame_presented(const uint32_t&                                 in_n_frame) override;
                void on_state_changed  (const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                        const Stereo::Replay::EngineDLL::PlaybackState& in_new_state) override;

                /* The user closed the replay window. The quit handler then closes this window. */
                void on_window_closed() override;

                /* Called when the replay places its window. The control window is then placed below it, centered. Any thread. */
                void on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement) override;

                /* Sets the number of frames of the workload, which the seek slider needs. 0 (the default) leaves the slider out. Call
                 * it before the window runs.
                 */
                void set_n_frames(const uint64_t& in_n_frames);

                /* Adds the eye separation slider. Use it for a replay that has a recipe. */
                void show_eye_separation();

            protected:
                void draw_contents        (Utils::UI::ContentExtent* inout_extent_ptr) override;
                bool handle_message       (const HWND&               in_window,
                                           const UINT&               in_message,
                                           const WPARAM&             in_wparam,
                                           const LPARAM&             in_lparam,
                                           LRESULT*                  out_result_ptr) override;
                void on_resized_to_content()                                           override;

            private:
                /* The placement of the replay window. Lock order: no other lock is taken while the mutex is held. */
                struct ReplayWindow
                {
                    bool                                       is_known = false;
                    std::mutex                                 mutex;
                    Stereo::Replay::EngineDLL::WindowPlacement placement;
                };


                void place_below_replay_window();

                /* Seeks to the frame. If that is back in time, asks for confirmation first (see m_has_pending_seek). */
                void request_seek(const uint32_t& in_n_frame,
                                  const uint32_t& in_current_frame);

                static Utils::UI::WindowOptions get_window_options(const std::string& in_title);


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
