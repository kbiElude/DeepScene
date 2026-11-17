/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef VRPLAYER_VR_PLAYER_WINDOW_H
#define VRPLAYER_VR_PLAYER_WINDOW_H

#include "vr_player/app_installer/app_installer.h"
#include "utils/files/http_downloader.h"
#include "utils/ui/imgui_player_window_base.h"
#include "app_session.h"
#include "types.h"
#include <string>

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
        void draw_details      ();
        void draw_device_combo ();
        void draw_header       ();
        void draw_install_error();
        void draw_launch_area  ();
        void draw_vr_settings  ();
        bool is_launch_possible() const;
        void launch            ();
        void select_defaults   ();
        void set_up_style      ();
        void start_app         (const size_t& in_n_app);
        void start_install     ();
        void update_install    ();
        void update_session    ();

        static std::string              get_size_text     (const uint64_t& in_n_bytes);
        static Utils::UI::WindowOptions get_window_options();

        WindowContent                        m_content;
        int                                  m_depth_index;      /* index into the depth choices */
        float                                m_depth_strength;   /* strength of that choice: 1 is the natural depth the converter works out for each scene */
        int                                  m_device_index;     /* index into the devices; -1 if no device can be used */
        Utils::Files::HttpDownloader         m_downloader;
        std::string                          m_install_error;    /* text of the install error window                       */
        VRPlayer::AppInstaller::AppInstaller m_installer;
        int                                  m_installing_app;   /* index into the catalog; -1 if none is being installed  */
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
    };
}

#endif /* VRPLAYER_VR_PLAYER_WINDOW_H */
