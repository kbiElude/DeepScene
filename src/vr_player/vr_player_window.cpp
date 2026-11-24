/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "vr_player_window.h"
#include "stereo/core/resolution_policy.h"
#include "utils/files/directory_operations.h"
#include "imgui.h"
#include <algorithm>
#include <float.h>
#include <stdio.h>

/* The look of the window: one accent color on dark panels. */
static const ImVec4 ACCENT_COLOR          (0.33f,
                                           0.42f,
                                           0.95f,
                                           1.00f);
static const ImVec4 ACCENT_ACTIVE_COLOR   (0.27f,
                                           0.35f,
                                           0.85f,
                                           1.00f);
static const ImVec4 ACCENT_HOVERED_COLOR  (0.42f,
                                           0.50f,
                                           1.00f,
                                           1.00f);
static const ImVec4 ACCENT_VIOLET_COLOR   (0.62f,
                                           0.38f,
                                           0.95f,
                                           1.00f);
static const ImVec4 BORDER_COLOR          (0.20f,
                                           0.22f,
                                           0.28f,
                                           1.00f);
static const ImVec4 CHIP_BORDER_COLOR     (0.33f,
                                           0.42f,
                                           0.95f,
                                           0.55f);
static const ImVec4 CHIP_FILL_COLOR       (0.33f,
                                           0.42f,
                                           0.95f,
                                           0.16f);
static const ImVec4 DIM_TEXT_COLOR        (0.58f,
                                           0.61f,
                                           0.70f,
                                           1.00f);
static const ImVec4 ERROR_TEXT_COLOR      (1.00f,
                                           0.45f,
                                           0.45f,
                                           1.00f);
static const ImVec4 FRAME_COLOR           (0.15f,
                                           0.165f,
                                           0.20f,
                                           1.00f);
static const ImVec4 FRAME_HOVERED_COLOR   (0.19f,
                                           0.21f,
                                           0.26f,
                                           1.00f);
static const ImVec4 PANEL_COLOR           (0.105f,
                                           0.115f,
                                           0.145f,
                                           1.00f);
static const ImVec4 SELECTED_COLOR        (0.20f,
                                           0.25f,
                                           0.50f,
                                           0.55f);
static const ImVec4 SELECTED_HOVERED_COLOR(0.22f,
                                           0.28f,
                                           0.55f,
                                           0.45f);
static const ImVec4 SHADOW_COLOR          (0.00f,
                                           0.00f,
                                           0.00f,
                                           0.35f);
static const ImVec4 TEXT_COLOR            (0.92f,
                                           0.93f,
                                           0.96f,
                                           1.00f);
static const ImVec4 WINDOW_COLOR          (0.07f,
                                           0.075f,
                                           0.095f,
                                           1.00f);

/* Sizes at 100% scale; the monitor scale is applied on top. */
static constexpr float ACCENT_LINE_HEIGHT    = 3.0f;
static constexpr float CARD_ACCENT_BAR_WIDTH = 3.0f;
static constexpr float CARD_ROUNDING         = 8.0f;
static constexpr float CARD_SHADOW_OFFSET    = 2.0f;
static constexpr float CARD_WASH_ALPHA       = 0.30f;
static constexpr float CHIP_GAP              = 6.0f;
static constexpr float CHIP_PADDING_X        = 10.0f;
static constexpr float CHIP_PADDING_Y        = 3.0f;
static constexpr int   DEFAULT_DEPTH_INDEX   = 1;
static constexpr float GLOW_ALPHA            = 0.07f;
static constexpr int   GLOW_N_LAYERS         = 4;
static constexpr float GLOW_SPREAD           = 2.0f;
static constexpr float SHINE_ALPHA           = 0.22f;

/* The depth strengths the user can choose: less than the converter's natural depth for each scene, the same, and somewhat more where the scene allows it. */
static constexpr size_t  N_DEPTHS                       = 3;
static const char* const DEPTH_NAME_ARRAY[N_DEPTHS]     = {"Reduced", "Natural", "More"};
static constexpr float   DEPTH_STRENGTH_ARRAY[N_DEPTHS] = {0.65f, 1.0f, 1.25f};


/* The size per eye. The first choices are fractions of the headset eye size in each direction (0.707 is half the pixels, 0.5 a quarter); then fixed sizes; the last is typed in. */
static constexpr size_t  N_HEADSET_RESOLUTIONS                         = 3;
static constexpr size_t  N_RESOLUTIONS                                 = 8;
static constexpr int     DEFAULT_CUSTOM_HEIGHT                         = 1080;
static constexpr int     DEFAULT_CUSTOM_WIDTH                          = 1920;
static constexpr float   HEADSET_FRACTION_ARRAY[N_HEADSET_RESOLUTIONS] = {1.0f, 0.70710678f, 0.5f};
static constexpr int     MAX_CUSTOM_EXTENT                             = 4096;
static constexpr int     RESOLUTION_CUSTOM_INDEX                       = 7;
static constexpr int     RESOLUTION_HEIGHT_ARRAY[N_RESOLUTIONS]        = {0, 0, 0, 720, 1080, 1440, 2160, 0};
static const char* const RESOLUTION_NAME_ARRAY[N_RESOLUTIONS]          = {"Headset: full resolution", "Headset: half of the pixels", "Headset: a quarter of the pixels", "1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160", "Custom"};
static constexpr int     RESOLUTION_PREVIEW_DEFAULT_INDEX              = 4; /* 1920 x 1080: the choice when there is no headset */
static constexpr int     RESOLUTION_WIDTH_ARRAY[N_RESOLUTIONS]         = {0, 0, 0, 1280, 1920, 2560, 3840, 0};

static constexpr float LAUNCH_BUTTON_WIDTH = 200.0f;
static constexpr float LIST_MIN_WIDTH      = 280.0f;
static constexpr float LIST_WIDTH_FRACTION = 0.38f;
static constexpr float SMALL_GAP           = 6.0f;
static constexpr float STATUS_N_LINES      = 3.0f;

/* Sizes of the text, relative to the size of the body text. */
static constexpr float CARD_TITLE_SCALE    = 1.25f;
static constexpr float DETAILS_TITLE_SCALE = 1.7f;
static constexpr float PAGE_TITLE_SCALE    = 2.0f;
static constexpr float SECTION_TITLE_SCALE = 1.2f;

VRPlayer::VrPlayerWindow::VrPlayerWindow(const WindowContent& in_content)
    :ImGuiPlayerWindowBase      (get_window_options() ),
     m_content                  (in_content),
     m_depth_index              (DEFAULT_DEPTH_INDEX),
     m_depth_strength           (DEPTH_STRENGTH_ARRAY[DEFAULT_DEPTH_INDEX]),
     m_device_index             (-1),
     m_downloader               (false), /* in_is_http_allowed */
     m_installer                (&m_downloader),
     m_installing_app           (-1),
     m_is_status_error          (false),
     m_is_styled                (false),
     m_pace_to_headset          (true),
     m_overlay_app              (-1),
     m_push_overlay             (false),
     m_write_log                (in_content.is_logging),
     m_resolution_custom_height (DEFAULT_CUSTOM_HEIGHT),
     m_resolution_custom_width  (DEFAULT_CUSTOM_WIDTH),
     m_resolution_index         (0),
     m_selected_app             (-1),
     m_should_open_install_error(false)
{
    select_defaults();
}

bool VRPlayer::VrPlayerWindow::draw_app_card(const size_t& in_n_app)
{
    const VRPlayer::AppCatalog::AppEntry& app              = m_content.catalog.app_vec.at(in_n_app);
    const ImGuiStyle&                     style            = ImGui::GetStyle();
    const float                           pad              = style.FramePadding.x * 1.6f;
    const bool                            is_available     = app.unavailable_reason.empty();
    const bool                            is_selected      = (static_cast<int>(in_n_app) == m_selected_app);
    const float                           width            = ImGui::GetContentRegionAvail().x;
    const float                           wrap_width       = width - 2.0f * pad;
    float                                 height           = 2.0f * pad;
    ImVec2                                reason_size      = ImVec2(0.0f,
                                                                    0.0f);
    ImVec2                                description_size = ImVec2(0.0f,
                                                                    0.0f);
    float                                 title_height     = 0.0f;
    bool                                  result           = false;

    ImGui::PushFont(nullptr,
                    style.FontSizeBase * CARD_TITLE_SCALE);

    /* A title which is longer than the card wraps to a second line. */
    title_height = ImGui::CalcTextSize(app.name.c_str(),
                                       nullptr,
                                       false, /* hide_text_after_double_hash */
                                       wrap_width).y;

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

        /* The card is drawn here, so the selectable has no color of its own. */
        ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(0.0f, 0.0f, 0.0f, 0.0f) );
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ImVec4(0.0f, 0.0f, 0.0f, 0.0f) );
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(0.0f, 0.0f, 0.0f, 0.0f) );

        result = ImGui::Selectable("##card",
                                   is_selected,
                                   0, /* flags */
                                   ImVec2(width,
                                          height) );

        ImGui::PopStyleColor(3); /* count */

        {
            const ImVec2 max        = ImVec2(min.x + width,
                                             min.y + height);
            const float  rounding   = CARD_ROUNDING * style.FontScaleDpi;
            const float  shadow_dy  = CARD_SHADOW_OFFSET * style.FontScaleDpi;
            const bool   is_hovered = ImGui::IsItemHovered();
            ImDrawList*  draw_list  = ImGui::GetWindowDrawList();

            draw_list->AddRectFilled(ImVec2(min.x,
                                            min.y + shadow_dy),
                                     ImVec2(max.x,
                                            max.y + shadow_dy),
                                     ImGui::GetColorU32(SHADOW_COLOR),
                                     rounding);
            draw_list->AddRectFilled(min,
                                     max,
                                     ImGui::GetColorU32((is_hovered) ? FRAME_HOVERED_COLOR
                                                                     : FRAME_COLOR),
                                     rounding);

            if (is_selected)
            {
                /* A wash of the accent, strongest at the bar and fading to the right; the strip at the left follows the rounded corners. */
                const ImU32 wash_color  = ImGui::GetColorU32(ImVec4(ACCENT_COLOR.x,
                                                                    ACCENT_COLOR.y,
                                                                    ACCENT_COLOR.z,
                                                                    CARD_WASH_ALPHA) );
                const ImU32 clear_color = ImGui::GetColorU32(ImVec4(ACCENT_COLOR.x,
                                                                    ACCENT_COLOR.y,
                                                                    ACCENT_COLOR.z,
                                                                    0.0f) );

                draw_list->AddRectFilled(min,
                                         ImVec2(min.x + 2.0f * rounding,
                                                max.y),
                                         wash_color,
                                         rounding,
                                         ImDrawFlags_RoundCornersLeft);
                draw_list->AddRectFilledMultiColor(ImVec2(min.x + 2.0f * rounding,
                                                          min.y),
                                                   max,
                                                   wash_color,
                                                   clear_color,
                                                   clear_color,
                                                   wash_color);
            }

            if (is_selected ||
                is_hovered)
            {
                /* The bar stays inside the straight part of the left edge. */
                draw_list->AddRectFilled(ImVec2(min.x,
                                                min.y + rounding),
                                         ImVec2(min.x + CARD_ACCENT_BAR_WIDTH * style.FontScaleDpi,
                                                max.y - rounding),
                                         ImGui::GetColorU32((is_selected) ? ACCENT_COLOR
                                                                          : ImVec4(ACCENT_COLOR.x,
                                                                                   ACCENT_COLOR.y,
                                                                                   ACCENT_COLOR.z,
                                                                                   0.45f) ) );
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(min.x + pad,
                                         min.y + pad) );

        ImGui::PushFont      (nullptr,
                              style.FontSizeBase * CARD_TITLE_SCALE);
        ImGui::PushStyleColor(ImGuiCol_Text,
                              (is_available) ? TEXT_COLOR
                                             : DIM_TEXT_COLOR);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
        ImGui::TextUnformatted(app.name.c_str() );
        ImGui::PopTextWrapPos ();
        ImGui::PopStyleColor  ();
        ImGui::PopFont        ();

        ImGui::SetCursorScreenPos(ImVec2(min.x + pad,
                                         ImGui::GetCursorScreenPos().y) );
        ImGui::PushTextWrapPos   (ImGui::GetCursorPosX() + wrap_width);

        if (app.description.empty() == false)
        {
            ImGui::PushStyleColor (ImGuiCol_Text,
                                   DIM_TEXT_COLOR);
            ImGui::TextUnformatted(app.description.c_str() );
            ImGui::PopStyleColor  ();
        }

        if (is_available == false)
        {
            ImGui::SetCursorScreenPos(ImVec2(min.x + pad,
                                             ImGui::GetCursorScreenPos().y) );
            ImGui::PushStyleColor    (ImGuiCol_Text,
                                      ERROR_TEXT_COLOR);
            ImGui::TextUnformatted   (app.unavailable_reason.c_str() );
            ImGui::PopStyleColor     ();
        }

        ImGui::PopTextWrapPos();

        ImGui::SetCursorScreenPos(ImVec2(min.x,
                                         min.y + height + style.ItemSpacing.y) );
        ImGui::Dummy             (ImVec2(0.0f,
                                         0.0f) );
    }

    ImGui::PopID();

    return result;
}

void VRPlayer::VrPlayerWindow::draw_app_list()
{
    if (m_content.catalog_error.empty() == false)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ERROR_TEXT_COLOR);
        ImGui::TextWrapped   ("Catalog: %s",
                              m_content.catalog_error.c_str() );
        ImGui::PopStyleColor ();
    }
    else
    if (m_content.catalog.app_vec.empty() )
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              DIM_TEXT_COLOR);
        ImGui::TextWrapped    ("No applications.");
        ImGui::PopStyleColor  ();
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

void VRPlayer::VrPlayerWindow::draw_button_glow(const float& in_height,
                                                const float& in_width)
{
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2      min   = ImGui::GetCursorScreenPos();

    /* Layers of the accent around the button, the widest first, so that the light is dense at the button and fades out. */
    for (int n_layer = GLOW_N_LAYERS;
             n_layer > 0;
           --n_layer)
    {
        const float spread = static_cast<float>(n_layer) * GLOW_SPREAD * style.FontScaleDpi;

        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x - spread,
                                                         min.y - spread),
                                                  ImVec2(min.x + in_width  + spread,
                                                         min.y + in_height + spread),
                                                  ImGui::GetColorU32(ImVec4(ACCENT_COLOR.x,
                                                                            ACCENT_COLOR.y,
                                                                            ACCENT_COLOR.z,
                                                                            GLOW_ALPHA) ),
                                                  style.FrameRounding + spread);
    }
}

void VRPlayer::VrPlayerWindow::draw_button_shine()
{
    /* The top half of the button is lighter. The strip is inset by the rounding, so it stays inside the corners. */
    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2      min   = ImGui::GetItemRectMin();
    const ImVec2      max   = ImGui::GetItemRectMax();
    const ImU32       lit   = ImGui::GetColorU32(ImVec4(1.0f,
                                                        1.0f,
                                                        1.0f,
                                                        SHINE_ALPHA) );
    const ImU32       clear = ImGui::GetColorU32(ImVec4(1.0f,
                                                        1.0f,
                                                        1.0f,
                                                        0.0f) );

    ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(min.x + style.FrameRounding,
                                                               min.y),
                                                        ImVec2(max.x - style.FrameRounding,
                                                               min.y + 0.5f * (max.y - min.y) ),
                                                        lit,
                                                        lit,
                                                        clear,
                                                        clear);
}

void VRPlayer::VrPlayerWindow::draw_chips()
{
    /* The choices that the app is played with, as rounded labels that wrap onto further rows. */
    const ImGuiStyle&        style      = ImGui::GetStyle();
    const float              gap        = CHIP_GAP       * style.FontScaleDpi;
    const float              height     = ImGui::GetTextLineHeight() + 2.0f * CHIP_PADDING_Y * style.FontScaleDpi;
    const float              padding_x  = CHIP_PADDING_X * style.FontScaleDpi;
    const ImVec2             origin     = ImGui::GetCursorScreenPos();
    const float              right      = origin.x + ImGui::GetContentRegionAvail().x;
    ImDrawList*              draw_list  = ImGui::GetWindowDrawList();
    std::vector<std::string> text_vec;
    float                    x          = origin.x;
    float                    y          = origin.y;

    text_vec.push_back((m_device_index >= 0) ? m_content.device_vec.at(static_cast<size_t>(m_device_index) ).name
                                             : std::string("2D preview") );
    text_vec.push_back(DEPTH_NAME_ARRAY[m_depth_index]);

    if (m_resolution_index == RESOLUTION_CUSTOM_INDEX)
    {
        text_vec.push_back(std::to_string(m_resolution_custom_width)  +
                           " x "                                      +
                           std::to_string(m_resolution_custom_height) );
    }
    else
    {
        text_vec.push_back(RESOLUTION_NAME_ARRAY[m_resolution_index]);
    }

    for (const std::string& text : text_vec)
    {
        const float width = ImGui::CalcTextSize(text.c_str() ).x + 2.0f * padding_x;

        if (x + width > right &&
            x > origin.x)
        {
            x  = origin.x;
            y += height + gap;
        }

        draw_list->AddRectFilled(ImVec2(x,
                                        y),
                                 ImVec2(x + width,
                                        y + height),
                                 ImGui::GetColorU32(CHIP_FILL_COLOR),
                                 0.5f * height);
        draw_list->AddRect      (ImVec2(x,
                                        y),
                                 ImVec2(x + width,
                                        y + height),
                                 ImGui::GetColorU32(CHIP_BORDER_COLOR),
                                 0.5f * height);
        draw_list->AddText      (ImVec2(x + padding_x,
                                        y + CHIP_PADDING_Y * style.FontScaleDpi),
                                 ImGui::GetColorU32(TEXT_COLOR),
                                 text.c_str() );

        x += width + gap;
    }

    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy             (ImVec2(0.0f,
                                     y + height - origin.y) );
}

void VRPlayer::VrPlayerWindow::draw_contents(Utils::UI::ContentExtent* inout_extent_ptr)
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

void VRPlayer::VrPlayerWindow::draw_details()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    if (m_selected_app < 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              DIM_TEXT_COLOR);
        ImGui::TextWrapped   ("Select an app.");
        ImGui::PopStyleColor ();

        goto end;
    }

    {
        const VRPlayer::AppCatalog::AppEntry& app = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) );

        ImGui::PushFont       (nullptr,
                               style.FontSizeBase * DETAILS_TITLE_SCALE);
        ImGui::TextUnformatted(app.name.c_str() );
        ImGui::PopFont        ();

        if (app.unavailable_reason.empty() == false)
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ERROR_TEXT_COLOR);
            ImGui::TextWrapped   ("%s",
                                  app.unavailable_reason.c_str() );
            ImGui::PopStyleColor ();
        }

        if (app.description.empty() == false)
        {
            ImGui::TextWrapped("%s",
                               app.description.c_str() );
        }

        ImGui::Spacing();
        draw_chips    ();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        draw_vr_settings();
        draw_launch_area();
    }

end:
    return;
}

void VRPlayer::VrPlayerWindow::draw_device_combo()
{
    constexpr const char* NO_DEVICE_TEXT = "2D preview only";

    const bool  has_devices = (m_content.device_vec.empty() == false);
    std::string preview     = NO_DEVICE_TEXT;

    if (m_device_index >= 0)
    {
        preview = m_content.device_vec.at(static_cast<size_t>(m_device_index) ).name;
    }

    /* With no runtime or no headset there is nothing to choose: the picture is shown in the window, and the list says so. */
    ImGui::BeginDisabled   (has_devices == false);
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

            ImGui::PushID       (static_cast<int>(n_device) );
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
        ImGui::TextWrapped("No headset found.");
    }
    else
    if (m_device_index >= 0)
    {
        ImGui::Text("Runtime: %s",
                    m_content.device_vec.at(static_cast<size_t>(m_device_index) ).runtime_name.c_str() );
    }

    ImGui::PopStyleColor();
}

void VRPlayer::VrPlayerWindow::draw_header()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushFont       (nullptr,
                           style.FontSizeBase * PAGE_TITLE_SCALE);
    ImGui::TextUnformatted("DeepScene");
    ImGui::PopFont        ();

    ImGui::PushStyleColor (ImGuiCol_Text,
                           DIM_TEXT_COLOR);
    ImGui::TextUnformatted("Play your 2D demos in stereo");
    ImGui::PopStyleColor  ();

    {
        /* The line runs from the accent to a violet, and fades out toward the right. */
        const ImU32  accent_color = ImGui::GetColorU32(ACCENT_COLOR);
        const ImU32  clear_color  = ImGui::GetColorU32(ImVec4(ACCENT_VIOLET_COLOR.x,
                                                              ACCENT_VIOLET_COLOR.y,
                                                              ACCENT_VIOLET_COLOR.z,
                                                              0.0f) );
        const ImU32  violet_color = ImGui::GetColorU32(ACCENT_VIOLET_COLOR);
        const ImVec2 min          = ImGui::GetCursorScreenPos();
        const float  bottom       = min.y + (SMALL_GAP + ACCENT_LINE_HEIGHT) * style.FontScaleDpi;
        const float  middle       = min.x + 0.5f * ImGui::GetContentRegionAvail().x;
        const float  right        = min.x +        ImGui::GetContentRegionAvail().x;
        const float  top          = min.y + SMALL_GAP * style.FontScaleDpi;

        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(min.x,
                                                                   top),
                                                            ImVec2(middle,
                                                                   bottom),
                                                            accent_color,
                                                            violet_color,
                                                            violet_color,
                                                            accent_color);
        ImGui::GetWindowDrawList()->AddRectFilledMultiColor(ImVec2(middle,
                                                                   top),
                                                            ImVec2(right,
                                                                   bottom),
                                                            violet_color,
                                                            clear_color,
                                                            clear_color,
                                                            violet_color);

        ImGui::Dummy(ImVec2(0.0f,
                            3.0f * SMALL_GAP * style.FontScaleDpi) );
    }
}

void VRPlayer::VrPlayerWindow::draw_install_error()
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
        ImGui::PopTextWrapPos ();

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

void VRPlayer::VrPlayerWindow::draw_launch_area()
{
    const ImGuiStyle& style         = ImGui::GetStyle      ();
    const float       button_height = ImGui::GetFrameHeight() * 1.6f;
    const float       button_width  = LAUNCH_BUTTON_WIDTH     * style.FontScaleDpi;
    const bool        is_possible   = is_launch_possible();
    const bool        is_download   = (m_selected_app >= 0 &&
                                       m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) ).is_download_needed);
    const bool        is_error_text = (m_installing_app < 0   &&
                                       m_status.empty() == false &&
                                       m_is_status_error);
    const float       line_height   = ImGui::GetTextLineHeightWithSpacing();
    const float       status_height = STATUS_N_LINES * line_height;

    /* The error area is as tall as its text, which wraps in a table cell, plus a blank line after it. */
    const float       error_width   = ImGui::GetContentRegionAvail().x - 2.0f * style.CellPadding.x;
    const float       error_height  = (is_error_text) ? ImGui::CalcTextSize(m_status.c_str(),
                                                                            nullptr,
                                                                            false, /* hide_text_after_double_hash */
                                                                            error_width).y + 2.0f * style.CellPadding.y + line_height
                                                      : 0.0f;
    const float       area_height   = button_height + std::max<float>(status_height,
                                                                      error_height);
    const float       free_height   = ImGui::GetContentRegionAvail().y;

    /* The button sits at the bottom of the panel, centered. */
    if (free_height > area_height)
    {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + free_height - area_height);
    }

    const float status_top = ImGui::GetCursorPosY();

    if (is_error_text)
    {
        /* The error is in a table and the button follows it on a new line, so nothing covers the text however long it is. */
        if (ImGui::BeginTable("##status_error",
                              1, /* columns */
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg) )
        {
            ImGui::TableNextRow   ();
            ImGui::TableNextColumn();

            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ERROR_TEXT_COLOR);
            ImGui::TextWrapped   ("%s",
                                  m_status.c_str() );
            ImGui::PopStyleColor ();

            ImGui::EndTable();
        }

        ImGui::NewLine();
    }

    if (is_error_text == false)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              (m_is_status_error) ? ERROR_TEXT_COLOR
                                                  : DIM_TEXT_COLOR);
        ImGui::PushTextWrapPos(0.0f);
    }

    if (is_error_text)
    {
        /* The error text is already drawn above. */
    }
    else
    if (m_installing_app >= 0)
    {
        const VRPlayer::AppInstaller::InstallProgress progress          = m_installer.get_progress();
        const float                                   progress_fraction = (progress.n_bytes_total != 0) ? static_cast<float>(static_cast<double>(progress.n_bytes_done) /
                                                                                                                             static_cast<double>(progress.n_bytes_total) )
                                                                                                        : 0.0f;
        const std::string                             progress_text     = (progress.n_bytes_total != 0) ? get_size_text(progress.n_bytes_done) + " of " + get_size_text(progress.n_bytes_total)
                                                                                                        : std::string();

        ImGui::Text       ("%s: %s",
                           m_content.catalog.app_vec.at(static_cast<size_t>(m_installing_app) ).name.c_str(),
                           progress.message.c_str() );
        ImGui::ProgressBar(progress_fraction,
                           ImVec2(-FLT_MIN,
                                  0.0f),
                           progress_text.c_str() );
    }
    else
    if (is_possible == false)
    {
        ImGui::TextUnformatted("Pick an app.");
    }

    if (is_error_text == false)
    {
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor ();

        /* The button keeps its position however long the text above it is. */
        ImGui::SetCursorPosY(status_top + status_height);
    }

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
        if (is_possible)
        {
            draw_button_glow(button_height,
                             button_width);
        }

        ImGui::BeginDisabled(is_possible == false);

        if (ImGui::Button((is_download) ? "Download & launch"
                                        : "Launch",
                          ImVec2(button_width,
                                 button_height) ))
        {
            launch();
        }

        if (is_possible)
        {
            draw_button_shine();
        }

        ImGui::EndDisabled();
    }
}

void VRPlayer::VrPlayerWindow::draw_vr_settings()
{
    const ImGuiStyle& style = ImGui::GetStyle();

    ImGui::PushFont       (nullptr,
                           style.FontSizeBase * SECTION_TITLE_SCALE);
    ImGui::TextUnformatted("VR settings");
    ImGui::PopFont        ();

    ImGui::Spacing();

    if (m_content.device_error.empty() == false)
    {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ERROR_TEXT_COLOR);
        ImGui::TextWrapped   ("Devices: %s",
                              m_content.device_error.c_str() );
        ImGui::PopStyleColor ();
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
    ImGui::TextWrapped   ("Natural suits each scene. Reduced is flatter; More is deeper.");
    ImGui::PopStyleColor ();

    ImGui::Spacing        ();
    ImGui::TextUnformatted ("Resolution per eye");

    /* Without a headset there is no size to take a fraction of. */
    if (m_device_index < 0 &&
        m_resolution_index < static_cast<int>(N_HEADSET_RESOLUTIONS) )
    {
        m_resolution_index = RESOLUTION_PREVIEW_DEFAULT_INDEX;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);

    if (ImGui::BeginCombo("##resolution",
                          RESOLUTION_NAME_ARRAY[m_resolution_index]) )
    {
        for (int n_resolution = 0;
                 n_resolution < static_cast<int>(N_RESOLUTIONS);
               ++n_resolution)
        {
            const bool is_headset_choice = (n_resolution < static_cast<int>(N_HEADSET_RESOLUTIONS) );

            if (ImGui::Selectable(RESOLUTION_NAME_ARRAY[n_resolution],
                                  n_resolution == m_resolution_index,
                                  (is_headset_choice && m_device_index < 0) ? ImGuiSelectableFlags_Disabled
                                                                            : ImGuiSelectableFlags_None) )
            {
                m_resolution_index = n_resolution;
            }
        }

        ImGui::EndCombo();
    }

    if (m_resolution_index == RESOLUTION_CUSTOM_INDEX)
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("width##custom",
                       &m_resolution_custom_width,
                        0);
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("height##custom",
                       &m_resolution_custom_height,
                        0);

        m_resolution_custom_height = (std::max)(1,
                                                (std::min)(m_resolution_custom_height,
                                                           MAX_CUSTOM_EXTENT) );
        m_resolution_custom_width  = (std::max)(1,
                                                (std::min)(m_resolution_custom_width,
                                                           MAX_CUSTOM_EXTENT) );
    }

    ImGui::PushStyleColor(ImGuiCol_Text,
                          DIM_TEXT_COLOR);
    ImGui::TextWrapped   ("Lower is lighter on the graphics card.");
    ImGui::PopStyleColor ();

    /* The overlay is pushed for an application whose recipe gives an amount, and not for one whose recipe does not. There is no control for it. */
    {
        const float overlay_parallax = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) ).overlay_parallax;

        if (m_overlay_app != m_selected_app)
        {
            m_overlay_app  = m_selected_app;
            m_push_overlay = (overlay_parallax > 0.0f);
        }
    }
}

std::string VRPlayer::VrPlayerWindow::get_size_text(const uint64_t& in_n_bytes)
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

Utils::UI::WindowOptions VRPlayer::VrPlayerWindow::get_window_options()
{
    constexpr int CLIENT_HEIGHT = 620;
    constexpr int CLIENT_WIDTH  = 980;

    Utils::UI::WindowOptions result;

    result.class_name       = L"VRPlayerWindow";
    result.client_height    = CLIENT_HEIGHT;
    result.client_width     = CLIENT_WIDTH;
    result.is_resizable     = true;
    result.title            = "DeepScene";
    result.uses_system_font = true;

    return result;
}

bool VRPlayer::VrPlayerWindow::is_launch_possible() const
{
    bool result = false;

    /* A device is not required: without one, the picture is shown in the window. */
    if (m_selected_app   >= 0 &&
        m_installing_app <  0)
    {
        const SessionState state = m_session.get_state();

        if (state != SessionState::RUNNING)
        {
            const auto& selected_app = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) );

            result = selected_app.unavailable_reason.empty();
        }
    }

    return result;
}

void VRPlayer::VrPlayerWindow::launch()
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

void VRPlayer::VrPlayerWindow::select_defaults()
{
    /* Select the first application that can be played, or else the first one, so that its unavailable reason is shown. */
    for (size_t n_app = 0;
                n_app < m_content.catalog.app_vec.size();
              ++n_app)
    {
        const auto& candidate_app           = m_content.catalog.app_vec.at          (n_app);
        const bool  is_candidate_available  = candidate_app.unavailable_reason.empty();
        bool        is_selected_unavailable = false;

        if (m_selected_app >= 0)
        {
            const auto& selected_app = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) );

            is_selected_unavailable = (selected_app.unavailable_reason.empty() == false);
        }

        if (m_selected_app           < 0 ||
            (is_selected_unavailable     &&
             is_candidate_available) )
        {
            m_selected_app = static_cast<int>(n_app);
        }
    }

    for (size_t n_device = 0;
                n_device < m_content.device_vec.size();
              ++n_device)
    {
        const auto& device              = m_content.device_vec.at        (n_device);
        const bool  is_device_available = device.unavailable_reason.empty();

        if (m_device_index < 0 &&
            is_device_available)
        {
            m_device_index = static_cast<int>(n_device);
        }
    }
}

void VRPlayer::VrPlayerWindow::set_up_style()
{
    constexpr float FRAME_ROUNDING   = 8.0f;
    constexpr float PANEL_ROUNDING   = 12.0f;
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

void VRPlayer::VrPlayerWindow::start_app(const size_t& in_n_app)
{
    const VRPlayer::AppCatalog::AppEntry& app           = m_content.catalog.app_vec.at(in_n_app);
    std::string                           error;
    uint32_t                              target_height = 0;
    uint32_t                              target_width  = 0;

    /* The size asked for, or the size of an eye of the headset scaled by the fraction (0 without a headset). */
    if (m_resolution_index >= static_cast<int>(N_HEADSET_RESOLUTIONS) )
    {
        const bool is_custom = (m_resolution_index == RESOLUTION_CUSTOM_INDEX);

        target_height = static_cast<uint32_t>((is_custom) ? m_resolution_custom_height
                                                          : RESOLUTION_HEIGHT_ARRAY[m_resolution_index]);
        target_width  = static_cast<uint32_t>((is_custom) ? m_resolution_custom_width
                                                          : RESOLUTION_WIDTH_ARRAY[m_resolution_index]);
    }
    else
    if (m_device_index >= 0)
    {
        const Headset::VRDevices::VrDevice& device   = m_content.device_vec.at(static_cast<size_t>(m_device_index) );
        const float                         fraction = HEADSET_FRACTION_ARRAY[m_resolution_index];

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

        m_status.clear();
    }
}

void VRPlayer::VrPlayerWindow::start_install()
{
    const VRPlayer::AppCatalog::AppEntry& app   = m_content.catalog.app_vec.at(static_cast<size_t>(m_selected_app) );
    std::string                           error;

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
        m_status                    = "Install failed.";
    }
}

void VRPlayer::VrPlayerWindow::update_install()
{
    if (m_installing_app >= 0)
    {
        const VRPlayer::AppInstaller::InstallProgress progress = m_installer.get_progress();
        const size_t                                  n_app    = static_cast<size_t>(m_installing_app);

        switch (progress.phase)
        {
            case VRPlayer::AppInstaller::InstallPhase::DONE:
            {
                const bool is_program_there = Utils::Files::DirectoryOperations::is_file(m_content.catalog.app_vec.at(n_app).executable_path);

                m_installing_app = -1;

                if (is_program_there)
                {
                    m_content.catalog.app_vec.at(n_app).is_download_needed = false;

                    start_app(n_app);
                }
                else
                {
                    /* A virus scanner often removes a packed demo, and this application is one. */
                    m_install_error             = "The application was unpacked, but its program is not there: "                                                      +
                                                  m_content.catalog.app_vec.at(n_app).executable_path                                                                 +
                                                  "\n\nA virus scanner may have removed it. Look in its quarantine, allow the file, and launch the application again.";
                    m_is_status_error           = true;
                    m_should_open_install_error = true;
                    m_status                    = "Install failed.";
                }

                break;
            }

            case VRPlayer::AppInstaller::InstallPhase::CANCELLED:
            {
                m_installing_app  = -1;
                m_is_status_error = false;

                m_status.clear();

                break;
            }

            case VRPlayer::AppInstaller::InstallPhase::FAILED:
            {
                m_install_error             = progress.message;
                m_installing_app            = -1;
                m_is_status_error           = true;
                m_should_open_install_error = true;
                m_status                    = "Install failed.";

                break;
            }

            default:
            {
                /* The installation is still running. Its progress is drawn elsewhere. */
                break;
            }
        }
    }
}

void VRPlayer::VrPlayerWindow::update_session()
{
    const SessionState previous_state = m_session.get_state();

    m_session.update();

    /* The depth strength and the overlay setting can be changed while the application plays. */
    m_session.set_depth_strength   (m_depth_strength);
    m_session.set_is_overlay_pushed(m_push_overlay                   &&
                                    m_overlay_app >= 0               &&
                                    m_overlay_app == m_selected_app);

    if (previous_state == SessionState::RUNNING)
    {
        const SessionState current_state = m_session.get_state();

        if (current_state != previous_state)
        {
            /* Only a failure is said. */
            m_is_status_error = (current_state == SessionState::FAILED);

            if (m_is_status_error)
            {
                m_status = m_session.get_message();
            }
            else
            {
                m_status.clear();
            }
        }
    }
}
