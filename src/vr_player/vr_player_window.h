/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_VR_PLAYER_WINDOW_H
#define VRPLAYER_VR_PLAYER_WINDOW_H

#include "vr_player/app_installer/app_installer.h"
#include "vr_player/app_thumbnails/thumbnail_fetcher.h"
#include "utils/files/http_downloader.h"
#include "utils/ui/imgui_player_window_base.h"
#include "app_session.h"
#include "thumbnail_textures.h"
#include "types.h"
#include <string>
#include <vector>

namespace VRPlayer
{
    /* The window of the VR player: the applications that can be played in VR, the device to play them on, and the button that starts the
     * application with the converter in it. The depth strength and the overlay setting are passed on while the application plays. The device
     * and the quality are chosen before it starts.
     */
    class VrPlayerWindow : public Utils::UI::ImGuiPlayerWindowBase
    {
    public:
        explicit VrPlayerWindow(const WindowContent& in_content);

    protected:
        void draw_contents(Utils::UI::ContentExtent* inout_extent_ptr) override;

    private:
        bool draw_app_card     (const size_t& in_n_app);
        void draw_app_list     ();
        void draw_button_glow  (const float&  in_height,
                                const float&  in_width);
        void draw_button_shine ();
        void draw_details      ();
        void draw_device_combo ();
        void draw_header       ();
        void draw_install_error();
        void draw_launch_area  ();
        void draw_progress_bar (const float&       in_fraction,
                                const std::string& in_text);
        void draw_thumbnail_credit();
        void draw_vr_settings  ();
        void draw_window_buttons();
        void fit_width_to_titles();
        bool is_launch_possible() const;
        void launch            ();
        void select_defaults   ();
        void set_up_style      ();
        void start_app         (const size_t& in_n_app);
        void start_install     ();
        void update_install    ();
        void update_session    ();
        void update_thumbnails ();
        void update_transitions();

        static std::string              get_size_text     (const uint64_t& in_n_bytes);
        static Utils::UI::WindowOptions get_window_options();

        std::vector<float>                   m_card_highlight_vec; /* per card, 0 to 1: how far its bar and wash have grown toward the state it is in (selected, hovered, none) */
        WindowContent                        m_content;
        int                                  m_depth_index;     /* index into the depth choices */
        float                                m_depth_strength;   /* strength of that choice: 1 is the natural depth the converter works out for each scene */
        float                                m_details_fade;     /* 0 to 1: the details of the selected app fade in when the selection changes */
        int                                  m_details_fade_app; /* the selection the fade is for                          */
        int                                  m_device_index;     /* index into the devices; -1 if no device can be used */
        Utils::Files::HttpDownloader         m_downloader;
        std::string                          m_install_error;    /* text of the install error window                       */
        VRPlayer::AppInstaller::AppInstaller m_installer;
        int                                  m_installing_app;   /* index into the catalog; -1 if none is being installed  */
        bool                                 m_is_sized;         /* the width of the window has been set to fit the titles */
        bool                                 m_is_status_error;
        bool                                 m_is_styled;
        bool                                 m_pace_to_headset;  /* no control in the window: always on */
        int                                  m_overlay_app;      /* the application m_push_overlay was set for; -1 if none */
        bool                                 m_push_overlay;     /* the draws that the recipe leaves alone are pushed toward the viewer */
        bool                                 m_write_log;        /* no control in the window: off unless the program was started with --log */
        int                                  m_resolution_custom_height; /* the size per eye of the custom choice */
        int                                  m_resolution_custom_width;
        int                                  m_resolution_index; /* index into the resolution choices; the first ones are fractions of the headset size */
        std::string                          m_running_app_name;
        int                                  m_selected_app;    /* index into the catalog; -1 if the catalog is empty    */
        AppSession                           m_session;
        bool                                 m_should_open_install_error;
        std::string                          m_status;
        Utils::Files::HttpDownloader         m_thumbnail_downloader; /* the fetcher's own: a downloader is not shared between threads */
        std::vector<float>                   m_thumbnail_fade_vec;   /* per card, 0 to 1: how far its picture has faded in           */
        VRPlayer::AppThumbnails::ThumbnailFetcher m_thumbnail_fetcher;
        uint64_t                             m_thumbnail_n_seen;     /* the fetcher's count at the last look for pictures; ~0 before the first */
        ThumbnailTextures                    m_thumbnail_textures;
    };
}

#endif /* VRPLAYER_VR_PLAYER_WINDOW_H */
