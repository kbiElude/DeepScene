/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "control_window.h"
#include "state_names.h"
#include "imgui.h"
#include <algorithm>

/* Posted by the replay thread: the window of the replay has been put in place. */
static constexpr UINT WM_PLACE_BELOW_REPLAY_WINDOW = WM_APP + 1;

/* The top of the slider: the eyes of a person are 0.065 m apart, scenes are rarely in metres. */
static constexpr float EYE_SEPARATION_MAX = 2.0f;


Replay::StereoReplayPlayer::ControlWindow::ControlWindow(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                                                               const std::string&                            in_title)
    :ImGuiWindowHost     (get_window_options(in_title) ),
     m_controller_ptr    (in_controller_ptr),
     m_has_eye_separation(false),
     m_is_placed         (false),
     m_last_placement    ()
{
    /* Stub */
}

void Replay::StereoReplayPlayer::ControlWindow::draw_contents(VR::UIHost::ContentExtent* inout_extent_ptr)
{
    constexpr float BUTTON_WIDTH_N_EMS = 6.0f;

    const float       button_width = BUTTON_WIDTH_N_EMS * ImGui::GetFontSize();
    const auto        status       = m_controller_ptr->get_status();
    const ImGuiStyle& style        = ImGui::GetStyle             ();
    const bool        is_ended     = (status.state == Replay::StereoReplayEngineDLL::PlaybackState::FAILED   ||
                                            status.state == Replay::StereoReplayEngineDLL::PlaybackState::FINISHED);
    const bool        is_playing   = (status.state == Replay::StereoReplayEngineDLL::PlaybackState::PLAYING);

    ImGui::Text("State: %s",
                StateNames::get_state_name_ptr(status.state) );

    extend_content(inout_extent_ptr);

    /* The number of the frame which was presented last; right after a reset, it is the first one: frame 0. */
    ImGui::Text("Frame: %u",
                (status.n_frames_presented > 0) ? status.n_frames_presented - 1
                                                : 0);

    extend_content(inout_extent_ptr);

    ImGui::Text("Calls: %llu",
                static_cast<unsigned long long>(status.n_calls_executed) );

    extend_content(inout_extent_ptr);

    ImGui::Separator();

    /* After the end, only a reset starts the replay again. */
    ImGui::BeginDisabled(is_ended);

    if (ImGui::Button((is_playing) ? "Pause"
                                   : "Play",
                      ImVec2(button_width,
                             0.0f) ))
    {
        if (is_playing)
        {
            m_controller_ptr->pause();
        }
        else
        {
            m_controller_ptr->play();
        }
    }

    ImGui::EndDisabled();

    extend_content(inout_extent_ptr);

    ImGui::SameLine();

    if (ImGui::Button("Reset",
                      ImVec2(button_width,
                             0.0f) ))
    {
        m_controller_ptr->reset();
    }

    extend_content(inout_extent_ptr);

    if (m_has_eye_separation)
    {
        float separation = m_controller_ptr->get_eye_separation();

        ImGui::Separator();

        ImGui::SetNextItemWidth(2.0f * button_width + style.ItemSpacing.x);

        if (ImGui::SliderFloat("Eye separation",
                               &separation,
                               0.0f,
                               EYE_SEPARATION_MAX,
                               "%.3f") )
        {
            m_controller_ptr->set_eye_separation(separation);
        }

        extend_content(inout_extent_ptr);
    }

    /* The message does not make the window wider than the buttons do. */
    if (status.state == Replay::StereoReplayEngineDLL::PlaybackState::FAILED)
    {
        ImGui::PushTextWrapPos(inout_extent_ptr->right);
        {
            ImGui::TextColored(ImVec4(1.0f,
                                      0.4f,
                                      0.4f,
                                      1.0f),
                               "%s",
                               status.error_message.c_str() );
        }
        ImGui::PopTextWrapPos();

        extend_content(inout_extent_ptr);
    }
}

VR::UIHost::WindowOptions Replay::StereoReplayPlayer::ControlWindow::get_window_options(const std::string& in_title)
{
    constexpr int CLIENT_HEIGHT = 150;
    constexpr int CLIENT_WIDTH  = 420;

    VR::UIHost::WindowOptions result;

    result.class_name    = L"StereoReplayPlayerControlWindow";
    result.client_height = CLIENT_HEIGHT;
    result.client_width  = CLIENT_WIDTH;
    result.fits_content  = true;
    result.is_topmost    = true; /* the windows of the replay are made by the replayed app, and can cover this one */
    result.title         = in_title;

    return result;
}

bool Replay::StereoReplayPlayer::ControlWindow::handle_message(const HWND&   in_window,
                                                               const UINT&   in_message,
                                                               const WPARAM& in_wparam,
                                                               const LPARAM& in_lparam,
                                                               LRESULT*      out_result_ptr)
{
    bool result = false;

    (void) in_window;
    (void) in_wparam;
    (void) in_lparam;

    if (in_message == WM_PLACE_BELOW_REPLAY_WINDOW)
    {
        place_below_replay_window();

        *out_result_ptr = 0;
        result          = true;
    }

    return result;
}

void Replay::StereoReplayPlayer::ControlWindow::on_error(const std::string& in_message)
{
    (void) in_message;
}

void Replay::StereoReplayPlayer::ControlWindow::on_frame_presented(const uint32_t& in_n_frame)
{
    (void) in_n_frame;
}

void Replay::StereoReplayPlayer::ControlWindow::on_resized_to_content()
{
    /* The window is wider or narrower now: it is centered under the replay window again. */
    m_is_placed = false;

    place_below_replay_window();
}

void Replay::StereoReplayPlayer::ControlWindow::on_state_changed(const Replay::StereoReplayEngineDLL::PlaybackState& in_old_state,
                                                                       const Replay::StereoReplayEngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;
    (void) in_new_state;
}

void Replay::StereoReplayPlayer::ControlWindow::on_window_closed()
{
    /* The user has closed the window of the replay: the tool ends with it, and main() sees to that. */
}

void Replay::StereoReplayPlayer::ControlWindow::on_window_placed(const Replay::StereoReplayEngineDLL::WindowPlacement& in_placement)
{
    {
        std::lock_guard<std::mutex> lock(m_replay_window.mutex);

        m_replay_window.is_known  = true;
        m_replay_window.placement = in_placement;
    }

    /* The window is moved by the thread which owns it. */
    ::PostMessageW(get_window(),
                   WM_PLACE_BELOW_REPLAY_WINDOW,
                   0,
                   0);
}

void Replay::StereoReplayPlayer::ControlWindow::place_below_replay_window()
{
    constexpr int GAP_N_PIXELS      = 8;
    constexpr int N_PARTS_TO_CENTER = 2;

    RECT                                     control_rect = {};
    bool                                     is_known     = false;
    MONITORINFO                              monitor_info = {};
    Replay::StereoReplayEngineDLL::WindowPlacement placement;
    RECT                                     replay_rect  = {};
    int                                      x            = 0;
    int                                      y            = 0;

    {
        std::lock_guard<std::mutex> lock(m_replay_window.mutex);

        is_known  = m_replay_window.is_known;
        placement = m_replay_window.placement;
    }

    /* A window of the replay which comes back in another place (it was made anew) takes the controls with it. */
    if (is_known                                     &&
        (m_is_placed      == false                   ||
         placement.height != m_last_placement.height ||
         placement.width  != m_last_placement.width  ||
         placement.x      != m_last_placement.x      ||
         placement.y      != m_last_placement.y) )
    {
        replay_rect.bottom = placement.y + placement.height;
        replay_rect.left   = placement.x;
        replay_rect.right  = placement.x + placement.width;
        replay_rect.top    = placement.y;

        monitor_info.cbSize = sizeof(monitor_info);

        ::GetWindowRect(get_window(),
                       &control_rect);
        ::GetMonitorInfoW(::MonitorFromRect(&replay_rect,
                                            MONITOR_DEFAULTTONEAREST),
                         &monitor_info);

        /* Under the window of the replay, in the middle; if that leaves the screen, it is moved back into it. */
        x = placement.x + (placement.width - (control_rect.right - control_rect.left) ) / N_PARTS_TO_CENTER;
        y = placement.y + placement.height + GAP_N_PIXELS;

        x = std::max<int>(monitor_info.rcWork.left,
                          std::min<int>(x,
                                        monitor_info.rcWork.right - (control_rect.right - control_rect.left) ));
        y = std::max<int>(monitor_info.rcWork.top,
                          std::min<int>(y,
                                        monitor_info.rcWork.bottom - (control_rect.bottom - control_rect.top) ));

        ::SetWindowPos(get_window(),
                       HWND_TOPMOST,
                       x,
                       y,
                       0,
                       0,
                       SWP_NOACTIVATE | SWP_NOSIZE);

        m_is_placed      = true;
        m_last_placement = placement;
    }
}

void Replay::StereoReplayPlayer::ControlWindow::show_eye_separation()
{
    m_has_eye_separation = true;
}
