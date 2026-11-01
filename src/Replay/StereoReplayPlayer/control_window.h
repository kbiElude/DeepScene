/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYPLAYER_CONTROL_WINDOW_H
#define STEREOREPLAYPLAYER_CONTROL_WINDOW_H

#include "Replay/StereoReplayEngineDLL/i_playback_controller.h"
#include "Replay/StereoReplayEngineDLL/i_replay_observer.h"
#include "VR/UIHost/imgui_window_host.h"
#include <mutex>
#include <string>
#include <Windows.h>

namespace Replay
{
    namespace StereoReplayPlayer
    {
        /* The window with the playback controls: play / pause, reset, and what has been replayed so far. It lives apart from the
         * windows of the replay, so that what the replay does to its API state is not touched.
         */
        class ControlWindow : public VR::UIHost::ImGuiWindowHost,
                              public Replay::StereoReplayEngineDLL::IReplayObserver
        {
        public:
            ControlWindow(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                                const std::string&                            in_title);

            void on_error          (const std::string&                            in_message) override;
            void on_frame_presented(const uint32_t&                               in_n_frame) override;
            void on_state_changed  (const Replay::StereoReplayEngineDLL::PlaybackState& in_old_state,
                                          const Replay::StereoReplayEngineDLL::PlaybackState& in_new_state) override;

            /* The replay's window is closed by the user: the quit handler closes this one. */
            void on_window_closed() override;

            /* The window which the replay puts in place decides where this one goes: below it, in the middle. Any thread. */
            void on_window_placed(const Replay::StereoReplayEngineDLL::WindowPlacement& in_placement) override;

            /* Adds the slider of the eye separation; for a replay which has a recipe. */
            void show_eye_separation();

        protected:
            void draw_contents        (VR::UIHost::ContentExtent* inout_extent_ptr) override;
            bool handle_message       (const HWND&                in_window,
                                       const UINT&                in_message,
                                       const WPARAM&              in_wparam,
                                       const LPARAM&              in_lparam,
                                       LRESULT*                   out_result_ptr) override;
            void on_resized_to_content()                                          override;

        private:
            /* The placement of the window of the replay. Lock order: no other lock is taken while it is held. */
            struct ReplayWindow
            {
                bool                                     is_known = false;
                std::mutex                               mutex;
                Replay::StereoReplayEngineDLL::WindowPlacement placement;
            };


            void place_below_replay_window();

            static VR::UIHost::WindowOptions get_window_options(const std::string& in_title);


            Replay::StereoReplayEngineDLL::IPlaybackController* m_controller_ptr;
            bool                                          m_has_eye_separation;
            bool                                          m_is_placed;
            Replay::StereoReplayEngineDLL::WindowPlacement      m_last_placement;
            ReplayWindow                                  m_replay_window;
        };
    }
}

#endif /* STEREOREPLAYPLAYER_CONTROL_WINDOW_H */
