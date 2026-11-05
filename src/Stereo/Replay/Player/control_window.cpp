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


Stereo::Replay::Player::ControlWindow::ControlWindow(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                                                     const std::string&                              in_title)
    :ImGuiWindowHost       (get_window_options(in_title) ),
     m_controller_ptr      (in_controller_ptr),
     m_go_to_frame         (0),
     m_has_eye_separation  (false),
     m_has_pending_seek    (false),
     m_is_back_warning_off (false),
     m_is_placed           (false),
     m_is_redo_on_release  (true),
     m_is_slider_active    (false),
     m_last_placement      (),
     m_n_frames            (0),
     m_pending_seek_frame  (0),
     m_slider_frame        (0)
{
    /* Stub */
}

void Stereo::Replay::Player::ControlWindow::draw_contents(VR::UIHost::ContentExtent* inout_extent_ptr)
{
    constexpr float BUTTON_WIDTH_N_EMS = 6.0f;

    const float       button_width = BUTTON_WIDTH_N_EMS * ImGui::GetFontSize();
    const auto        status       = m_controller_ptr->get_status();
    const ImGuiStyle& style        = ImGui::GetStyle             ();
    const bool        is_ended     = (status.state == Stereo::Replay::EngineDLL::PlaybackState::FAILED ||
                                      status.state == Stereo::Replay::EngineDLL::PlaybackState::FINISHED);
    const bool        is_playing   = (status.state == Stereo::Replay::EngineDLL::PlaybackState::PLAYING);

    ImGui::Text("State: %s",
                ::Replay::ReplayPlayerSupport::StateNames::get_state_name_ptr(status.state) );

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

    /* The slider, the step and the box seek: forward is hurried through, back in time starts over, which is asked about first. */
    if (m_n_frames > 1)
    {
        const uint32_t current_frame = (status.n_frames_presented > 0) ? status.n_frames_presented - 1
                                                                       : 0;
        const int      last_frame    = static_cast<int>(m_n_frames - 1);

        ImGui::Separator();

        /* The slider is where the replay is, until it is taken hold of. */
        if (m_is_slider_active == false)
        {
            m_slider_frame = static_cast<int>(current_frame);
        }

        ImGui::SetNextItemWidth(2.0f * button_width + style.ItemSpacing.x);

        ImGui::SliderInt("Frame",
                         &m_slider_frame,
                         0,
                         last_frame);

        m_is_slider_active = ImGui::IsItemActive();

        if (ImGui::IsItemDeactivatedAfterEdit() )
        {
            request_seek(static_cast<uint32_t>(m_slider_frame),
                         current_frame);
        }

        extend_content(inout_extent_ptr);

        ImGui::BeginDisabled(static_cast<int>(current_frame) >= last_frame);

        if (ImGui::Button("Step",
                          ImVec2(button_width,
                                 0.0f) ))
        {
            m_controller_ptr->play_to_frame(current_frame + 1);
        }

        ImGui::EndDisabled();

        ImGui::SameLine();

        ImGui::SetNextItemWidth(button_width);

        if (ImGui::InputInt("Go to",
                            &m_go_to_frame,
                            0,
                            0,
                            ImGuiInputTextFlags_EnterReturnsTrue) )
        {
            request_seek(static_cast<uint32_t>(std::min<int>(std::max<int>(m_go_to_frame,
                                                                           0),
                                                             last_frame) ),
                         current_frame);
        }

        extend_content(inout_extent_ptr);

        if (m_has_pending_seek)
        {
            ImGui::PushTextWrapPos(inout_extent_ptr->right);
            {
                ImGui::TextColored(ImVec4(1.0f,
                                          0.8f,
                                          0.3f,
                                          1.0f),
                                   "Going back to frame %u starts the replay over from the first frame and plays on to it, unpaced. That takes a while.",
                                   m_pending_seek_frame);
            }
            ImGui::PopTextWrapPos();

            extend_content(inout_extent_ptr);

            if (ImGui::Button("Go back",
                              ImVec2(button_width,
                                     0.0f) ))
            {
                m_controller_ptr->play_to_frame(m_pending_seek_frame);

                m_has_pending_seek = false;
            }

            ImGui::SameLine();

            if (ImGui::Button("Stay",
                              ImVec2(button_width,
                                     0.0f) ))
            {
                m_has_pending_seek = false;
            }

            ImGui::Checkbox("Do not ask again",
                            &m_is_back_warning_off);

            extend_content(inout_extent_ptr);
        }
    }

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

        /* A frame which was played is not played again by itself: the new distance is in the next one, unless the frame is played again, which is starting
         * over (quick if the dependencies are known) and playing on to it.
         */
        const bool is_released = ImGui::IsItemDeactivatedAfterEdit();

        if (is_released                           &&
            is_playing                == false    &&
            m_is_redo_on_release                  &&
            status.n_frames_presented >  0)
        {
            m_controller_ptr->reset();
            m_controller_ptr->play_to_frame(status.n_frames_presented - 1);
        }

        extend_content(inout_extent_ptr);

        if (is_playing == false)
        {
            ImGui::Checkbox("Show the frame again on release",
                            &m_is_redo_on_release);

            if (m_is_redo_on_release == false)
            {
                ImGui::TextDisabled("Applies from the next frame (Step or Play).");
            }

            extend_content(inout_extent_ptr);
        }
    }

    /* The message does not make the window wider than the buttons do. */
    if (status.state == Stereo::Replay::EngineDLL::PlaybackState::FAILED)
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

VR::UIHost::WindowOptions Stereo::Replay::Player::ControlWindow::get_window_options(const std::string& in_title)
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

bool Stereo::Replay::Player::ControlWindow::handle_message(const HWND&   in_window,
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

void Stereo::Replay::Player::ControlWindow::on_error(const std::string& in_message)
{
    (void) in_message;
}

void Stereo::Replay::Player::ControlWindow::on_frame_presented(const uint32_t& in_n_frame)
{
    (void) in_n_frame;
}

void Stereo::Replay::Player::ControlWindow::on_resized_to_content()
{
    /* The window is wider or narrower now: it is centered under the replay window again. */
    m_is_placed = false;

    place_below_replay_window();
}

void Stereo::Replay::Player::ControlWindow::on_state_changed(const Stereo::Replay::EngineDLL::PlaybackState& in_old_state,
                                                             const Stereo::Replay::EngineDLL::PlaybackState& in_new_state)
{
    (void) in_old_state;
    (void) in_new_state;
}

void Stereo::Replay::Player::ControlWindow::on_window_closed()
{
    /* The user has closed the window of the replay: the tool ends with it, and main() sees to that. */
}

void Stereo::Replay::Player::ControlWindow::on_window_placed(const Stereo::Replay::EngineDLL::WindowPlacement& in_placement)
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

void Stereo::Replay::Player::ControlWindow::place_below_replay_window()
{
    constexpr int GAP_N_PIXELS      = 8;
    constexpr int N_PARTS_TO_CENTER = 2;

    RECT                                       control_rect = {};
    bool                                       is_known     = false;
    MONITORINFO                                monitor_info = {};
    Stereo::Replay::EngineDLL::WindowPlacement placement;
    RECT                                       replay_rect  = {};
    int                                        x            = 0;
    int                                        y            = 0;

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

void Stereo::Replay::Player::ControlWindow::request_seek(const uint32_t& in_n_frame,
                                                         const uint32_t& in_current_frame)
{
    if (in_n_frame != in_current_frame)
    {
        if (in_n_frame > in_current_frame ||
            m_is_back_warning_off)
        {
            m_controller_ptr->play_to_frame(in_n_frame);

            m_has_pending_seek = false;
        }
        else
        {
            m_has_pending_seek   = true;
            m_pending_seek_frame = in_n_frame;
        }
    }
}

void Stereo::Replay::Player::ControlWindow::set_n_frames(const uint64_t& in_n_frames)
{
    m_n_frames = in_n_frames;
}

void Stereo::Replay::Player::ControlWindow::show_eye_separation()
{
    m_has_eye_separation = true;
}
