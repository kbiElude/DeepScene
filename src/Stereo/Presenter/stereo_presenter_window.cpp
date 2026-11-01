/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Presenter/stereo_presenter_window.h"
#include "imgui.h"
#include <algorithm>
#include <d3d11.h>

Stereo::Presenter::StereoPresenterWindow::StereoPresenterWindow(std::unique_ptr<Stereo::Transport::IFrameImporter> in_importer_ptr,
                                                                const std::string&                                 in_title)
    :ImGuiWindowHost(get_window_options(in_title) ),
     m_importer_ptr (std::move(in_importer_ptr) ),
     m_is_styled    (false)
{
    /* Stub */
}

void Stereo::Presenter::StereoPresenterWindow::draw_contents(VR::UIHost::ContentExtent* inout_extent_ptr)
{
    const ImVec2                     available = ImGui::GetContentRegionAvail();
    Stereo::Transport::ImportedFrame frame;

    (void) inout_extent_ptr;

    if (m_is_styled == false)
    {
        /* The image goes to the edge of the window, on black. */
        ImGui::GetStyle().WindowPadding              = ImVec2(0.0f,
                                                              0.0f);
        ImGui::GetStyle().Colors[ImGuiCol_WindowBg]  = ImVec4(0.0f,
                                                              0.0f,
                                                              0.0f,
                                                              1.0f);

        m_is_styled = true;
    }

    if (m_sink_ptr == nullptr)
    {
        m_sink_ptr.reset(new PreviewSink(get_device_ptr(),
                                         get_device_context_ptr(),
                                         [this](const uint32_t& in_width,
                                                const uint32_t& in_height)
                                         {
                                             fit_window_to_image(in_width,
                                                                 in_height);
                                         }) );
    }

    /* The headset reads the frames, if there is one: the channel has room for one reader. */
    if (m_headset_sink_ptr == nullptr &&
        m_importer_ptr->acquire(&frame) )
    {
        m_sink_ptr->on_frame(frame);

        m_importer_ptr->release();
    }

    if (m_sink_ptr->get_view_ptr() == nullptr)
    {
        char         headset_text_i8vec128[128];
        uint64_t     shown_frame_index = 0;
        double       shown_seconds     = 0.0;
        const char*  text              = "Waiting for the application...";

        if (m_headset_sink_ptr != nullptr)
        {
            /* No preview while the headset shows the frames, so the frame and time are given here. */
            text = "Showing in the headset (no preview while it does)";

            if (m_headset_sink_ptr->get_shown_frame(&shown_frame_index,
                                                    &shown_seconds) )
            {
                ::snprintf(headset_text_i8vec128,
                           sizeof(headset_text_i8vec128),
                           "Showing in the headset (no preview while it does)\n\nFrame %llu  t = %.3f s",
                           static_cast<unsigned long long>(shown_frame_index),
                           shown_seconds);

                text = headset_text_i8vec128;
            }
        }

        const ImVec2 text_size = ImGui::CalcTextSize(text);

        ImGui::SetCursorPos(ImVec2((available.x - text_size.x) / 2.0f,
                                   (available.y - text_size.y) / 2.0f) );
        ImGui::TextUnformatted(text);
    }
    else
    {
        const float  scale = std::min<float>(available.x / static_cast<float>(m_sink_ptr->get_width() ),
                                             available.y / static_cast<float>(m_sink_ptr->get_height() ) );
        const ImVec2 size  = ImVec2(static_cast<float>(m_sink_ptr->get_width() )  * scale,
                                    static_cast<float>(m_sink_ptr->get_height() ) * scale);

        ImGui::SetCursorPos(ImVec2((available.x - size.x) / 2.0f,
                                   (available.y - size.y) / 2.0f) );
        ImGui::Image(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(m_sink_ptr->get_view_ptr() ) )),
                     size);
    }
}

Stereo::Transport::IFrameImporter* Stereo::Presenter::StereoPresenterWindow::get_importer_ptr() const
{
    return m_importer_ptr.get();
}

void Stereo::Presenter::StereoPresenterWindow::fit_window_to_image(const uint32_t& in_width,
                                                                   const uint32_t& in_height)
{
    RECT        frame   = {0, 0, static_cast<LONG>(in_width), static_cast<LONG>(in_height)};
    MONITORINFO monitor = {};
    const HWND  window  = get_window();

    monitor.cbSize = sizeof(monitor);

    ::AdjustWindowRectEx(&frame,
                         static_cast<DWORD>(::GetWindowLongW(window,
                                                             GWL_STYLE) ),
                         FALSE, /* bMenu */
                         static_cast<DWORD>(::GetWindowLongW(window,
                                                             GWL_EXSTYLE) ));

    const BOOL has_monitor_info = ::GetMonitorInfoW(::MonitorFromWindow(window,
                                                                        MONITOR_DEFAULTTONEAREST),
                                                   &monitor);

    if (has_monitor_info != FALSE)
    {
        if (frame.right  - frame.left <= monitor.rcWork.right  - monitor.rcWork.left &&
            frame.bottom - frame.top  <= monitor.rcWork.bottom - monitor.rcWork.top)
        {
            /* Asynchronously: this is called while the window draws, and a new size makes it make its swap chain again. */
            ::SetWindowPos(window,
                           nullptr,
                           0, /* X */
                           0, /* Y */
                           frame.right  - frame.left,
                           frame.bottom - frame.top,
                           SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
        }
    }
}

void Stereo::Presenter::StereoPresenterWindow::set_headset_sink(std::unique_ptr<HeadsetSink> in_sink_ptr)
{
    m_headset_sink_ptr = std::move(in_sink_ptr);
}

VR::UIHost::WindowOptions Stereo::Presenter::StereoPresenterWindow::get_window_options(const std::string& in_title)
{
    constexpr int CLIENT_HEIGHT = 600;
    constexpr int CLIENT_WIDTH  = 800;

    VR::UIHost::WindowOptions result;

    result.class_name    = L"StereoPresenterWindow";
    result.client_height = CLIENT_HEIGHT;
    result.client_width  = CLIENT_WIDTH;
    result.is_resizable  = true;
    result.title         = (in_title.empty() ) ? "VR Player" : in_title;

    return result;
}
