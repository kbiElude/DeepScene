/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "vr_player_window.h"
#include "stereo/core/resolution_policy.h"
#include "utils/files/file_system.h"
#include "imgui.h"
#include <algorithm>
#include <float.h>
#include <stdio.h>

/* The look of the window: one accent color on dark panels. */
static const ImVec4 ACCENT_COLOR        (0.33f, 0.42f, 0.95f, 1.00f);
static const ImVec4 ACCENT_ACTIVE_COLOR (0.27f, 0.35f, 0.85f, 1.00f);
static const ImVec4 ACCENT_HOVERED_COLOR(0.42f, 0.50f, 1.00f, 1.00f);
static const ImVec4 BORDER_COLOR        (0.20f, 0.22f, 0.28f, 1.00f);
static const ImVec4 DIM_TEXT_COLOR      (0.58f, 0.61f, 0.70f, 1.00f);
static const ImVec4 ERROR_TEXT_COLOR    (1.00f, 0.45f, 0.45f, 1.00f);
static const ImVec4 FRAME_COLOR         (0.15f, 0.165f, 0.20f, 1.00f);
static const ImVec4 FRAME_HOVERED_COLOR (0.19f, 0.21f, 0.26f, 1.00f);
static const ImVec4 PANEL_COLOR         (0.105f, 0.115f, 0.145f, 1.00f);
static const ImVec4 SELECTED_COLOR      (0.20f, 0.25f, 0.50f, 0.55f);
static const ImVec4 SELECTED_HOVERED_COLOR(0.22f, 0.28f, 0.55f, 0.45f);
static const ImVec4 TEXT_COLOR          (0.92f, 0.93f, 0.96f, 1.00f);
static const ImVec4 WINDOW_COLOR        (0.07f, 0.075f, 0.095f, 1.00f);

/* Sizes at 100% scale; the scale of the monitor is applied. */
static constexpr float ACCENT_LINE_HEIGHT           = 2.0f;
static constexpr float CARD_ACCENT_BAR_WIDTH        = 3.0f;
static constexpr int   DEFAULT_DEPTH_INDEX          = 1;

/* The depth that can be asked for: less than the converter works out for each scene, as much, and a little more where the scene allows it. */
static constexpr size_t N_DEPTHS                                  = 3;
static const char* const DEPTH_NAME_ARRAY[N_DEPTHS]               = {"Reduced", "Natural", "More"};
static constexpr float   DEPTH_STRENGTH_ARRAY[N_DEPTHS]           = {0.65f, 1.0f, 1.25f};

/* How the pixels of an eye of the headset are brought down, in each direction: half the pixels and a quarter of them. */
static constexpr size_t N_QUALITIES                                  = 3;
static const char* const QUALITY_NAME_ARRAY[N_QUALITIES]             = {"Full (the resolution of the headset)", "Half of the pixels", "A quarter of the pixels"};
static constexpr float   QUALITY_SIZE_FRACTION_ARRAY[N_QUALITIES]    = {1.0f, 0.70710678f, 0.5f};
static constexpr float LAUNCH_BUTTON_WIDTH          = 200.0f;
static constexpr float LIST_MIN_WIDTH               = 280.0f;
static constexpr float LIST_WIDTH_FRACTION          = 0.38f;
static constexpr float SMALL_GAP                    = 6.0f;
static constexpr float STATUS_N_LINES               = 3.0f;

/* Sizes of the text, relative to the size of the body text. */
static constexpr float CARD_TITLE_SCALE         = 1.25f;
static constexpr float DETAILS_TITLE_SCALE      = 1.7f;
static constexpr float PAGE_TITLE_SCALE         = 2.0f;
static constexpr float SECTION_TITLE_SCALE      = 1.2f;

VR::VRPlayer::VrPlayerWindow::VrPlayerWindow(const WindowContent& in_content)
    :ImGuiWindowHost       (get_window_options() ),
     m_content             (in_content),
     m_depth_index         (DEFAULT_DEPTH_INDEX),
     m_depth_strength      (DEPTH_STRENGTH_ARRAY[DEFAULT_DEPTH_INDEX]),
     m_device_index        (-1),
     m_downloader          (false), /* in_is_http_allowed */
     m_installer           (&m_downloader),
     m_installing_app      (-1),
     m_is_status_error     (false),
     m_is_styled           (false),
     m_pace_to_headset     (true),
     m_overlay_app         (-1),
     m_push_overlay        (false),
     m_write_log           (in_content.is_logging),
     m_quality_index       (0),
     m_selected_app        (-1),
     m_should_open_install_error(false)
{
    select_defaults();
}

bool VR::VRPlayer::VrPlayerWindow::draw_app_card(const size_t& in_n_app)
{
    const VR::AppCatalog::AppEntry& app              = m_content.catalog.app_vec.at(in_n_app);
    const ImGuiStyle&               style            = ImGui::GetStyle();
    const float                     pad              = style.FramePadding.x * 1.6f;
    const bool                      is_available     = app.unavailable_reason.empty();
    const bool                      is_selected      = (static_cast<int>(in_n_app) == m_selected_app);
    const float                     width            = ImGui::GetContentRegionAvail().x;
    const float                     wrap_width       = width - 2.0f * pad;
    float                           height           = 2.0f * pad;
    ImVec2                          reason_size      = ImVec2(0.0f,
                                                          0.0f);
    ImVec2                          description_size = ImVec2(0.0f,
                                                              0.0f);
    float                           title_height     = 0.0f;
    bool                            result           = false;

    ImGui::PushFont(nullptr,
                    style.FontSizeBase * CARD_TITLE_SCALE);

    title_height = ImGui::GetTextLineHeight();

    ImGui::PopFont();

    height += title_height;

    if (app.description.empty() == false)
    {
        description_size = ImGui::CalcTextSize(app.description.c_str(),
                                               nullptr,
                                               false,
                                               wrap_width);
        height          += style.ItemSpacing.y + description_size.y;
    }

    if (is_available == false)
    {
        reason_size = ImGui::CalcTextSize(app.unavailable_reason.c_str(),
                                          nullptr,
                                          false,
                                          wrap_width);
        height     += style.ItemSpacing.y + reason_size.y;
    }

    ImGui::PushID(static_cast<int>(in_n_app) );

    {
        const ImVec2 min = ImGui::GetCursorScreenPos();

        result = ImGui::Selectable("##card",
                                   is_selected,
                                   0, /* flags */
                                   ImVec2(width,
                                          height) );

        if (is_selected)
        {
            const ImU32 accent_color = ImGui::GetColorU32(ACCENT_COLOR);

            ImGui::GetWindowDrawList()->AddRectFilled(min,
                                                      ImVec2(min.x + CARD_ACCENT_BAR_WIDTH * style.FontScaleDpi,
                                                             min.y + height),
                                                      accent_color);
        }

        ImGui::SetCursorScreenPos(ImVec2(min.x + pad,
                                         min.y + pad) );

        ImGui::PushFont(nullptr,
                        style.FontSizeBase * CARD_TITLE_SCALE);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              (is_available) ? TEXT_COLOR
                                             : DIM_TEXT_COLOR);
        ImGui::TextUnformatted(app.name.c_str() );
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(min.x + pad,
                                         ImGui::GetCursorScreenPos().y) );
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);

        if (app.description.empty() == false)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  DIM_TEXT_COLOR);
            ImGui::TextUnformatted(app.description.c_str() );
            ImGui::PopStyleColor();
        }

        if (is_available == false)
        {
            ImGui::SetCursorScreenPos(ImVec2(min.x + pad,
                                             ImGui::GetCursorScreenPos().y) );
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ERROR_TEXT_COLOR);
            ImGui::TextUnformatted(app.unavailable_reason.c_str() );
            ImGui::PopStyleColor();
        }

        ImGui::PopTextWrapPos();

        ImGui::SetCursorScreenPos(ImVec2(min.x,
                                         min.y + height + style.ItemSpacing.y) );
        ImGui::Dummy(ImVec2(0.0f,
                            0.0f) );
    }

    ImGui::PopID();

    return result;
}

void VR::VRPlayer::VrPlayerWindow::draw_app_list()
{
    if (m_content.catalog_error.empty() == false)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ERROR_TEXT_COLOR);
        ImGui::TextWrapped("The catalog cannot be read: %s",
                           m_content.catalog_error.c_str() );
        ImGui::PopStyleColor();
    }
    else
    if (m_content.catalog.app_vec.empty() )
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              DIM_TEXT_COLOR);
        ImGui::TextWrapped("The catalog lists no applications.");
        ImGui::PopStyleColor();
    }
    else
    {
        for (size_t n_app = 0;
                    n_app < m_content.catalog.app_vec.size();
                  ++n_app)
        {
            if (draw_app_card(n_app) )
            {
                m_selected_app = static_cast<int>(n_app);
            }
        }
    }
}

void VR::VRPlayer::VrPlayerWindow::draw_contents(Utils::UIHost::ContentExtent* inout_extent_ptr)
{
    const ImGuiStyle& style = ImGui::GetStyle();

    (void) inout_extent_ptr;

    update_install();
    update_session();

    if (m_is_styled == false)
    {
        set_up_style();

        m_is_styled = true;
    }

    draw_header();

    {
        const float list_width = std::max<float>(LIST_MIN_WIDTH * style.FontScaleDpi,
                                                 ImGui::GetContentRegionAvail().x * LIST_WIDTH_FRACTION);

        ImGui::BeginChild("Applications",
                          ImVec2(list_width,
                                 0.0f),
                          ImGuiChildFlags_Borders);

        draw_app_list();

        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("Details",
                          ImVec2(0.0f,
                                 0.0f),
                          ImGuiChildFlags_Borders);

        draw_details();

        ImGui::EndChild();
    }

    draw_install_error();
}

void VR::VRPlayer::VrPlayerWindow::draw_details()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    if (m_selected_app < 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              DIM_TEXT_COLOR);
        ImGui::TextWrapped("Select an application on the left.");
        ImGui::PopStyleColor();

        goto end;
    }

    {
        const VR::AppCatalog::AppEntry& app = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) );

        ImGui::PushFont(nullptr,
                        style.FontSizeBase * DETAILS_TITLE_SCALE);
        ImGui::TextUnformatted(app.name.c_str() );
        ImGui::PopFont();

        if (app.unavailable_reason.empty() == false)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ERROR_TEXT_COLOR);
            ImGui::TextWrapped("%s",
                               app.unavailable_reason.c_str() );
            ImGui::PopStyleColor();
        }

        if (app.is_download_needed)
        {
            const std::string size_text = get_size_text(app.install_spec.n_archive_bytes);

            ImGui::PushStyleColor(ImGuiCol_Text,
                                  DIM_TEXT_COLOR);
            ImGui::TextWrapped("It is not on this computer yet: it is downloaded (%s) the first time it is launched.",
                               size_text.c_str() );
            ImGui::PopStyleColor();
        }

        ImGui::TextWrapped("%s",
                           (app.description.empty() ) ? "No description."
                                                      : app.description.c_str() );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        draw_vr_settings();
        draw_launch_area();
    }

end:
    return;
}

void VR::VRPlayer::VrPlayerWindow::draw_device_combo()
{
    constexpr const char* NO_DEVICE_TEXT = "2D preview only";

    const bool  has_devices = (m_content.device_vec.empty() == false);
    std::string preview     = NO_DEVICE_TEXT;

    if (m_device_index >= 0)
    {
        preview = m_content.device_vec.at(static_cast<size_t>(m_device_index) ).name;
    }

    /* With no runtime, or no headset, there is nothing to choose: the picture is shown in the window, and the list says so. */
    ImGui::BeginDisabled(has_devices == false);
    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::BeginCombo("##device",
                          preview.c_str() ))
    {
        for (size_t n_device = 0;
                    n_device < m_content.device_vec.size();
                  ++n_device)
        {
            const Headset::VRDevices::VrDevice& device    = m_content.device_vec.at(n_device);
            const bool                          is_usable = device.unavailable_reason.empty();

            ImGui::PushID(static_cast<int>(n_device) );
            ImGui::BeginDisabled(is_usable == false);

            if (ImGui::Selectable((is_usable) ? device.name.c_str()
                                              : (device.name + "  (unavailable)").c_str(),
                                  static_cast<int>(n_device) == m_device_index) )
            {
                m_device_index = static_cast<int>(n_device);
            }

            ImGui::EndDisabled();

            if (is_usable == false)
            {
                const bool is_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);

                if (is_hovered)
                {
                    ImGui::SetTooltip("%s",
                                      device.unavailable_reason.c_str() );
                }
            }

            ImGui::PopID();
        }

        if (ImGui::Selectable(NO_DEVICE_TEXT,
                              m_device_index < 0) )
        {
            m_device_index = -1;
        }

        ImGui::EndCombo();
    }

    ImGui::EndDisabled();

    ImGui::PushStyleColor(ImGuiCol_Text,
                          DIM_TEXT_COLOR);

    if (has_devices == false)
    {
        ImGui::TextWrapped("No OpenXR runtime or headset was found: the picture is shown in this window only.");
    }
    else
    if (m_device_index >= 0)
    {
        ImGui::Text("Runtime: %s",
                    m_content.device_vec.at(static_cast<size_t>(m_device_index) ).runtime_name.c_str() );
    }
    else
    {
        ImGui::TextWrapped("The picture is shown in this window only.");
    }

    ImGui::PopStyleColor();
}

void VR::VRPlayer::VrPlayerWindow::draw_header()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushFont(nullptr,
                    style.FontSizeBase * PAGE_TITLE_SCALE);
    ImGui::TextUnformatted("DeepScene");
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_Text,
                          DIM_TEXT_COLOR);
    ImGui::TextUnformatted("Play your 2D demos in stereo");
    ImGui::PopStyleColor();

    {
        const ImVec2 min = ImGui::GetCursorScreenPos();

        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x,
                                                         min.y + SMALL_GAP * style.FontScaleDpi),
                                                  ImVec2(min.x + ImGui::GetContentRegionAvail().x,
                                                         min.y + (SMALL_GAP + ACCENT_LINE_HEIGHT) * style.FontScaleDpi),
                                                  ImGui::GetColorU32(ACCENT_COLOR) );

        ImGui::Dummy(ImVec2(0.0f,
                            3.0f * SMALL_GAP * style.FontScaleDpi) );
    }
}

void VR::VRPlayer::VrPlayerWindow::draw_launch_area()
{
    const ImGuiStyle& style         = ImGui::GetStyle();
    const float       button_height = ImGui::GetFrameHeight() * 1.6f;
    const float       button_width  = LAUNCH_BUTTON_WIDTH * style.FontScaleDpi;
    const bool        is_possible   = is_launch_possible();
    const float       status_height = STATUS_N_LINES * ImGui::GetTextLineHeightWithSpacing();
    const float       area_height   = button_height + status_height;
    const float       free_height   = ImGui::GetContentRegionAvail().y;

    /* The button is at the bottom of the panel, in the middle. */
    if (free_height > area_height)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + free_height - area_height);
    }

    const float status_top = ImGui::GetCursorPosY();

    ImGui::PushStyleColor(ImGuiCol_Text,
                          (m_is_status_error) ? ERROR_TEXT_COLOR
                                              : DIM_TEXT_COLOR);
    ImGui::PushTextWrapPos(0.0f);

    if (m_installing_app >= 0)
    {
        const VR::AppInstaller::InstallProgress progress          = m_installer.get_progress();
        const float                             progress_fraction = (progress.n_bytes_total != 0) ? static_cast<float>(static_cast<double>(progress.n_bytes_done) /
                                                                                                                       static_cast<double>(progress.n_bytes_total) )
                                                                                                  : 0.0f;
        const std::string                       progress_text     = (progress.n_bytes_total != 0) ? get_size_text(progress.n_bytes_done) + " of " + get_size_text(progress.n_bytes_total)
                                                                                                  : std::string();

        ImGui::Text("%s: %s",
                    m_content.catalog.app_vec.at(static_cast<size_t>(m_installing_app) ).name.c_str(),
                    progress.message.c_str() );
        ImGui::ProgressBar(progress_fraction,
                           ImVec2(-FLT_MIN,
                                  0.0f),
                           progress_text.c_str() );
    }
    else
    if (m_status.empty() == false)
    {
        ImGui::TextUnformatted(m_status.c_str() );
    }
    else
    if (is_possible == false)
    {
        ImGui::TextUnformatted("Pick an application which is available.");
    }

    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    /* The button does not move with the length of what is said above it. */
    ImGui::SetCursorPosY(status_top + status_height);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 0.5f * std::max<float>(0.0f,
                                                                       ImGui::GetContentRegionAvail().x - button_width) );

    if (m_installing_app >= 0)
    {
        if (ImGui::Button("Cancel",
                          ImVec2(button_width,
                                 button_height) ))
        {
            m_installer.cancel();
        }
    }
    else
    {
        ImGui::BeginDisabled(is_possible == false);

        if (ImGui::Button("Launch",
                          ImVec2(button_width,
                                 button_height) ))
        {
            launch();
        }

        ImGui::EndDisabled();
    }
}

void VR::VRPlayer::VrPlayerWindow::draw_install_error()
{
    constexpr float WIDTH = 460.0f;

    const ImGuiStyle& style = ImGui::GetStyle();

    if (m_should_open_install_error)
    {
        ImGui::OpenPopup("Cannot install the application");

        m_should_open_install_error = false;
    }

    if (ImGui::BeginPopupModal("Cannot install the application",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize) )
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + WIDTH * style.FontScaleDpi);
        ImGui::TextUnformatted(m_install_error.c_str() );
        ImGui::PopTextWrapPos();

        ImGui::Spacing();

        if (ImGui::Button("OK",
                          ImVec2(120.0f * style.FontScaleDpi,
                                 0.0f) ))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void VR::VRPlayer::VrPlayerWindow::draw_vr_settings()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushFont(nullptr,
                    style.FontSizeBase * SECTION_TITLE_SCALE);
    ImGui::TextUnformatted("VR settings");
    ImGui::PopFont();

    ImGui::Spacing();

    if (m_content.device_error.empty() == false)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ERROR_TEXT_COLOR);
        ImGui::TextWrapped("The devices cannot be listed: %s",
                           m_content.device_error.c_str() );
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::TextUnformatted("Device");

        draw_device_combo();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Depth");

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::BeginCombo("##depth",
                          DEPTH_NAME_ARRAY[m_depth_index]) )
    {
        for (int n_depth = 0;
                 n_depth < static_cast<int>(N_DEPTHS);
               ++n_depth)
        {
            if (ImGui::Selectable(DEPTH_NAME_ARRAY[n_depth],
                                  n_depth == m_depth_index) )
            {
                m_depth_index    = n_depth;
                m_depth_strength = DEPTH_STRENGTH_ARRAY[n_depth];
            }
        }

        ImGui::EndCombo();
    }

    ImGui::PushStyleColor(ImGuiCol_Text,
                          DIM_TEXT_COLOR);
    ImGui::TextWrapped("Natural suits each scene. Reduced is flatter; More is deeper, within what is comfortable. Can be changed while it plays.");
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::TextUnformatted("Image quality");

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::BeginCombo("##quality",
                          QUALITY_NAME_ARRAY[m_quality_index]) )
    {
        for (int n_quality = 0;
                 n_quality < static_cast<int>(N_QUALITIES);
               ++n_quality)
        {
            if (ImGui::Selectable(QUALITY_NAME_ARRAY[n_quality],
                                  n_quality == m_quality_index) )
            {
                m_quality_index = n_quality;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::PushStyleColor(ImGuiCol_Text,
                          DIM_TEXT_COLOR);
    ImGui::TextWrapped("Full is the resolution of the headset; lower is easier on the graphics card. Set at start; headset only.");
    ImGui::PopStyleColor();

    /* The overlay is pushed for an application whose recipe says an amount, and not for one that does not; no control for it. */
    {
        const float overlay_parallax = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) ).overlay_parallax;

        if (m_overlay_app != m_selected_app)
        {
            m_overlay_app  = m_selected_app;
            m_push_overlay = (overlay_parallax > 0.0f);
        }
    }
}

std::string VR::VRPlayer::VrPlayerWindow::get_size_text(const uint64_t& in_n_bytes)
{
    constexpr double N_BYTES_PER_KB = 1024.0;
    constexpr double N_BYTES_PER_MB = 1024.0 * 1024.0;

    char size_text_i8vec32[32];

    if (in_n_bytes >= static_cast<uint64_t>(N_BYTES_PER_MB) )
    {
        ::snprintf(size_text_i8vec32,
                   sizeof(size_text_i8vec32),
                   "%.1f MB",
                   static_cast<double>(in_n_bytes) / N_BYTES_PER_MB);
    }
    else
    {
        ::snprintf(size_text_i8vec32,
                   sizeof(size_text_i8vec32),
                   "%.0f KB",
                   static_cast<double>(in_n_bytes) / N_BYTES_PER_KB);
    }

    return size_text_i8vec32;
}

Utils::UIHost::WindowOptions VR::VRPlayer::VrPlayerWindow::get_window_options()
{
    constexpr int CLIENT_HEIGHT = 620;
    constexpr int CLIENT_WIDTH  = 980;

    Utils::UIHost::WindowOptions result;

    result.class_name    = L"VRPlayerWindow";
    result.client_height = CLIENT_HEIGHT;
    result.client_width  = CLIENT_WIDTH;
    result.is_resizable       = true;
    result.title              = "DeepScene";
    result.uses_system_font   = true;

    return result;
}

bool VR::VRPlayer::VrPlayerWindow::is_launch_possible() const
{
    /* A device is not needed: with none, the picture is shown in the window. */
    return (m_selected_app >= 0                                                                                          &&
            m_installing_app < 0                                                                                         &&
            m_session.get_state() != SessionState::RUNNING                                                               &&
            m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) ).unavailable_reason.empty() );
}

void VR::VRPlayer::VrPlayerWindow::launch()
{
    if (m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) ).is_download_needed)
    {
        start_install();
    }
    else
    {
        start_app(static_cast<size_t>(m_selected_app) );
    }
}

void VR::VRPlayer::VrPlayerWindow::start_app(const size_t& in_n_app)
{
    const VR::AppCatalog::AppEntry& app           = m_content.catalog.app_vec.at(in_n_app);
    std::string                     error;
    uint32_t                        target_height = 0;
    uint32_t                        target_width  = 0;

    /* An eye of the headset, brought down as the quality says; none without a headset. */
    if (m_device_index >= 0)
    {
        const Headset::VRDevices::VrDevice& device   = m_content.device_vec.at(static_cast<size_t>(m_device_index) );
        const float                         fraction = QUALITY_SIZE_FRACTION_ARRAY[m_quality_index];

        target_height = static_cast<uint32_t>(static_cast<float>(device.eye_height) * fraction + 0.5f);
        target_width  = static_cast<uint32_t>(static_cast<float>(device.eye_width)  * fraction + 0.5f);
    }

    m_is_status_error = (m_session.start(app,
                                         m_content.launcher_directory,
                                         target_width,
                                         target_height,
                                        m_device_index >= 0, /* in_use_headset: a device which was chosen is the headset; none is the window alone */
                                        m_pace_to_headset,
                                        m_write_log,
                                        &error) == false);

    if (m_is_status_error)
    {
        m_status = error;
    }
    else
    {
        m_session.set_depth_strength(m_depth_strength);

        m_running_app_name = app.name;
        m_status           = app.name + " is running.";
    }
}

void VR::VRPlayer::VrPlayerWindow::select_defaults()
{
    /* The first application which can be played, or else the first one: its reason is what is shown. */
    for (size_t n_app = 0;
                n_app < m_content.catalog.app_vec.size();
              ++n_app)
    {
        if (m_selected_app < 0 ||
            (m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) ).unavailable_reason.empty() == false &&
             m_content.catalog.app_vec.at(n_app).unavailable_reason.empty() ) )
        {
            m_selected_app = static_cast<int>(n_app);
        }
    }

    for (size_t n_device = 0;
                n_device < m_content.device_vec.size();
              ++n_device)
    {
        if (m_device_index < 0 &&
            m_content.device_vec.at(n_device).unavailable_reason.empty() )
        {
            m_device_index = static_cast<int>(n_device);
        }
    }
}

void VR::VRPlayer::VrPlayerWindow::start_install()
{
    const VR::AppCatalog::AppEntry& app   = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) );
    std::string                     error;

    if (m_installer.start(app.install_spec,
                         &error) )
    {
        m_installing_app  = m_selected_app;
        m_is_status_error = false;
        m_status.clear();
    }
    else
    {
        m_install_error             = error;
        m_is_status_error           = true;
        m_should_open_install_error = true;
        m_status                    = "The application cannot be installed.";
    }
}

void VR::VRPlayer::VrPlayerWindow::update_install()
{
    if (m_installing_app >= 0)
    {
        const VR::AppInstaller::InstallProgress progress = m_installer.get_progress();
        const size_t                            n_app    = static_cast<size_t>(m_installing_app);

        switch (progress.phase)
        {
            case VR::AppInstaller::InstallPhase::DONE:
            {
                const bool is_program_there = Utils::Files::FileSystem::is_file(m_content.catalog.app_vec.at(n_app).executable_path);

                m_installing_app = -1;

                if (is_program_there)
                {
                    m_content.catalog.app_vec.at(n_app).is_download_needed = false;

                    start_app(n_app);
                }
                else
                {
                    /* What a virus scanner does with a packed demo, which is what this one is. */
                    m_install_error             = "The application was unpacked, but its program is not there: " + m_content.catalog.app_vec.at(n_app).executable_path +
                                                  "\n\nA virus scanner may have removed it. Look in its quarantine, allow the file, and launch the application again.";
                    m_is_status_error           = true;
                    m_should_open_install_error = true;
                    m_status                    = "The application cannot be installed.";
                }

                break;
            }

            case VR::AppInstaller::InstallPhase::CANCELLED:
            {
                m_installing_app  = -1;
                m_is_status_error = false;
                m_status          = "The download was cancelled.";

                break;
            }

            case VR::AppInstaller::InstallPhase::FAILED:
            {
                m_install_error             = progress.message;
                m_installing_app            = -1;
                m_is_status_error           = true;
                m_should_open_install_error = true;
                m_status                    = "The application cannot be installed.";

                break;
            }

            default:
            {
                /* Still going: the progress is drawn. */
                break;
            }
        }
    }
}

void VR::VRPlayer::VrPlayerWindow::update_session()
{
    const SessionState previous_state = m_session.get_state();

    m_session.update();

    /* The depth, and the overlay, can be changed while the application plays. */
    m_session.set_depth_strength(m_depth_strength);
    m_session.set_is_overlay_pushed(m_push_overlay && m_overlay_app >= 0 && m_overlay_app == m_selected_app);

    if (previous_state == SessionState::RUNNING &&
        m_session.get_state() != previous_state)
    {
        m_is_status_error = (m_session.get_state() == SessionState::FAILED);
        m_status          = (m_is_status_error) ? m_session.get_message()
                                                : m_running_app_name + " has ended.";
    }
}

void VR::VRPlayer::VrPlayerWindow::set_up_style()
{
    constexpr float FRAME_ROUNDING   = 6.0f;
    constexpr float PANEL_ROUNDING   = 10.0f;
    constexpr float WINDOW_PADDING   = 16.0f;

    ImGuiStyle& style = ImGui::GetStyle();
    const float scale = style.FontScaleDpi;

    style.ChildRounding  = PANEL_ROUNDING * scale;
    style.FrameRounding  = FRAME_ROUNDING * scale;
    style.GrabRounding   = FRAME_ROUNDING * scale;
    style.PopupRounding  = FRAME_ROUNDING * scale;
    style.WindowPadding  = ImVec2(WINDOW_PADDING * scale,
                                  WINDOW_PADDING * scale);

    style.Colors[ImGuiCol_Border]         = BORDER_COLOR;
    style.Colors[ImGuiCol_Button]         = ACCENT_COLOR;
    style.Colors[ImGuiCol_ButtonActive]   = ACCENT_ACTIVE_COLOR;
    style.Colors[ImGuiCol_ButtonHovered]  = ACCENT_HOVERED_COLOR;
    style.Colors[ImGuiCol_ChildBg]        = PANEL_COLOR;
    style.Colors[ImGuiCol_FrameBg]        = FRAME_COLOR;
    style.Colors[ImGuiCol_FrameBgActive]  = FRAME_HOVERED_COLOR;
    style.Colors[ImGuiCol_FrameBgHovered] = FRAME_HOVERED_COLOR;
    style.Colors[ImGuiCol_Header]         = SELECTED_COLOR;
    style.Colors[ImGuiCol_HeaderActive]   = SELECTED_HOVERED_COLOR;
    style.Colors[ImGuiCol_HeaderHovered]  = SELECTED_HOVERED_COLOR;
    style.Colors[ImGuiCol_PopupBg]        = PANEL_COLOR;
    style.Colors[ImGuiCol_SliderGrab]     = ACCENT_HOVERED_COLOR;
    style.Colors[ImGuiCol_SliderGrabActive] = ACCENT_COLOR;
    style.Colors[ImGuiCol_Text]           = TEXT_COLOR;
    style.Colors[ImGuiCol_WindowBg]       = WINDOW_COLOR;
}
